/**
 ******************************************************************************
 * @file    game_fsm.c
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Logica del juego Beat Clash — FSM, generacion y deteccion de notas.
 *
 * FSM:
 *   SPLASH → MENU_NIVEL → CONTEO → JUGANDO → RESULTADO → SPLASH
 *
 * Notas:
 *   - Se generan aleatoriamente para cada jugador independientemente
 *   - Se mueven de derecha a izquierda a nota_speed px por tick
 *   - Hit window: nota llega a la zona de presion [x_rel <= PRESS_ZONE_W]
 *   - Puntaje segun precision: PERFECTO/BUENO/OK/FALLO
 *
 * Nivel 1: notas simples, 1 nota max en pantalla por jugador
 * Nivel 2: hasta 2 notas simultaneas, velocidad mayor
 * Nivel 3: hasta 2 notas, velocidad maxima, intervalo menor
 *
 * Aqui puedo ajustar dificultad, numero de notas y logica de spawn.
 ******************************************************************************
 */

#include "game_fsm.h"
#include "renderer.h"
#include "audio.h"
#include "ili9341.h"
#include "input.h"
#include "board_pins.h"
#include "stm32f4xx_hal.h"
#include <string.h>

/* Forward declaration */
static uint16_t note_y_raw(uint8_t carril);

/* ========================================================================== */
/* === GENERADOR ALEATORIO (LCG) ============================================ */
/* ========================================================================== */

static uint32_t rng_state = 1664525UL;

void Game_SeedRandom(uint32_t seed) {
    rng_state = seed ? seed : 12345UL;
}

static uint32_t rng_next(void) {
    rng_state = rng_state * 1664525UL + 1013904223UL;
    return rng_state;
}

static uint8_t rng_lane(void) {
    return (uint8_t)(rng_next() & 3);
}

/* ========================================================================== */
/* === GESTION DE NOTAS ===================================================== */
/* ========================================================================== */

/* Encuentra una ranura libre en el pool de notas */
static Nota_t *alloc_nota(GameState_t *gs) {
    for (uint8_t i = 0; i < MAX_NOTES; i++) {
        if (!gs->notas[i].activa) return &gs->notas[i];
    }
    return NULL;
}

/* Cuenta notas activas por jugador */
static uint8_t count_notas(GameState_t *gs, uint8_t jugador) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < MAX_NOTES; i++) {
        if (gs->notas[i].activa && gs->notas[i].jugador == jugador) n++;
    }
    return n;
}

/* Maximo de notas simultaneas por nivel */
static uint8_t max_simultaneous(uint8_t nivel) {
    return (nivel >= 2) ? 2 : 1;
}

/* Spawn de una nota para el jugador dado */
static void spawn_nota(GameState_t *gs, uint8_t jugador) {
    if (gs->j[jugador].notas_spawneadas >= NOTES_PER_GAME) return;
    if (count_notas(gs, jugador) >= max_simultaneous(gs->nivel)) return;

    Nota_t *n = alloc_nota(gs);
    if (!n) return;

    n->activa   = 1;
    n->golpeada = 0;
    n->erased   = 0;
    n->jugador  = jugador;
    n->carril   = rng_lane();
    n->x_rel    = (int16_t)PLAYER_W;       /* spawn fuera de la pantalla derecha */
    n->x_prev   = n->x_rel;

    gs->j[jugador].notas_spawneadas++;
    gs->j[jugador].tick_ultimo_spawn = HAL_GetTick();
}

/* ========================================================================== */
/* === DETECCION DE GOLPE =================================================== */
/* ========================================================================== */

/* Evalua la presion de botones del jugador contra las notas activas         */
static void check_hits(GameState_t *gs, uint8_t jugador, uint32_t now) {
    EstadoJugador_t *j = &gs->j[jugador];
    if (!j->press) return;  /* ninguna presion nueva esta iteracion          */

    for (uint8_t b = 0; b < 4; b++) {
        if (!(j->press & (1 << b))) continue;  /* boton b no fue presionado */

        /* Buscar nota activa del carril b en zona de presion */
        Nota_t *best   = NULL;
        int16_t best_d = 9999;

        for (uint8_t i = 0; i < MAX_NOTES; i++) {
            Nota_t *n = &gs->notas[i];
            if (!n->activa || n->golpeada || n->jugador != jugador) continue;
            if (n->carril != b) continue;

            /* Verificar si la nota esta en la ventana de golpe               */
            /* Hit window: x_rel <= PRESS_ZONE_W y borde derecho >= -NOTE_W  */
            if (n->x_rel > (int16_t)PRESS_ZONE_W) continue;
            if (n->x_rel + (int16_t)NOTE_W < 0)   continue;

            /* Distancia del centro de la nota al centro de la press zone     */
            int16_t nc = n->x_rel + (int16_t)(NOTE_W / 2);
            int16_t pc = (int16_t)(PRESS_ZONE_W / 2);
            int16_t d  = nc - pc;
            if (d < 0) d = -d;
            if (d < best_d) { best_d = d; best = n; }
        }

        if (best) {
            /* HIT! Determinar precision */
            uint16_t pts;
            uint32_t freq;
            if (best_d <= (int16_t)HIT_PERFECT) {
                pts = SCORE_PERFECT; freq = AUDIO_NOTE_HIT_PERFECT;
            } else if (best_d <= (int16_t)HIT_GOOD) {
                pts = SCORE_GOOD;    freq = AUDIO_NOTE_HIT_GOOD;
            } else {
                pts = SCORE_OK;      freq = AUDIO_NOTE_HIT_OK;
            }

            j->puntaje += pts;
            if (j->puntaje > SCORE_MAX) j->puntaje = SCORE_MAX;
            j->combo++;

            /* Sonido de golpe */
            Audio_PlayTone(jugador, freq, 80);

            /* Flash LED del carril */
            LED_Set(jugador, b, 1);
            j->led_flash_tick[b] = now;
            j->led_on[b]         = 1;

            /* Flash visual zona presion */
            Renderer_FlashPressZone(jugador, b, NOTE_COLOR[b]);

            best->golpeada = 1;
            best->activa   = 0;  /* quitarla de la pantalla en el siguiente tick */

            /* Actualizar score en pantalla */
            Renderer_UpdateScores(gs);

        } else {
            /* Presion sin nota en zona — fallo (no penaliza, solo sonido) */
            j->combo = 0;
            Audio_PlayTone(jugador, AUDIO_NOTE_MISS, 150);
            /* Flash rojo en el carril presionado */
            Renderer_FlashPressZone(jugador, b, COLOR_DARKGRAY);
        }
    }
}

/* ========================================================================== */
/* === ACTUALIZACION DE NOTAS =============================================== */
/* ========================================================================== */

static void update_notas(GameState_t *gs, uint32_t now) {
    for (uint8_t i = 0; i < MAX_NOTES; i++) {
        Nota_t *n = &gs->notas[i];
        if (!n->activa) continue;

        n->x_prev = n->x_rel;
        n->x_rel -= (int16_t)gs->nota_speed;

        /* Nota que paso la zona de presion sin golpe — FALLO */
        if (n->x_rel + (int16_t)NOTE_W < 0 && !n->golpeada) {
            Audio_PlayTone(n->jugador, AUDIO_NOTE_MISS, 200);
            gs->j[n->jugador].combo = 0;
            /* Flash breve en zona presion */
            Renderer_FlashPressZone(n->jugador, n->carril, COLOR_DARKGRAY);
            n->activa = 0;
        }

        /* Nota golpeada — borrar de pantalla en la siguiente pasada         */
        if (n->golpeada && !n->erased) {
            n->erased = 1;
            n->activa = 0;
            /* Repintar el area de la nota con el fondo */
            uint16_t xo = (n->jugador == 0) ? P1_X_OFF : P2_X_OFF;
            int16_t abs_x = (int16_t)xo + n->x_prev;
            if (abs_x < 0) abs_x = 0;
            int16_t w = n->x_prev + NOTE_W - (abs_x - (int16_t)xo);
            if (w > (int16_t)PLAYER_W) w = (int16_t)PLAYER_W;
            if (w > 0) {
                ILI9341_FillRect((uint16_t)abs_x, note_y_raw(n->carril),
                                 (uint16_t)w, NOTE_H,
                                 n->x_prev < (int16_t)PRESS_ZONE_W
                                     ? PRESS_COLOR[n->carril]
                                     : LANE_COLOR[n->carril]);
            }
        }
    }

    /* Apagar LEDs flash despues de 80ms */
    for (uint8_t j = 0; j < 2; j++) {
        for (uint8_t b = 0; b < 4; b++) {
            if (gs->j[j].led_on[b] &&
                (now - gs->j[j].led_flash_tick[b] >= 80)) {
                gs->j[j].led_on[b] = 0;
                LED_Set(j, b, 0);
                /* Restaurar fondo de press zone */
                Renderer_FlashPressZone(j, b, PRESS_COLOR[b]);
            }
        }
    }
}

/* Helper: y de nota (necesario aqui sin incluir renderer privado)            */
static uint16_t note_y_raw(uint8_t carril) {
    return (uint16_t)(LANE_TOP(carril) + NOTE_Y_PAD);
}

/* ========================================================================== */
/* === FSM — ESTADOS ======================================================== */
/* ========================================================================== */

static void cambiar_estado(GameState_t *gs, EstadoApp_t nuevo, uint32_t now) {
    gs->estado_prev  = gs->estado;
    gs->estado       = nuevo;
    gs->tick_estado  = now;
    gs->pantalla_init = 0;
}

static void fsm_splash(GameState_t *gs, uint32_t now) {
    /* Detectar flanco en START (activo BAJO) */
    if (gs->start_btn && !gs->start_btn_prev) {
        Audio_PlayTone(0, AUDIO_NOTE_BEEP_HIGH, 100);
        cambiar_estado(gs, ESTADO_MENU_NIVEL, now);
    }
}

static void fsm_menu(GameState_t *gs, uint32_t now) {
    /* Navegar con joystick J1 Y o botones J1 */
    static uint32_t nav_tick = 0;
    if (now - nav_tick < 250) return;  /* debounce de navegacion             */

    /* Joystick J1 Y: arriba/abajo para navegar */
    if (gs->j[0].joy_y < JOY_THRESH_LO) {
        if (gs->menu_cursor > 0) { gs->menu_cursor--; gs->pantalla_init = 0; }
        nav_tick = now;
    } else if (gs->j[0].joy_y > JOY_THRESH_HI) {
        if (gs->menu_cursor < 2) { gs->menu_cursor++; gs->pantalla_init = 0; }
        nav_tick = now;
    }

    /* J1 boton Rojo o START para confirmar nivel */
    if ((gs->j[0].press & 1) || (gs->start_btn && !gs->start_btn_prev)) {
        gs->nivel = gs->menu_cursor + 1;
        switch (gs->nivel) {
            case 1: gs->nota_speed = NOTE_SPEED_L1; gs->spawn_intervalo = SPAWN_INTERVAL_L1; break;
            case 2: gs->nota_speed = NOTE_SPEED_L2; gs->spawn_intervalo = SPAWN_INTERVAL_L2; break;
            case 3: gs->nota_speed = NOTE_SPEED_L3; gs->spawn_intervalo = SPAWN_INTERVAL_L3; break;
        }
        Audio_PlayTone(0, AUDIO_NOTE_BEEP_HIGH, 100);
        cambiar_estado(gs, ESTADO_CONTEO, now);
        gs->conteo_num = 3;
        gs->tick_conteo = now;
        Renderer_DrawConteo(3);
    }
}

static void fsm_conteo(GameState_t *gs, uint32_t now) {
    if (now - gs->tick_conteo >= 1000) {
        gs->tick_conteo = now;

        if (gs->conteo_num > 0) {
            gs->conteo_num--;
            uint32_t freq = (gs->conteo_num > 0)
                            ? AUDIO_NOTE_BEEP_HIGH : AUDIO_NOTE_GO;
            Audio_PlayTone(0, freq, 300);
            Audio_PlayTone(1, freq, 300);
            Renderer_DrawConteo(gs->conteo_num);
        }

        if (gs->conteo_num == 0 && (now - gs->tick_conteo >= 800)) {
            /* Ir a JUGANDO despues de mostrar GO por 0.8s */
            /* Reiniciar estado de jugadores */
            for (uint8_t j = 0; j < 2; j++) {
                gs->j[j].puntaje             = 0;
                gs->j[j].combo               = 0;
                gs->j[j].notas_spawneadas    = 0;
                gs->j[j].tick_ultimo_spawn   = now;
                memset(gs->j[j].led_on,       0, sizeof(gs->j[j].led_on));
                memset(gs->j[j].led_flash_tick, 0, sizeof(gs->j[j].led_flash_tick));
            }
            memset(gs->notas, 0, sizeof(gs->notas));
            /* Apagar todos los LEDs */
            HAL_GPIO_WritePin(J1_LED_PORT, J1_LED_ALL_PINS, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(J2_LED_PORT, J2_LED_ALL_PINS, GPIO_PIN_RESET);
            cambiar_estado(gs, ESTADO_JUGANDO, now);
        }
    }
}

static void fsm_jugando(GameState_t *gs, uint32_t now) {
    /* Spawn de notas para cada jugador */
    for (uint8_t j = 0; j < 2; j++) {
        if (gs->j[j].notas_spawneadas < NOTES_PER_GAME) {
            if (now - gs->j[j].tick_ultimo_spawn >= gs->spawn_intervalo) {
                spawn_nota(gs, j);
            }
        }
    }

    /* Deteccion de golpes */
    check_hits(gs, 0, now);
    check_hits(gs, 1, now);

    /* Mover notas */
    update_notas(gs, now);

    /* Verificar fin de partida */
    uint8_t j0_done = (gs->j[0].notas_spawneadas >= NOTES_PER_GAME);
    uint8_t j1_done = (gs->j[1].notas_spawneadas >= NOTES_PER_GAME);
    if (j0_done && j1_done) {
        /* Verificar que no queden notas activas */
        uint8_t activas = 0;
        for (uint8_t i = 0; i < MAX_NOTES; i++) {
            if (gs->notas[i].activa) { activas = 1; break; }
        }
        if (!activas) {
            Audio_Silence();
            /* Sonido de victoria */
            uint16_t ganador_pts = (gs->j[0].puntaje >= gs->j[1].puntaje)
                                   ? gs->j[0].puntaje : gs->j[1].puntaje;
            (void)ganador_pts;
            Audio_PlayTone(0, AUDIO_NOTE_VICTORY, 500);
            Audio_PlayTone(1, AUDIO_NOTE_VICTORY, 500);
            cambiar_estado(gs, ESTADO_RESULTADO, now);
        }
    }
}

static void fsm_resultado(GameState_t *gs, uint32_t now) {
    /* START para volver al menu */
    if (gs->start_btn && !gs->start_btn_prev) {
        Audio_PlayTone(0, AUDIO_NOTE_BEEP_HIGH, 100);
        cambiar_estado(gs, ESTADO_SPLASH, now);
    }
}

/* ========================================================================== */
/* === ENTRYPOINTS PUBLICOS ================================================= */
/* ========================================================================== */

void Game_Init(GameState_t *gs) {
    memset(gs->notas, 0, sizeof(gs->notas));
    for (uint8_t j = 0; j < 2; j++) {
        gs->j[j].puntaje          = 0;
        gs->j[j].combo            = 0;
        gs->j[j].notas_spawneadas = 0;
        memset(gs->j[j].led_on, 0, sizeof(gs->j[j].led_on));
    }
    gs->nivel          = 1;
    gs->menu_cursor    = 0;
    gs->nota_speed     = NOTE_SPEED_L1;
    gs->spawn_intervalo = SPAWN_INTERVAL_L1;
    gs->conteo_num     = 3;
    gs->tick_logica    = 0;
    gs->tick_render    = 0;
    gs->pantalla_init  = 0;
    gs->start_btn      = 0;
    gs->start_btn_prev = 0;

    cambiar_estado(gs, ESTADO_SPLASH, HAL_GetTick());
}

void Game_Update(GameState_t *gs, uint32_t now) {
    /* Guardar prev del START antes de que Input_Update lo actualice          */
    /* (Input_Update actualiza start_btn; aqui ya fue actualizado en el loop) */

    switch (gs->estado) {
    case ESTADO_SPLASH:      fsm_splash(gs, now);    break;
    case ESTADO_MENU_NIVEL:  fsm_menu(gs, now);      break;
    case ESTADO_CONTEO:      fsm_conteo(gs, now);    break;
    case ESTADO_JUGANDO:     fsm_jugando(gs, now);   break;
    case ESTADO_RESULTADO:   fsm_resultado(gs, now); break;
    }
}
