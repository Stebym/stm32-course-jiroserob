/**
 ******************************************************************************
 * @file    ili9341.c
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Driver ILI9341 — inicializacion y primitivas de dibujo basico
 *          para practicas de pantalla (relleno, figuras, imagenes, texto).
 ******************************************************************************
 */

#include "ili9341.h"      // prototipos propios (ILI9341_Init, FillRect, etc.) y defines de color/MADCTL
#include "board_pins.h"    // pines fisicos del SPI/CS/DC/RST usados por las macros LCD_CS_LOW() etc.

extern SPI_HandleTypeDef hspi1;                    // handle del periferico SPI1, ya inicializado en main.c (MX_SPI1_Init) -- este driver solo lo usa
static uint8_t lcd_row_buf[ILI9341_MAXDIM * 2];     // buffer estatico de UNA fila de pixeles (2 bytes c/u, RGB565), reutilizado en FillRect -- agrandar ILI9341_MAXDIM en el header agranda este buffer en RAM

/* Dimensiones activas segun orientacion — Init() arranca en paisaje 320x240 */
uint16_t ILI9341_W = 320;   // ancho activo en pixeles -- cambia en SetPortrait/SetFlip180 segun la orientacion pedida
uint16_t ILI9341_H = 240;   // alto activo en pixeles

/* ========================================================================== */
/* === FUNCIONES INTERNAS DE BUS ============================================ */
/* ========================================================================== */

/* Envoltorio de HAL_SPI_Transmit con recuperacion automatica de errores: en
 * un montaje de protoboard, un pico de ruido electrico (p. ej. al conmutar
 * los LEDs de los botones a traves de los ULN2003A) puede hacer que una
 * transmision SPI puntual falle o agote su timeout. Sin recuperacion, el HAL
 * deja el periferico en estado ocupado/error y CUALQUIER escritura
 * posterior a la pantalla se descarta silenciosamente (HAL_SPI_Transmit
 * devuelve HAL_BUSY de inmediato sin transmitir nada) -- el resto del
 * sistema (lectura de botones, log por UART) sigue funcionando con
 * normalidad, asi que el sintoma visible es "la pantalla deja de responder
 * para siempre" aunque los botones se seguian presionando y viendo en
 * consola. HAL_SPI_Abort() fuerza al periferico de vuelta a un estado listo
 * para que la proxima escritura pueda volver a intentarse normalmente. */
/* Cuenta fallas de SPI CONSECUTIVAS (se resetea a 0 en cada transmision
 * exitosa) como señal indirecta de que hay ruido electrico activo en el bus
 * ahora mismo -- ver ILI9341_FalloComunicacionDetectado() mas abajo. No
 * detecta el caso en que el ruido cae justo sobre RST y el controlador se
 * resetea SIN que la transmision en curso falle (HAL_SPI_Transmit no tiene
 * forma de saberlo, la pantalla es solo-escritura); pero como ambos casos
 * vienen de la misma fuente de ruido, en la practica suelen ocurrir en la
 * misma rafaga. */
static volatile uint8_t lcd_spi_fallas_seguidas = 0;   // contador de fallas de SPI consecutivas, arranca en 0 (bus sano)
#define LCD_SPI_FALLAS_UMBRAL  3   // fallas seguidas antes de pedir un reinit completo -- evita reaccionar a un unico glitch aislado que ya se recupera solo

static void LCD_SPI_Send(const uint8_t *data, uint16_t len, uint32_t timeout) {   // envoltorio de bajo nivel de HAL_SPI_Transmit con recuperacion de errores (ver comentario de arriba)
    if (HAL_SPI_Transmit(&hspi1, (uint8_t *)data, len, timeout) != HAL_OK) {   // intenta la transmision; si falla o agota el timeout, entra a la rama de recuperacion
        HAL_SPI_Abort(&hspi1);   // descarta la transaccion fallida y regresa hspi1.State a HAL_SPI_STATE_READY
        if (lcd_spi_fallas_seguidas < 0xFF) lcd_spi_fallas_seguidas++;
    } else {
        lcd_spi_fallas_seguidas = 0;   // transmision exitosa -- el bus esta sano por ahora, reinicia el contador
    }
}

/* Ver comentario de lcd_spi_fallas_seguidas arriba. Consumir (llamar desde
 * el loop principal, NO desde dentro de una transmision) y, si devuelve 1,
 * el llamador debe reinicializar la pantalla por completo (ILI9341_Init())
 * y forzar un redibujo total de lo que se este mostrando -- el contenido
 * viejo del GRAM ya no es confiable. */
uint8_t ILI9341_FalloComunicacionDetectado(void) {   // consultada desde el loop principal de main.c, ver comentario de lcd_spi_fallas_seguidas arriba
    if (lcd_spi_fallas_seguidas < LCD_SPI_FALLAS_UMBRAL) return 0;   // todavia no se llego al umbral de fallas seguidas: nada que reportar
    lcd_spi_fallas_seguidas = 0;   // rearma para la proxima rafaga de fallas
    return 1;
}

static void LCD_WriteCmd(uint8_t cmd) {   // envia un comando de 1 byte SIN datos (ej. soft reset, sleep out, display on/off)
    LCD_CS_LOW();                          // baja Chip Select: selecciona el ILI9341 en el bus SPI, empieza la transaccion
    LCD_DC_LOW();                          // Data/Command en modo COMANDO (nivel bajo = el chip interpreta el siguiente byte como comando)
    LCD_SPI_Send(&cmd, 1, 10);             // transmite el byte de comando por SPI1, timeout 10ms
    LCD_CS_HIGH();                         // sube Chip Select: termina la transaccion
}

static void LCD_CmdData(uint8_t cmd, const uint8_t *data, uint8_t n) {   // envia un comando seguido de n bytes de datos/parametros (ej. MADCTL + 1 byte de orientacion)
    LCD_CS_LOW();                          // selecciona el chip
    LCD_DC_LOW();                          // modo comando para el byte de comando
    LCD_SPI_Send(&cmd, 1, 10);             // envia el byte de comando

    LCD_DC_HIGH();                              // pasa a modo DATOS (nivel alto) para los bytes que siguen
    LCD_SPI_Send(data, n, 50);                  // envia los n bytes de datos/parametros del comando, timeout 50ms
    LCD_CS_HIGH();                              // libera el chip, termina la transaccion
}

/* ========================================================================== */
/* === INICIALIZACION ======================================================= */
/* ========================================================================== */

void ILI9341_Init(void) {   // secuencia completa de arranque del controlador -- se llama una sola vez desde main.c
    /* Reset estricto por Hardware */
    LCD_RST_HIGH();     // pone RESET en alto (inactivo), estado de reposo antes del pulso real
    HAL_Delay(5);       // espera 5ms con RESET en alto antes de empezar el pulso
    LCD_RST_LOW();      // baja RESET: dispara el reset fisico del controlador
    HAL_Delay(20);      // mantiene RESET bajo 20ms (minimo del datasheet para un reset valido)
    LCD_RST_HIGH();     // sube RESET de nuevo: termina el pulso de reset
    HAL_Delay(150);     // espera 150ms a que el controlador arranque internamente -- bajar este delay puede dejar los primeros comandos sin efecto porque el chip todavia no responde

    /* Software Reset */
    LCD_WriteCmd(0x01);   // SWRESET (0x01): reset por software del controlador, ademas del reset fisico ya hecho
    HAL_Delay(150);       // espera obligatoria (datasheet) tras SWRESET antes de mandar mas comandos

    /* Display OFF durante la configuracion */
    LCD_WriteCmd(0x28);   // DISPOFF (0x28): apaga la salida a pantalla mientras se configura, evita mostrar basura durante el setup

    /* Power Control A */
    { uint8_t d[] = {0x39,0x2C,0x00,0x34,0x02}; LCD_CmdData(0xCB, d, 5); }   // Power Control A (0xCB): valores de fabrica para las tensiones internas -- no tocar sin el datasheet a mano

    /* Power Control B */
    { uint8_t d[] = {0x00,0xC1,0x30}; LCD_CmdData(0xCF, d, 3); }   // Power Control B (0xCF): idem, valores recomendados por el fabricante

    /* Driver Timing Control A */
    { uint8_t d[] = {0x85,0x00,0x78}; LCD_CmdData(0xE8, d, 3); }   // Driver Timing Control A (0xE8): tiempos internos del driver de fila/columna

    /* Driver Timing Control B */
    { uint8_t d[] = {0x00,0x00}; LCD_CmdData(0xEA, d, 2); }        // Driver Timing Control B (0xEA): idem, valores de fabrica

    /* Power on Sequence Control */
    { uint8_t d[] = {0x64,0x03,0x12,0x81}; LCD_CmdData(0xED, d, 4); }   // Power On Sequence Control (0xED): orden/tiempo de encendido de las etapas de alimentacion internas

    /* Pump Ratio Control */
    { uint8_t d[] = {0x20}; LCD_CmdData(0xF7, d, 1); }   // Pump Ratio Control (0xF7): relacion de la bomba de carga interna (genera el voltaje negativo para el panel)

    /* Power Control 1 — VRH */
    { uint8_t d[] = {0x23}; LCD_CmdData(0xC0, d, 1); }   // Power Control 1 (0xC0): ajusta VRH (nivel GVDD) -- define el rango de contraste/grises; cambiarlo puede aclarar u oscurecer la imagen

    /* Power Control 2 */
    { uint8_t d[] = {0x10}; LCD_CmdData(0xC1, d, 1); }   // Power Control 2 (0xC1): ajusta la corriente de los amplificadores de la fuente de alimentacion del panel

    /* VCOM Control 1 */
    { uint8_t d[] = {0x3E,0x28}; LCD_CmdData(0xC5, d, 2); }   // VCOM Control 1 (0xC5): fija el voltaje VCOM (referencia de los pixeles) -- afecta contraste/uniformidad

    /* VCOM Control 2 */
    { uint8_t d[] = {0x86}; LCD_CmdData(0xC7, d, 1); }   // VCOM Control 2 (0xC7): ajuste fino adicional de VCOM

    /* Memory Access Control (MADCTL) — Modo Paisaje 320x240 BGR */
    { uint8_t d[] = {ILI9341_MADCTL_LANDSCAPE}; LCD_CmdData(0x36, d, 1); }   // MADCTL (0x36): orientacion inicial paisaje 320x240 + orden de color BGR -- mismo registro que tocan despues ILI9341_SetPortrait/SetFlip180

    /* Pixel Format — 16 bits RGB565 */
    { uint8_t d[] = {0x55}; LCD_CmdData(0x3A, d, 1); }   // COLMOD (0x3A): 0x55 = 16 bits/pixel tanto en el bus MCU como en el panel (RGB565) -- formato que usan todas las funciones de dibujo de este driver

    /* Frame Rate Control ~70Hz */
    { uint8_t d[] = {0x00,0x18}; LCD_CmdData(0xB1, d, 2); }   // Frame Rate Control (0xB1): tasa de refresco del panel ~70Hz -- no afecta la velocidad con la que ESTE driver dibuja (eso lo limita el SPI, no este registro)

    /* Display Function Control */
    { uint8_t d[] = {0x08,0x82,0x27}; LCD_CmdData(0xB6, d, 3); }   // Display Function Control (0xB6): direccion de barrido de filas/columnas del panel y numero de lineas activas

    /* Gamma Enable */
    { uint8_t d[] = {0x00}; LCD_CmdData(0xF2, d, 1); }   // 3-Gamma Control (0xF2): deshabilita la correccion gamma de 3 puntos (se usan las tablas completas de abajo)
    { uint8_t d[] = {0x01}; LCD_CmdData(0x26, d, 1); }   // Gamma Set (0x26): selecciona la curva gamma 1 (la definida por las tablas Positive/Negative Gamma siguientes)

    /* Positive Gamma */
    { uint8_t d[] = {0x0F,0x31,0x2B,0x0C,0x0E,0x08,0x4E,0xF1,
                     0x37,0x07,0x10,0x03,0x0E,0x09,0x00};
      LCD_CmdData(0xE0, d, 15); }   // Positive Gamma Correction (0xE0): 15 puntos de la curva gamma (rama positiva) -- calibracion de fabrica del panel, tocarlos cambia contraste/tono de color, no el layout

    /* Negative Gamma */
    { uint8_t d[] = {0x00,0x0E,0x14,0x03,0x11,0x07,0x31,0xC1,
                     0x48,0x08,0x0F,0x0C,0x31,0x36,0x0F};
      LCD_CmdData(0xE1, d, 15); }   // Negative Gamma Correction (0xE1): 15 puntos de la curva gamma (rama negativa), complementa la tabla anterior

    /* Sleep Out */
    LCD_WriteCmd(0x11);   // SLPOUT (0x11): saca al panel del modo sleep (arranca la generacion interna de imagen)
    HAL_Delay(120);       // espera obligatoria de 120ms tras SLPOUT (datasheet) antes de encender la salida

    /* Display ON */
    LCD_WriteCmd(0x29);   // DISPON (0x29): enciende la salida a pantalla, ya con toda la config anterior aplicada
    HAL_Delay(20);        // pequeña espera tras encender antes de dibujar, evita parpadeo del primer frame

    /* Limpiar pantalla a negro */
    ILI9341_FillScreen(COLOR_BLACK);   // limpia toda la pantalla a negro apenas termina el init, para no mostrar basura de VRAM sin inicializar
}

/* ========================================================================== */
/* === VENTANA DE DIRECCION ================================================= */
/* ========================================================================== */

void ILI9341_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {   // define el rectangulo (x0,y0)-(x1,y1) donde caen los proximos pixeles escritos y deja el bus listo para recibirlos
    uint8_t d[4];   // buffer de 4 bytes para mandar los pares hi/lo de columnas y luego de filas

    /* CASET (0x2A) — Columnas (Eje X) */
    d[0] = x0 >> 8; d[1] = x0 & 0xFF;   // byte alto y bajo de la columna inicial x0 (el controlador espera 16 bits big-endian)
    d[2] = x1 >> 8; d[3] = x1 & 0xFF;   // byte alto y bajo de la columna final x1
    LCD_CmdData(0x2A, d, 4);            // CASET (0x2A): fija el rango de columnas [x0,x1] de la ventana activa

    /* PASET (0x2B) — Filas (Eje Y) */
    d[0] = y0 >> 8; d[1] = y0 & 0xFF;   // byte alto y bajo de la fila inicial y0
    d[2] = y1 >> 8; d[3] = y1 & 0xFF;   // byte alto y bajo de la fila final y1
    LCD_CmdData(0x2B, d, 4);            // PASET (0x2B): fija el rango de filas [y0,y1] de la ventana activa

    /* RAMWR (0x2C) — Preparar bus para recibir píxeles */
    LCD_CS_LOW();                          // selecciona el chip para la escritura de pixeles que sigue
    LCD_DC_LOW();                          // modo comando para mandar el byte RAMWR
    uint8_t c = 0x2C;                      // RAMWR: comando de "escritura de memoria" -- cada byte que se mande despues cae dentro de la ventana ya definida
    LCD_SPI_Send(&c, 1, 10);               // envia el comando RAMWR
    LCD_DC_HIGH();                         // pasa a modo DATOS: deja el bus listo para los pixeles -- CS se queda BAJO a proposito (ver ILI9341_EndWrite)
}

void ILI9341_EndWrite(void) {   // cierra una transaccion de escritura de pixeles abierta por ILI9341_SetWindow
    LCD_CS_HIGH();   // sube Chip Select: cierra la transaccion de escritura abierta por SetWindow -- hay que llamarla siempre despues de escribir pixeles, si no el bus queda "colgado" en modo dato
}

/* ========================================================================== */
/* === ORIENTACION EN TIEMPO DE EJECUCION ==================================== */
/* ========================================================================== */
/* Reconfigura MADCTL y las dimensiones activas. Usar antes de dibujar una    */
/* pantalla que necesite la otra orientacion (ej. modo cocktail Simon).      */

/* Estado persistente del giro de 180 grados: permite que el flip aplicado
 * una sola vez se mantenga vigente en toda la aplicacion (splash, menus y
 * juego), sin importar cuantas veces se llame despues a
 * ILI9341_SetPortrait. Sin esta bandera, ILI9341_SetPortrait escribiria un
 * MADCTL "limpio" (sin flip) en cada llamada -- como se llama en cada
 * cambio de pantalla (Demo_Enter, arranque de cada modo real, etc.), un
 * flip pedido una sola vez se perderia en la siguiente transicion. Con la
 * bandera, ILI9341_SetPortrait reaplica el ultimo flip solicitado en cada
 * llamada. */
static uint8_t ili9341_flip_180 = 0;   // bandera persistente: 1 = mantener el panel girado 180 en cada llamada a SetPortrait futura, 0 = normal

void ILI9341_SetPortrait(uint8_t portrait) {   // 1=retrato 240x320, 0=paisaje 320x240 -- reaplica tambien el ultimo flip180 pedido (ver bandera de arriba)
    uint8_t madctl = portrait ? ILI9341_MADCTL_PORTRAIT : ILI9341_MADCTL_LANDSCAPE;   // valor base de MADCTL segun se pida retrato (240x320) o paisaje (320x240)
    if (ili9341_flip_180) madctl ^= 0xC0;   // si hay un flip180 pendiente, invierte los bits MY|MX (0xC0) sobre la orientacion base para que el flip sobreviva al cambio de orientacion
    LCD_CmdData(0x36, &madctl, 1);          // aplica el MADCTL calculado al controlador

    if (portrait) { ILI9341_W = 240; ILI9341_H = 320; }   // actualiza dimensiones logicas activas a retrato -- todas las funciones de dibujo (FillRect, clipping, etc.) usan estas variables
    else          { ILI9341_W = 320; ILI9341_H = 240; }   // o a paisaje si portrait es 0
}

/* MY|MX (bits 7,6 de MADCTL) invertidos sobre la orientacion base -- misma
 * dimension logica, panel fisicamente rotado 180 grados. ILI9341_W/H no
 * cambian porque MV (swap fila/col) no se toca. Guarda el pedido en
 * ili9341_flip_180 para que sobreviva a los ILI9341_SetPortrait() que
 * vengan despues (ver comentario arriba). */
void ILI9341_SetFlip180(uint8_t flip) {   // 1=panel rotado 180 grados, 0=normal -- misma orientacion logica (ancho/alto no cambian)
    ili9341_flip_180 = flip;   // guarda el pedido de flip para que SetPortrait lo siga aplicando en cada cambio de pantalla futuro
    uint8_t portrait = (ILI9341_H > ILI9341_W) ? 1 : 0;   // detecta la orientacion ACTUAL mirando las dimensiones ya activas (no recibe el parametro, lo infiere)
    uint8_t madctl    = portrait ? ILI9341_MADCTL_PORTRAIT : ILI9341_MADCTL_LANDSCAPE;   // MADCTL base para esa orientacion, todavia sin flip
    if (flip) madctl ^= 0xC0;        // si se pide flip, invierte MY|MX (bits 7,6) -- esto gira la imagen 180 grados sin tocar el ancho/alto logico
    LCD_CmdData(0x36, &madctl, 1);   // aplica el MADCTL resultante de inmediato (a diferencia de SetPortrait, esta funcion manda el comando ya mismo, no espera a la proxima llamada)
}

/* ========================================================================== */
/* === ESCRITURA DE PIXELES ================================================= */
/* ========================================================================== */

void ILI9341_WritePixels(const uint8_t *buf, uint32_t len_bytes) {   // manda un buffer crudo de pixeles RGB565 ya dentro de una ventana abierta por SetWindow
    LCD_DC_HIGH();   // modo datos: lo que sigue son pixeles, no comandos
    /* HAL_SPI_Transmit tiene Size uint16_t (max 65535) — enviar en trozos */
    while (len_bytes > 0) {   // repite hasta vaciar el buffer completo, porque el HAL solo admite hasta 65535 bytes por transmision
        uint16_t chunk = (len_bytes > 65534u) ? 65534u : (uint16_t)len_bytes;   // cuanto mandar en esta vuelta: como maximo 65534 bytes (un poco menos del limite real del HAL, por margen)
        LCD_SPI_Send(buf, chunk, 2000);   // transmite ese trozo por SPI, timeout generoso (2000ms) porque puede ser una imagen grande
        buf       += chunk;   // avanza el puntero de lectura la cantidad ya enviada
        len_bytes -= chunk;   // resta del total lo ya mandado, hasta llegar a 0 y salir del while
    }
}

/* ========================================================================== */
/* === FIGURAS BASICAS ======================================================= */
/* ========================================================================== */

void ILI9341_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {   // rectangulo relleno -- primitiva base que reutilizan casi todas las demas funciones de dibujo de este archivo
    if (!w || !h) return;                              // ancho o alto 0: no hay nada que dibujar, evita mandar una ventana invalida al controlador
    if (x >= ILI9341_W || y >= ILI9341_H) return;       // el rectangulo arranca totalmente fuera de la pantalla activa: no dibuja nada (clipping trivial)

    if ((uint32_t)x + w > ILI9341_W) w = ILI9341_W - x;   // recorta el ancho si el rectangulo se sale por la derecha de la pantalla (clipping)
    if ((uint32_t)y + h > ILI9341_H) h = ILI9341_H - y;   // recorta el alto si se sale por abajo

    ILI9341_SetWindow(x, y, x + w - 1, y + h - 1);   // define la ventana de escritura ya recortada y deja el bus listo para los pixeles

    uint8_t hi = color >> 8;      // byte alto del color RGB565 (bits 15-8)
    uint8_t lo = color & 0xFF;    // byte bajo del color RGB565 (bits 7-0) -- cambiar el color de entrada es lo unico necesario para pintar de otro color
    uint16_t fill = (w <= ILI9341_W) ? w : ILI9341_W;   // ancho real a usar para armar la fila de relleno (nunca mas que el ancho de pantalla, por seguridad del buffer estatico)
    for (uint16_t i = 0; i < fill; i++) {   // arma UNA fila completa del color pedido en el buffer estatico lcd_row_buf
        lcd_row_buf[i * 2]     = hi;   // byte alto del pixel i de esa fila
        lcd_row_buf[i * 2 + 1] = lo;   // byte bajo del pixel i de esa fila
    }

    for (uint16_t row = 0; row < h; row++) {   // repite el envio de la misma fila ya armada, una vez por cada fila del rectangulo (evita recalcular el color en cada fila)
        LCD_SPI_Send(lcd_row_buf, (uint16_t)(fill * 2), 500);   // transmite esa fila de pixeles por SPI (fill*2 porque cada pixel son 2 bytes), timeout 500ms
    }

    ILI9341_EndWrite();   // cierra la transaccion de escritura abierta por SetWindow, sube CS
}

void ILI9341_FillScreen(uint16_t color) {   // llena toda la pantalla activa de un solo color
    ILI9341_FillRect(0, 0, ILI9341_W, ILI9341_H, color);   // reutiliza FillRect cubriendo toda la pantalla activa -- respeta automaticamente la orientacion actual (ILI9341_W/H)
}

void ILI9341_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {   // pinta un solo pixel
    ILI9341_FillRect(x, y, 1, 1, color);   // pinta un unico pixel reutilizando FillRect con ancho y alto 1 -- simple pero no la forma mas rapida si se llama muchas veces seguidas (cada llamada abre y cierra su propia ventana SPI)
}

void ILI9341_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {   // rectangulo SIN relleno (solo contorno), armado con 4 FillRect angostos
    if (!w || !h) return;   // sin ancho o alto no hay nada que dibujar
    ILI9341_FillRect(x,         y,         w, 1, color);   // borde superior: franja de 1 pixel de alto a todo lo ancho
    ILI9341_FillRect(x,         y + h - 1, w, 1, color);   // borde inferior: misma franja pero en la ultima fila del rectangulo
    ILI9341_FillRect(x,         y,         1, h, color);   // borde izquierdo: franja de 1 pixel de ancho a todo el alto
    ILI9341_FillRect(x + w - 1, y,         1, h, color);   // borde derecho: misma franja pero en la ultima columna
}

/* Bresenham clasico. Los tramos horizontales/verticales se despachan como
 * FillRect de una fila/columna para aprovechar la rafaga SPI. */
void ILI9341_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {   // linea entre 2 puntos cualquiera; casos horizontal/vertical van por atajo, el resto por Bresenham
    if (y0 == y1) {   // caso especial: linea perfectamente horizontal -- se manda como un solo FillRect (mucho mas rapido que pixel por pixel)
        int16_t x = (x0 < x1) ? x0 : x1;   // toma el extremo izquierdo de la linea como punto de partida del rectangulo
        uint16_t w = (uint16_t)((x0 < x1) ? (x1 - x0) : (x0 - x1)) + 1;   // ancho del rectangulo = distancia entre extremos +1 (incluye ambos puntos)
        if (x >= 0 && y0 >= 0) ILI9341_FillRect((uint16_t)x, (uint16_t)y0, w, 1, color);   // dibuja la linea horizontal como una franja de 1 pixel de alto, solo si no arranca en coordenadas negativas
        return;   // ya termino, no sigue al algoritmo de Bresenham general
    }
    if (x0 == x1) {   // caso especial: linea perfectamente vertical -- mismo truco que la horizontal pero con FillRect angosto
        int16_t y = (y0 < y1) ? y0 : y1;   // extremo superior como punto de partida
        uint16_t h = (uint16_t)((y0 < y1) ? (y1 - y0) : (y0 - y1)) + 1;   // alto del rectangulo = distancia entre extremos +1
        if (x0 >= 0 && y >= 0) ILI9341_FillRect((uint16_t)x0, (uint16_t)y, 1, h, color);   // dibuja la linea vertical como una franja de 1 pixel de ancho
        return;   // termina aca, no entra al Bresenham general
    }

    int16_t dx = (int16_t)((x1 > x0) ? (x1 - x0) : (x0 - x1));   // distancia horizontal absoluta entre los dos puntos (Bresenham)
    int16_t sx = (x0 < x1) ? 1 : -1;   // signo del paso en X: hacia que lado avanza x0 en cada iteracion
    int16_t dy = (int16_t)-((y1 > y0) ? (y1 - y0) : (y0 - y1));   // distancia vertical, en negativo (convencion estandar de Bresenham para simplificar la comparacion del error)
    int16_t sy = (y0 < y1) ? 1 : -1;   // signo del paso en Y
    int16_t err = dx + dy;   // termino de error inicial que decide cuando avanzar en X, en Y, o en ambos

    while (1) {   // recorre pixel por pixel desde (x0,y0) hasta llegar a (x1,y1)
        if (x0 >= 0 && y0 >= 0) ILI9341_DrawPixel((uint16_t)x0, (uint16_t)y0, color);   // pinta el pixel actual, solo si cae en coordenadas validas (no negativas)
        if (x0 == x1 && y0 == y1) break;   // si ya llego al punto final, corta el bucle
        int16_t e2 = (int16_t)(2 * err);   // error duplicado, para comparar sin usar division/fracciones
        if (e2 >= dy) { err = (int16_t)(err + dy); x0 = (int16_t)(x0 + sx); }   // si corresponde, acumula error y avanza un paso en X
        if (e2 <= dx) { err = (int16_t)(err + dx); y0 = (int16_t)(y0 + sy); }   // si corresponde, acumula error y avanza un paso en Y (puede pasar junto con el paso en X, para diagonales de 45°)
    }
}

/* Circulo de punto medio (Bresenham) — solo contorno, 8 octantes por simetria. */
void ILI9341_DrawCircle(int16_t xc, int16_t yc, int16_t r, uint16_t color) {   // solo el contorno del circulo (sin relleno)
    int16_t x = r, y = 0, err = 0;   // arranca en el punto mas a la derecha del circulo (x=r, y=0), algoritmo de punto medio

    while (x >= y) {   // recorre solo un octante (1/8 del circulo) y refleja el resto por simetria -- mucho mas barato que calcular los 360 grados
        if (xc + x >= 0 && yc + y >= 0) ILI9341_DrawPixel((uint16_t)(xc + x), (uint16_t)(yc + y), color);   // punto del octante 1 (derecha-abajo), si no cae en coordenadas negativas
        if (xc + y >= 0 && yc + x >= 0) ILI9341_DrawPixel((uint16_t)(xc + y), (uint16_t)(yc + x), color);   // reflejo intercambiando x/y -- octante 2
        if (xc - y >= 0 && yc + x >= 0) ILI9341_DrawPixel((uint16_t)(xc - y), (uint16_t)(yc + x), color);   // octante 3 (izquierda-abajo)
        if (xc - x >= 0 && yc + y >= 0) ILI9341_DrawPixel((uint16_t)(xc - x), (uint16_t)(yc + y), color);   // octante 4
        if (xc - x >= 0 && yc - y >= 0) ILI9341_DrawPixel((uint16_t)(xc - x), (uint16_t)(yc - y), color);   // octante 5 (izquierda-arriba)
        if (xc - y >= 0 && yc - x >= 0) ILI9341_DrawPixel((uint16_t)(xc - y), (uint16_t)(yc - x), color);   // octante 6
        if (xc + y >= 0 && yc - x >= 0) ILI9341_DrawPixel((uint16_t)(xc + y), (uint16_t)(yc - x), color);   // octante 7 (derecha-arriba)
        if (xc + x >= 0 && yc - y >= 0) ILI9341_DrawPixel((uint16_t)(xc + x), (uint16_t)(yc - y), color);   // octante 8, cierra la vuelta completa

        y++;   // avanza siempre un paso en Y (se mueve hacia abajo del octante en cada iteracion)
        if (err <= 0) { err += 2 * y + 1; }   // ajuste de error si todavia no hace falta acercar X al centro
        if (err > 0)  { x--; err -= 2 * x + 1; }   // si el error se paso, retrocede X un paso para mantenerse sobre el borde del circulo -- achicar/agrandar `r` es lo unico necesario para cambiar el tamaño, el algoritmo no se toca
    }
}

/* Circulo relleno — barrido de lineas horizontales entre los bordes de cada fila. */
void ILI9341_FillCircle(int16_t xc, int16_t yc, int16_t r, uint16_t color) {   // circulo RELLENO -- el que usa renderer.c para los domos de los botones y las notas
    for (int16_t y = -r; y <= r; y++) {   // recorre cada fila del circulo, desde -r hasta +r relativo al centro
        int16_t dx = (int16_t)((int32_t)r * r - (int32_t)y * y);   // por Pitagoras: el cuadrado de la mitad del ancho de esta fila (r²-y²)
        /* raiz entera aproximada por busqueda lineal (r es pequeño en pantalla) */
        int16_t half = 0;   // va a acumular la raiz cuadrada entera de dx (mitad del ancho de esta fila)
        while ((half + 1) * (half + 1) <= dx) half++;   // sube half hasta el mayor entero cuyo cuadrado no se pasa de dx -- asi obtiene la "media anchura" sin usar sqrt() de punto flotante
        int16_t yy = (int16_t)(yc + y);   // coordenada Y real en pantalla de esta fila
        if (yy < 0) continue;   // si esta fila cae fuera de pantalla (arriba), se salta sin dibujar
        int16_t xx0 = (int16_t)(xc - half);   // columna donde arranca la franja horizontal de esta fila
        if (xx0 < 0) xx0 = 0;   // recorta el arranque si se sale de pantalla por la izquierda
        uint16_t w = (uint16_t)(2 * half + 1);   // ancho de la franja: 2*half+1 para cubrir simetricamente ambos lados del centro
        ILI9341_FillRect((uint16_t)xx0, (uint16_t)yy, w, 1, color);   // pinta toda la franja de esta fila de una sola vez (mucho mas rapido que pixel por pixel)
    }
}

/* Circulo relleno de 2 colores CONCENTRICOS (anillo/cuerpo exterior +
 * nucleo interior) en un solo pase por fila -- pensado para el patron que
 * se repite en todo el proyecto (domos de boton, notas, zonas de golpe: un
 * circulo grande de un color con un circulo mas chico centrado encima de
 * otro color). Dibujarlos con 2 llamadas separadas a ILI9341_FillCircle
 * pinta la region central DOS veces (una vez el color exterior, tapado
 * despues por el interior) y abre una ventana SPI nueva por cada fila de
 * CADA uno de los 2 circulos por separado. Esta version arma un unico
 * buffer de fila con ambos colores ya resueltos y abre una sola ventana
 * SPI por fila (para el circulo completo, no 2), lo que corta a la mitad
 * la cantidad de transacciones SPI y elimina el repintado duplicado del
 * centro -- relevante en Guitar Hero a 2 jugadores, donde se dibujan
 * varias notas por tick y el tiempo de SPI extra puede hacer que un frame
 * se pase de los 33ms del loop principal (RENDER_TICK_MS), sintiendose
 * como lag y a veces perdiendo la lectura de botones de ese tick.
 * r_in<=0 equivale a un ILI9341_FillCircle normal (solo color_out). */
void ILI9341_FillCircle2(int16_t xc, int16_t yc, int16_t r_out, uint16_t color_out,
                          int16_t r_in, uint16_t color_in) {
    uint8_t hi_out = (uint8_t)(color_out >> 8), lo_out = (uint8_t)(color_out & 0xFF);   // bytes RGB565 del color exterior, calculados una sola vez fuera del loop de filas
    uint8_t hi_in  = (uint8_t)(color_in  >> 8), lo_in  = (uint8_t)(color_in  & 0xFF);   // idem para el color interior

    for (int16_t y = -r_out; y <= r_out; y++) {   // recorre cada fila del circulo EXTERIOR, desde -r_out hasta +r_out
        int16_t yy = (int16_t)(yc + y);   // coordenada Y real en pantalla de esta fila
        if (yy < 0 || yy >= (int16_t)ILI9341_H) continue;   // fila fuera de pantalla (arriba o abajo) -- nada que dibujar en esta vuelta

        int32_t dx_out = (int32_t)r_out * r_out - (int32_t)y * y;   // Pitagoras: cuadrado de la media anchura del circulo exterior en esta fila
        int16_t half_out = 0;
        while ((int32_t)(half_out + 1) * (half_out + 1) <= dx_out) half_out++;   // misma busqueda lineal de raiz entera que ILI9341_FillCircle

        int16_t half_in = -1;   // -1 = esta fila no llega a cruzar el circulo interior (fuera de su rango vertical)
        if (r_in > 0) {   // solo hay circulo interior si se pidio un radio positivo
            int32_t dx_in = (int32_t)r_in * r_in - (int32_t)y * y;   // mismo calculo de Pitagoras, con el radio interior
            if (dx_in >= 0) {   // esta fila SI cae dentro del rango vertical del circulo interior
                half_in = 0;
                while ((int32_t)(half_in + 1) * (half_in + 1) <= dx_in) half_in++;
            }
        }

        int16_t xx0 = (int16_t)(xc - half_out);   // columna donde arranca la franja de esta fila (todavia sin recortar contra la pantalla)
        int16_t xx1 = (int16_t)(xc + half_out);   // columna donde termina (inclusive)
        if (xx1 < 0 || xx0 >= (int16_t)ILI9341_W) continue;   // la franja completa cae fuera de pantalla (izquierda o derecha) -- nada que dibujar
        if (xx0 < 0) xx0 = 0;                                    // recorta el arranque si se sale por la izquierda
        if (xx1 >= (int16_t)ILI9341_W) xx1 = (int16_t)(ILI9341_W - 1);   // recorta el final si se sale por la derecha
        uint16_t w = (uint16_t)(xx1 - xx0 + 1);   // ancho final ya recortado de la franja de esta fila
        if (w == 0 || w > ILI9341_MAXDIM) continue;   // seguridad: nunca exceder el tamaño del buffer estatico de fila

        int16_t in_x0 = (int16_t)(xc - half_in);   // columna absoluta donde arranca el nucleo interior en esta fila (solo valido si half_in>=0)
        int16_t in_x1 = (int16_t)(xc + half_in);   // columna absoluta donde termina el nucleo interior en esta fila

        for (uint16_t i = 0; i < w; i++) {   // arma el buffer de ESTA fila con los 2 colores ya resueltos, un solo recorrido
            int16_t px = (int16_t)(xx0 + i);   // columna absoluta de este pixel de la franja
            uint8_t hi, lo;
            if (half_in >= 0 && px >= in_x0 && px <= in_x1) { hi = hi_in;  lo = lo_in;  }   // este pixel cae dentro del nucleo interior
            else                                             { hi = hi_out; lo = lo_out; }   // este pixel es del anillo/cuerpo exterior
            lcd_row_buf[i * 2]     = hi;
            lcd_row_buf[i * 2 + 1] = lo;
        }

        ILI9341_SetWindow((uint16_t)xx0, (uint16_t)yy, (uint16_t)xx1, (uint16_t)yy);   // UNA sola ventana SPI para toda la franja (los 2 colores incluidos), no 2 como antes
        LCD_SPI_Send(lcd_row_buf, (uint16_t)(w * 2), 500);   // transmite la fila completa ya resuelta
        ILI9341_EndWrite();   // cierra la transaccion de esta fila
    }
}

/* ========================================================================== */
/* === IMAGENES ============================================================== */
/* ========================================================================== */

void ILI9341_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *data) {   // vuelca un bitmap RGB565 (ej. splash_bg.h) tal cual, sin decodificar ningun formato de compresion
    if (!w || !h) return;                              // sin ancho o alto no hay imagen que dibujar
    if (x >= ILI9341_W || y >= ILI9341_H) return;       // si arranca totalmente fuera de pantalla, no dibuja nada
    if ((uint32_t)x + w > ILI9341_W) w = ILI9341_W - x;   // recorta el ancho si se sale por la derecha
    if ((uint32_t)y + h > ILI9341_H) h = ILI9341_H - y;   // recorta el alto si se sale por abajo -- ojo: si se recorta, WritePixels igual manda w*h bytes del buffer original mas abajo, asi que una imagen recortada puede salir corrida (no hay recorte real de los datos fuente, solo de la ventana destino)

    ILI9341_SetWindow(x, y, x + w - 1, y + h - 1);        // define la ventana destino ya recortada
    ILI9341_WritePixels(data, (uint32_t)w * h * 2);       // vuelca el buffer de la imagen tal cual (RGB565, 2 bytes por pixel) -- asume que `data` ya tiene exactamente w*h pixeles en ese formato
    ILI9341_EndWrite();                                    // cierra la transaccion de escritura
}

/* ========================================================================== */
/* === FUENTE BITMAP 5x7 (ASCII 32-90) ======================================= */
/* ========================================================================== */
/* Cada char: 5 bytes (columnas izq→der). Cada byte: bit0=fila top, bit6=bot. */
/* Los comentarios de cada fila ya identifican el caracter/codigo ASCII que    */
/* representa -- para cambiar la forma de una letra/simbolo alcanza con editar */
/* SOLO esos 5 bytes de su fila; no hace falta tocar nada mas del driver.      */

static const uint8_t font5x7[][5] = {   // tabla de glifos, un renglon por caracter ASCII 32-90 (ver comentario del bloque arriba para el formato de cada byte)
    {0x00,0x00,0x00,0x00,0x00}, /* ' ' 32 */
    {0x00,0x00,0x5F,0x00,0x00}, /* '!' 33 */
    {0x00,0x07,0x00,0x07,0x00}, /* '"' 34 */
    {0x14,0x7F,0x14,0x7F,0x14}, /* '#' 35 */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* '$' 36 */
    {0x23,0x13,0x08,0x64,0x62}, /* '%' 37 */
    {0x36,0x49,0x55,0x22,0x50}, /* '&' 38 */
    {0x00,0x05,0x03,0x00,0x00}, /* ''' 39 */
    {0x00,0x1C,0x22,0x41,0x00}, /* '(' 40 */
    {0x00,0x41,0x22,0x1C,0x00}, /* ')' 41 */
    {0x08,0x2A,0x1C,0x2A,0x08}, /* '*' 42 */
    {0x08,0x08,0x3E,0x08,0x08}, /* '+' 43 */
    {0x00,0x50,0x30,0x00,0x00}, /* ',' 44 */
    {0x08,0x08,0x08,0x08,0x08}, /* '-' 45 */
    {0x00,0x60,0x60,0x00,0x00}, /* '.' 46 */
    {0x20,0x10,0x08,0x04,0x02}, /* '/' 47 */
    {0x3E,0x51,0x49,0x45,0x3E}, /* '0' 48 */
    {0x00,0x42,0x7F,0x40,0x00}, /* '1' 49 */
    {0x42,0x61,0x51,0x49,0x46}, /* '2' 50 */
    {0x21,0x41,0x45,0x4B,0x31}, /* '3' 51 */
    {0x18,0x14,0x12,0x7F,0x10}, /* '4' 52 */
    {0x27,0x45,0x45,0x45,0x39}, /* '5' 53 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* '6' 54 */
    {0x01,0x71,0x09,0x05,0x03}, /* '7' 55 */
    {0x36,0x49,0x49,0x49,0x36}, /* '8' 56 */
    {0x06,0x49,0x49,0x29,0x1E}, /* '9' 57 */
    {0x00,0x36,0x36,0x00,0x00}, /* ':' 58 */
    {0x00,0x56,0x36,0x00,0x00}, /* ';' 59 */
    {0x08,0x14,0x22,0x41,0x00}, /* '<' 60 */
    {0x14,0x14,0x14,0x14,0x14}, /* '=' 61 */
    {0x00,0x41,0x22,0x14,0x08}, /* '>' 62 */
    {0x02,0x01,0x51,0x09,0x06}, /* '?' 63 */
    {0x32,0x49,0x79,0x41,0x3E}, /* '@' 64 */
    {0x7E,0x11,0x11,0x11,0x7E}, /* 'A' 65 */
    {0x7F,0x49,0x49,0x49,0x36}, /* 'B' 66 */
    {0x3E,0x41,0x41,0x41,0x22}, /* 'C' 67 */
    {0x7F,0x41,0x41,0x22,0x1C}, /* 'D' 68 */
    {0x7F,0x49,0x49,0x49,0x41}, /* 'E' 69 */
    {0x7F,0x09,0x09,0x09,0x01}, /* 'F' 70 */
    {0x3E,0x41,0x49,0x49,0x7A}, /* 'G' 71 */
    {0x7F,0x08,0x08,0x08,0x7F}, /* 'H' 72 */
    {0x00,0x41,0x7F,0x41,0x00}, /* 'I' 73 */
    {0x20,0x40,0x41,0x3F,0x01}, /* 'J' 74 */
    {0x7F,0x08,0x14,0x22,0x41}, /* 'K' 75 */
    {0x7F,0x40,0x40,0x40,0x40}, /* 'L' 76 */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* 'M' 77 */
    {0x7F,0x04,0x08,0x10,0x7F}, /* 'N' 78 */
    {0x3E,0x41,0x41,0x41,0x3E}, /* 'O' 79 */
    {0x7F,0x09,0x09,0x09,0x06}, /* 'P' 80 */
    {0x3E,0x41,0x51,0x21,0x5E}, /* 'Q' 81 */
    {0x7F,0x09,0x19,0x29,0x46}, /* 'R' 82 */
    {0x46,0x49,0x49,0x49,0x31}, /* 'S' 83 */
    {0x01,0x01,0x7F,0x01,0x01}, /* 'T' 84 */
    {0x3F,0x40,0x40,0x40,0x3F}, /* 'U' 85 */
    {0x1F,0x20,0x40,0x20,0x1F}, /* 'V' 86 */
    {0x3F,0x40,0x38,0x40,0x3F}, /* 'W' 87 */
    {0x63,0x14,0x08,0x14,0x63}, /* 'X' 88 */
    {0x07,0x08,0x70,0x08,0x07}, /* 'Y' 89 */
    {0x61,0x51,0x49,0x45,0x43}, /* 'Z' 90 */
};

void ILI9341_DrawChar(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale) {   // dibuja UN caracter de la fuente 5x7, escalado por `scale`
    if (c >= 'a' && c <= 'z') c = (char)(c - 32);   /* minusculas -> mayusculas */   // la tabla font5x7 solo tiene glifos en mayuscula, asi que normaliza antes de buscar
    if (c < 32 || c > 90) c = '?';   // cualquier caracter fuera del rango soportado (32-90) se dibuja como '?' en vez de leer memoria fuera de la tabla
    const uint8_t *g = font5x7[(uint8_t)c - 32];   // ubica la fila de la tabla de este caracter (indice 0 = espacio, codigo ASCII 32)

    uint16_t adv = (uint16_t)(6u * scale);   // ancho que ocupa este caracter en pantalla: 5 columnas de glifo + 1 de espacio, por la escala -- subir `scale` agranda el texto
    uint16_t hgt = (uint16_t)(7u * scale);   // alto que ocupa: 7 filas de glifo, por la escala
    ILI9341_FillRect(x, y, adv, hgt, bg);    // primero pinta todo el rectangulo del caracter con el color de fondo (bg), asi el glifo queda limpio aunque antes hubiera otra cosa dibujada ahi

    for (uint8_t col = 0; col < 5; col++) {   // recorre las 5 columnas del glifo
        uint8_t bits = g[col];   // los 7 bits (uno por fila) de esta columna del caracter
        for (uint8_t row = 0; row < 7; row++) {   // recorre las 7 filas de esa columna
            if (bits & (1u << row)) {   // si el bit de esta fila esta en 1, ese pixel del glifo va pintado (bit0=fila de arriba, ver comentario del header de la tabla)
                ILI9341_FillRect((uint16_t)(x + col * scale),
                                 (uint16_t)(y + row * scale),
                                 scale, scale, fg);   // pinta un bloque de scale x scale pixeles en la posicion de este bit (asi se escala el glifo entero) con el color de frente (fg)
            }
        }
    }
}

void ILI9341_DrawString(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t scale) {   // dibuja una cadena completa encadenando ILI9341_DrawChar caracter por caracter
    while (*s) {   // recorre la cadena caracter por caracter hasta el '\0' final
        ILI9341_DrawChar(x, y, *s, fg, bg, scale);   // dibuja el caracter actual en la posicion x,y actual
        x = (uint16_t)(x + 6u * scale);   // avanza x el ancho de un caracter (mismo calculo que `adv` en DrawChar) para que el siguiente no se superponga
        s++;   // avanza al siguiente caracter de la cadena
    }
}
