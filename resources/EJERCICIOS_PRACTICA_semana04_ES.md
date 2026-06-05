# Ejercicios de Practica — Semana 04
## Curso Taller V (Sistemas Embebidos STM32F4xx)
### Entrada/Salida de Propósito General (GPIO)

Los ejercicios de esta semana reúnen todo lo que has aprendido hasta ahora — configuración de GPIO a nivel de registros, máscaras definidas por CMSIS, operaciones bit a bit, estructuras de control y diseño de FSM — y los conectan con comportamiento de hardware real y visible. Configurarás pines de salida para controlar LEDs, configurarás pines de entrada para leer botones, y construirás progresivamente programas más sofisticados que culminarán en una implementación completa de FSM.

Los ejercicios siguen una progresión deliberada de tres fases. La primera fase se enfoca en las salidas y el retardo por software. La segunda fase introduce las entradas y la toma de decisiones simple. La tercera fase desarrolla el mismo comportamiento de FSM a través de tres implementaciones cada vez más estructuradas, para que puedas ver — y sentir — por qué el enfoque final es mejor que el primero.

Todo el código de esta semana debe seguir los estándares de codificación del curso: solo C, separación de carpetas `Src/Inc`, archivos `.c` y `.h` pareados para cada módulo, nombres de máscaras definidos por CMSIS, operaciones bit a bit correctas y la convención de nomenclatura basada en módulos para las funciones. Antes de escribir cualquier código, siempre consulta el manual de referencia para el registro y el campo de bits que necesitas.

---

### Ejercicio 4.1 — LED parpadeante: el "Hola Mundo" embebido

Configura el pin 5 de GPIOA como salida digital push-pull y escribe un programa que haga parpadear el LED de la placa continuamente. El LED debe encenderse, esperar, apagarse, esperar y repetir. Usa el siguiente patrón de retardo por software para crear los períodos de espera — elige un valor que produzca un parpadeo visible de aproximadamente 1 segundo encendido y 1 segundo apagado:

```c
for(uint32_t i = 0; i < TU_VALOR_AQUI; i++){}
```

Para la configuración del GPIO, usa nombres de máscaras definidos por CMSIS — no uses números directamente para las posiciones de bits. Verifica la configuración completa del registro en el visualizador SFR antes de ejecutar el programa. Después de que el LED parpadee correctamente, experimenta con diferentes valores de retardo y observa cómo cambia la frecuencia de parpadeo. Piensa en esto: si necesitaras que el LED parpadeara exactamente a 1 Hz, ¿qué tan seguro estás de que tu retardo `for()` realmente lo logra? ¿Qué factores podrían afectar la precisión de este enfoque?

---

### Ejercicio 4.2 — Dos LEDs con temporización independiente

Conecta un LED externo a un pin de tu elección en GPIOB. Configura tanto el pin 5 de GPIOA como tu pin de GPIOB elegido como salidas digitales push-pull. Escribe un programa donde los dos LEDs parpadeen a diferentes velocidades — uno aproximadamente el doble de rápido que el otro.

Antes de escribir cualquier código, piensa en cómo lograr dos velocidades diferentes usando un solo valor de retardo `for()` como unidad base. Después de que el programa esté funcionando, obsérvalo durante unos minutos. ¿La relación de temporización entre los dos LEDs se mantiene constante, o se desvía? Esta observación se conecta con por qué el enfoque de retardo por software tiene limitaciones reales en sistemas embebidos.

---

### Ejercicio 4.3 — Tres LEDs en secuencia

Conecta tres LEDs externos (representando rojo, amarillo y verde) a tres pines de GPIO diferentes. Configura los tres como salidas digitales push-pull. Escribe un programa que los encienda y apague en una secuencia fija con aproximadamente 3 a 5 segundos entre cada transición — suficiente tiempo para ver claramente cada estado antes de que comience el siguiente.

La secuencia debe ser: rojo encendido → rojo apagado, verde encendido → verde apagado, amarillo encendido → amarillo apagado, luego repetir. Antes de escribir el código, dibuja la secuencia en papel como un diagrama de temporización — qué LED está encendido en cada momento. Este diagrama es el primer paso hacia una máquina de estados, aunque el código todavía no use una.

---

### Ejercicio 4.4 — Leyendo un botón y controlando un LED

Configura el pin 13 de GPIOC (botón de la placa) como entrada digital con el resistor pull-up interno habilitado. Configura el pin 5 de GPIOA como salida push-pull. Escribe un programa dentro de `while(1){}` que lea el estado del botón a través del registro IDR y controle el LED en consecuencia — el LED debe estar encendido mientras el botón está presionado y apagado mientras está suelto.

Lee el registro IDR usando notación CMSIS y usa una operación AND bit a bit con la máscara apropiada para aislar el bit 13. Recuerda que la configuración pull-up significa que la lógica puede estar invertida respecto a lo que esperas. Verifica la lectura del botón en el visualizador SFR antes de confiar en ella para controlar el LED. Después de que el comportamiento básico funcione, observa cuidadosamente: ¿el LED parpadea o se comporta de manera inesperada cuando presionas o sueltas el botón? Anota tu observación — será relevante en la semana 05.

---

### Ejercicio 4.5 — Dos botones controlando la temporización del LED

Conecta un botón externo a un pin de GPIO de tu elección. Configura ambos botones como entradas digitales con resistores pull-up habilitados, y configura el LED de la placa como salida. Escribe un programa donde un botón aumente la velocidad de parpadeo del LED y el otro la disminuya.

Necesitarás una variable que almacene el valor de retardo actual y sea modificada por las pulsaciones de los botones. Piensa cuidadosamente en cómo estructurar la lectura de dos botones y la modificación de la variable de retardo dentro del bucle `while(1){}`. ¿Qué debe ocurrir si ambos botones se presionan al mismo tiempo? Define ese comportamiento explícitamente en tu código, aunque la elección sea arbitraria. Un comportamiento no definido en sistemas embebidos siempre es un error esperando ocurrir.

---

### Ejercicio 4.6 — Semáforo: primer enfoque con if/else

Usando los tres LEDs del ejercicio 4.3 (rojo, amarillo, verde), implementa una secuencia de semáforo usando únicamente sentencias `if/else` y una sola variable que rastree el estado actual como un número simple (0, 1, 2). La temporización para cada estado debe ser de aproximadamente 3 a 5 segundos.

La secuencia debe ser: verde (estado 0) → amarillo (estado 1) → rojo (estado 2) → de vuelta a verde. Al final del retardo de cada estado, incrementa la variable de estado y usa lógica `if/else` para decidir qué LEDs encender y apagar. Después de que el programa funcione correctamente, lee tu propio código de manera crítica. ¿Qué tan fácil es entender qué significan el estado 0, el estado 1 y el estado 2 solo leyendo los números? ¿Cómo añadirías un cuarto estado — por ejemplo, un amarillo intermitente para cruce peatonal? Escribe tu evaluación honesta como un comentario al inicio del archivo.

---

### Ejercicio 4.7 — Semáforo: refactorizado con switch-case

Toma el programa de semáforo del ejercicio 4.6 y reescríbelo usando una estructura `switch-case` en lugar de `if/else`. Sigue usando números simples (0, 1, 2) para los estados por ahora — no introduzcas `enum` todavía. El comportamiento visible del programa debe ser idéntico al del ejercicio 4.6.

Después de que la versión refactorizada funcione correctamente, compara las dos versiones una al lado de la otra. ¿Cómo se lee la versión `switch-case` comparada con la versión `if/else`? ¿Es más fácil o más difícil ver la estructura de la lógica del semáforo? Ahora piensa en añadir ese cuarto estado nuevamente — ¿es más fácil añadir un nuevo `case` que un nuevo `else if`? Escribe tu comparación como un comentario al inicio del archivo. Estás construyendo la intuición de por qué `switch-case` es la columna vertebral natural de una implementación de FSM.

---

### Ejercicio 4.8 — Semáforo: versión final con enum

Toma el programa de semáforo del ejercicio 4.7 y refactorízalo una vez más — esta vez introduciendo un `enum` para dar nombres significativos a los estados. Define la enumeración antes de `main()` siguiendo la convención de nomenclatura del curso. Reemplaza cada número en el `switch-case` con el valor de enum correspondiente.

Después de que la versión refactorizada funcione correctamente, lee tu código una vez más de principio a fin. ¿Puedes entender qué hace el programa sin ejecutarlo, solo leyendo los nombres de los estados y las etiquetas `case`? Ahora piensa en añadir ese cuarto estado una vez más — ¿el proceso es claro y limpio? Compara las tres versiones (4.6, 4.7, 4.8) y escribe un comentario final que responda esta pregunta: si tuvieras que entregar este código a otro ingeniero que nunca lo ha visto, ¿cuál versión le darías y por qué? Este es el argumento de ingeniería para usar `enum` y `switch-case` en implementaciones de FSM — no solo porque es buen estilo, sino porque hace que el código se documente a sí mismo y sea mantenible.

---

### Ejercicio 4.9 — FSM controlada por botón con dos estados

Diseña e implementa una FSM con dos estados que sea controlada por un botón. El estado A tiene el LED apagado. El estado B tiene el LED encendido. Cada vez que el botón se presiona y se suelta, el sistema transiciona de un estado al otro. Usa `enum` para las definiciones de estado y `switch-case` para la implementación de la máquina de estados desde el principio — no empieces con números simples esta vez.

Antes de escribir cualquier código, dibuja el diagrama de estados completo en papel: dos estados, las transiciones entre ellos y qué desencadena cada transición. Luego traduce el diagrama a código. Piensa cuidadosamente en cómo detectar un ciclo de presionar y soltar el botón en lugar de solo detectar que el botón está siendo mantenido presionado — la diferencia entre detectar una transición y detectar un nivel es un concepto importante en el manejo de entradas embebidas.

---

### Ejercicio 4.10 — FSM con tres estados y múltiples salidas

Diseña e implementa una FSM con al menos tres estados que use tanto botones como múltiples LEDs. El comportamiento específico es tuyo para definir — pero debe cumplir los siguientes requisitos: cada estado debe producir un patrón de salida distinto en los LEDs, al menos una transición debe ser desencadenada por un botón, al menos una transición debe ser desencadenada por el otro botón, y debe haber al menos un estado que tenga diferentes transiciones dependiendo de qué botón se presione.

Antes de escribir cualquier código, completa el diseño completo de la FSM en papel: diagrama de estados con todos los estados, todas las transiciones, todos los disparadores y todas las salidas para cada estado. Pide a un compañero que revise tu diagrama antes de empezar a codificar. Después de que la implementación esté completa y funcionando, verifica que tu código coincida exactamente con tu diagrama — si difieren, actualiza el que esté equivocado para que el diagrama y el código siempre cuenten la misma historia. En un proyecto embebido real, el documento de diseño y el código siempre deben contar la misma historia.
