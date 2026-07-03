# Ejercicios de Practica — Semana 05
## Curso Taller V (Sistemas Embebidos STM32F4xx)
### Temporizadores y Medición de Tiempo

Los ejercicios de esta semana reemplazan el ineficiente retardo `for()` de la semana 04 con temporización impulsada por temporizadores de hardware. Cada ejercicio desde esta semana en adelante incluye un **LED_OK** obligatorio — un LED de latido controlado por su propio temporizador dedicado que parpadea continuamente para mostrar que el sistema está vivo y funcionando. Si LED_OK deja de parpadear, algo está mal a nivel del sistema, independientemente de lo que esté haciendo el resto del código. Esta es una práctica estándar en sistemas embebidos reales y debes adoptarla como hábito desde este momento.

Todos los ejercicios usan la siguiente convención de asignación de temporizadores a menos que se indique lo contrario:

- **TIM2** — dedicado a LED_OK, siempre parpadeando a 1 Hz (500ms encendido, 500ms apagado). Nunca uses TIM2 para ningún otro propósito.
- **TIM3** — el temporizador principal del ejercicio, configurado para el período requerido por cada ejercicio.

Ambos temporizadores deben configurarse completamente a nivel de registros usando notación CMSIS y nombres de máscaras definidos por CMSIS. Cada ISR debe seguir el patrón obligatorio: verificar el flag de interrupción, limpiarlo inmediatamente, luego establecer un flag `volatile` para que `main()` lo procese. El bucle principal verifica los flags y responde — nunca realices lógica compleja directamente dentro de una ISR.

Antes de escribir cualquier código para cada ejercicio, completa siempre estos pasos en orden: habilita el reloj RCC para el temporizador, configura PSC y ARR para el período deseado, habilita la interrupción de actualización en DIER, llama a `NVIC_EnableIRQ()`, e inicia el contador estableciendo CEN en CR1 al final.

---

### Ejercicio 5.1 — Blinky con dos temporizadores: LED principal y LED_OK

Este ejercicio replica lo que se hizo en la clase de teoría, pero ahora con dos temporizadores funcionando simultáneamente. Configura TIM2 para hacer parpadear LED_OK exactamente a 1 Hz usando el pin 5 de GPIOA (el LED de la placa). Configura TIM3 para hacer parpadear un segundo LED externo a una velocidad diferente de tu elección — algo claramente distinguible de 1 Hz, como 4 Hz o 0.5 Hz.

Cada temporizador necesita su propia ISR (`TIM2_IRQHandler` y `TIM3_IRQHandler`), su propia variable flag `volatile`, y su propio bloque de manejo en el bucle principal. Los dos LEDs deben parpadear de forma completamente independiente — la velocidad de uno nunca debe afectar la velocidad del otro.

Antes de escribir cualquier código, calcula tus valores de PSC y ARR para ambos temporizadores en papel, recorriendo la cadena de señal: reloj del sistema → PSC → frecuencia de tick → ARR → período del UpdateEvent. Verifica tus cálculos antes de cargarlos en los registros. Después de que el programa esté funcionando, abre el visualizador SFR y observa TIM2->CNT y TIM3->CNT contando simultáneamente — esto es paralelismo de hardware en acción. La CPU no está involucrada en el conteo en absoluto.

Piensa en esto: en la semana 04, dos LEDs parpadeando a diferentes velocidades requerían una gestión cuidadosa de un único retardo bloqueante. ¿Cómo cambia el enfoque del temporizador la estructura de tu bucle `while(1){}`? ¿Qué está haciendo el bucle principal ahora mientras los temporizadores están contando?

---

### Ejercicio 5.2 — Velocidad de parpadeo ajustable con dos botones

Extiende el ejercicio 5.1 haciendo que la velocidad de parpadeo de TIM3 sea ajustable mediante dos botones. Un botón aumenta la frecuencia de parpadeo (período más corto), el otro la disminuye (período más largo). LED_OK en TIM2 debe continuar parpadeando exactamente a 1 Hz independientemente de lo que le pase a TIM3.

Para cambiar el período de TIM3 mientras está funcionando, debes escribir nuevos valores en TIM3->PSC y TIM3->ARR. Piensa cuidadosamente sobre cuándo y cómo hacer esto de forma segura — modificar los registros de un temporizador en funcionamiento puede causar comportamientos inesperados si no se hace correctamente. Define un período mínimo y máximo para evitar que el parpadeo sea demasiado rápido para verlo o demasiado lento para ser útil.

Lee el estado de los botones mediante polling dentro del bucle principal — no uses interrupciones para los botones todavía. Estructura tu código de manera que la lectura de botones, el ajuste del período de TIM3 y el toggle del LED se manejen de forma limpia y separada en el bucle principal. Piensa en qué ocurre si ambos botones se presionan simultáneamente — define ese comportamiento explícitamente.

Después de que el programa funcione correctamente, reflexiona sobre una limitación que podrías notar: ¿qué le pasa al parpadeo del LED si mantienes presionado un botón? ¿Es el comportamiento suave? ¿Qué lo haría mejor? Escribe tus observaciones como un comentario en el código.

---

### Ejercicio 5.3 — Contador binario con 6 LEDs

Conecta 6 LEDs externos a 6 pines de salida GPIO consecutivos. Estos LEDs mostrarán un número binario que se incrementa en cada UpdateEvent de TIM3. LED_OK continúa en TIM2 a 1 Hz.

Configura TIM3 con un período de tu elección entre 250ms y 500ms — elige un valor y mantenlo. Cada vez que TIM3 dispara, incrementa una variable contadora `uint8_t` en el bucle principal. Mapea cada bit del contador a un LED correspondiente: el bit 0 controla LED0, el bit 1 controla LED1, y así hasta el bit 5 controlando LED5. Cuando el contador llega a 63 (los 6 bits establecidos), el siguiente incremento lo vuelve a 0 automáticamente — esto ocurre naturalmente con un `uint8_t` si enmascaras a 6 bits, o puedes dejar que desborde y enmascarar al escribir al GPIO.

La extracción de bits es el desafío clave de este ejercicio. Para obtener el estado del bit N del contador y aplicarlo a un pin LED específico, necesitas combinar las habilidades bit a bit de las semanas 2 y 3 con las habilidades de salida GPIO de la semana 4. Piensa en cómo aislar cada bit y mapearlo al pin de salida correcto. No uses cadenas `if/else` para esto — encuentra un enfoque más elegante usando operaciones bit a bit y desplazamientos.

Antes de ejecutar el código, predice la secuencia: ¿qué patrón binario deberías ver en los LEDs? Con tu período elegido, ¿cuánto tiempo tarda en contar de 0 a 63 y volver a 0? Verifica tu predicción observando los LEDs y comprobando la variable contadora en el depurador. También observa LED_OK durante todo el proceso — nunca debe dejar de parpadear, incluso mientras el contador está funcionando.

---

### Ejercicio 5.4 — FSM de semáforo controlada por temporizador

Implementa un sistema de semáforo completo usando TIM3 como fuente de temporización para todas las transiciones de estado. LED_OK continúa en TIM2 a 1 Hz. El semáforo usa tres LEDs externos: rojo, amarillo y verde.

Diseña tu FSM antes de escribir cualquier código. Dibuja el diagrama de estados completo en papel con todos los estados, transiciones y tiempos. Las duraciones aproximadas deben ser: verde — 4 segundos, amarillo — 1 segundo, rojo — 4 segundos. Como TIM3 dispara a un período fijo, necesitarás un contador dentro del bucle principal que rastree cuántos eventos de TIM3 han ocurrido en el estado actual, transitando al siguiente estado cuando el conteo alcance el número requerido. Piensa cuidadosamente en cómo implementar esto — es el desafío central de este ejercicio.

Usa `enum` para las definiciones de estado y `switch-case` para la implementación de la FSM, siguiendo el patrón establecido en la semana 04. El `enum` debe tener nombres significativos que describan claramente cada estado del semáforo.

Después de que el sistema esté funcionando, observa el LED_OK cuidadosamente. Debe parpadear exactamente a 1 Hz durante toda la secuencia del semáforo — a través de los estados verde, amarillo y rojo. Si LED_OK alguna vez hace una pausa o se ralentiza, hay código bloqueante en algún lugar de tu bucle principal. Encuéntralo y corrígelo.

Piensa en esto: ¿cuál es la relación entre el período configurado de TIM3 y la precisión de la temporización de tu semáforo? Si TIM3 dispara cada 100ms y quieres una fase verde de 4 segundos, ¿cuántos eventos de TIM3 necesitas contar? ¿Cuál es el error de temporización si el período real de TIM3 difiere ligeramente de tu cálculo?

---

### Ejercicio 5.5 — Semáforo con solicitud de cruce peatonal

Extiende el semáforo del ejercicio 5.4 añadiendo un botón de solicitud de cruce peatonal. Cuando se presiona el botón, el sistema debe registrar la solicitud y transicionar a una fase de cruce peatonal después de que la fase verde actual se complete — nunca interrumpiendo una fase amarilla o roja ya en progreso. La fase de cruce peatonal debe mantener el rojo para los vehículos durante 3 segundos adicionales más allá de la duración normal del rojo, luego reanudar el ciclo normal.

LED_OK continúa en TIM2 a 1 Hz, sin verse afectado por nada más en el sistema.

Este ejercicio requiere un diseño cuidadoso de la FSM. La solicitud peatonal debe almacenarse como un flag que la FSM verifica en el punto de transición apropiado — específicamente al transicionar de verde a amarillo. Si el flag está establecido en ese momento, la FSM entra en un estado rojo extendido; si no, sigue el ciclo normal. Lee el estado del botón mediante polling en el bucle principal — no mediante interrupciones.

Antes de escribir cualquier código, rediseña tu diagrama de estados del ejercicio 5.4 para incluir los nuevos estados peatonales y la transición condicional. Pide a un compañero que revise tu diagrama antes de empezar a codificar — un fallo en el diseño es mucho más fácil de corregir en papel que en código.

Después de que el sistema esté funcionando, prueba los casos límite deliberadamente: ¿qué ocurre si presionas el botón varias veces? ¿Qué ocurre si lo presionas durante la fase amarilla? ¿El sistema se comporta de manera sensata en todos los casos? Escribe un breve informe de prueba como comentarios en tu código describiendo cada caso que probaste y el comportamiento que observaste. Este hábito de pruebas sistemáticas es tan importante como escribir el código en sí.
