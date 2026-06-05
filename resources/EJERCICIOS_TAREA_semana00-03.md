# Ejercicios de Tarea — Semanas 00 a 03
## Curso de Sistemas Embebidos STM32F4xx

Este documento contiene los ejercicios para las semanas 00 a 03. Debes trabajarlos en orden, a tu propio ritmo, continuando desde donde quedó la sesión de taller. Todos los ejercicios se ejecutan directamente en el microcontrolador STM32F4xx conectado a tu portátil a través de STM32CubeIDE. No hay `printf` ni salida serial — el depurador es tu única ventana a lo que está haciendo el microcontrolador, y aprender a usarlo bien es en sí mismo uno de los objetivos de estos ejercicios.

Antes de ejecutar cualquier fragmento de código, predice siempre el resultado primero. Escribe tu predicción en papel o en un comentario. Luego ejecuta el código y compara lo que ves en el depurador con lo que esperabas. Si coinciden, perfecto — lo entendiste. Si no coinciden, mejor aún — tienes algo real que investigar y aprender. Nunca omitas el paso de predicción.

Trabaja con tus compañeros. Discute tus predicciones, compara tus resultados y ayúdense mutuamente cuando se atoren. Si necesitas orientación, usa el asistente de enseñanza con IA — pero llega siempre con tu propio intento primero.

---

## Semana 00 — Sistemas Numéricos, Tamaños de Variables y Representaciones

En los ejercicios de esta semana, todo lo que haces sigue el mismo patrón simple: declarar una variable, asignar un valor, poner un breakpoint en la siguiente línea e inspeccionar la variable en el depurador. La memoria del microcontrolador es el laboratorio, y el depurador es tu instrumento. Presta atención a las opciones de formato de visualización en el depurador — puedes ver la misma variable en decimal, hexadecimal y binario, y cambiar entre ellos te enseñará mucho.

### Ejercicio 0.1 — Primer contacto con el depurador

Crea un nuevo proyecto en STM32CubeIDE sin usar la generación de código de CubeMX. El IDE generará un proyecto esqueleto con un archivo `main.c`. Lo primero que siempre debes hacer es reemplazar el bucle `for(;;)` generado automáticamente por `while(1){}` — así es como escribimos un bucle infinito en este curso.

Dentro de `main()`, antes del `while(1){}`, declara la siguiente variable y coloca un breakpoint en la línea inmediatamente después:

```c
uint8_t my_variable = 42;
```

Compila, flashea y ejecuta el proyecto en modo de depuración. Cuando la ejecución se detenga en tu breakpoint, encuentra `my_variable` en el inspector de variables y verifica que contiene el valor 42. Este ejercicio trata de una sola cosa: asegurarte de que tu flujo de trabajo completo — escribir, compilar, flashear, depurar, inspeccionar — funciona correctamente. Si ves 42 en el inspector de variables, estás listo para continuar.

### Ejercicio 0.2 — El mismo número, diferentes representaciones

Declara las siguientes tres variables:

```c
uint8_t dec = 65;
uint8_t hex = 0x41;
uint8_t bin = 0b01000001;
```

Antes de ejecutar el código, predice: ¿estas tres variables contienen el mismo valor o valores diferentes? Ejecuta el código, coloca un breakpoint después de las declaraciones e inspecciona las tres en el depurador. Luego intenta cambiar el formato de visualización de cada variable entre decimal, hexadecimal y binario. Observa cómo el mismo valor subyacente se ve diferente según la representación. Piensa en esto: si el microcontrolador solo trabaja con bits, ¿qué significa que puedas escribir el mismo valor de tres maneras diferentes en tu código fuente?

### Ejercicio 0.3 — ¿Qué tan grande es una variable?

Declara las siguientes variables y predice qué valor tendrá cada una antes de ejecutar el código:

```c
uint8_t  a = 255;
uint16_t b = 255;
uint32_t c = 255;
uint8_t  d = 256;
```

Inspecciona las cuatro en el depurador. Una de ellas te sorprenderá. Después de verificar el resultado, agrega una variable más:

```c
uint8_t  e = 257;
```

¿Qué valor tiene `e`? ¿Por qué? Piensa en qué significa que el tipo de variable define el "tamaño del contenedor" y qué ocurre cuando el valor que quieres almacenar es más grande de lo que el contenedor puede contener.

### Ejercicio 0.4 — El signo importa

Declara las siguientes variables:

```c
uint8_t a = 200;
int8_t  b = 200;
int8_t  c = -1;
```

Antes de ejecutar, predice el valor de cada variable en el depurador. Ejecuta el código e inspecciona los resultados. Al menos uno será diferente de lo que podrías esperar. Después de inspeccionar los valores en decimal, cambia el formato de visualización de la variable `c` a hexadecimal. ¿Qué ves? Piensa por qué -1 tiene esa representación hexadecimal particular en una variable de 8 bits con signo. Esto es el complemento a 2 en acción — el mismo concepto que estudiaste en la clase de teoría, ahora visible dentro del hardware real.

### Ejercicio 0.5 — Límites y desbordamiento

Escribe el siguiente código y recórrelo línea por línea en el depurador, inspeccionando la variable después de cada paso:

```c
int8_t  x = 127;
x = x + 1;

uint8_t y = 255;
y = y + 1;
```

¿Qué le pasa a `x` después de sumarle 1 a 127? ¿Qué le pasa a `y` después de sumarle 1 a 255? Después de observar ambos resultados, explora las constantes de límite que proporciona `stdint.h`. Declara variables y asígnales los valores `INT8_MAX`, `INT8_MIN`, `UINT8_MAX` y `UINT16_MAX`, luego inspecciona cada una en el depurador. Construye una tabla mental de los valores mínimo y máximo para cada tipo. Este conocimiento no es opcional en sistemas embebidos — elegir el tipo incorrecto para una variable es una fuente real de errores en productos reales.

### Ejercicio 0.6 — Aritmética mental en hexadecimal

Declara `uint8_t result;` y escribe las siguientes asignaciones una a la vez. Antes de cada una, escribe tu predicción en papel. Luego ejecuta y verifica en el depurador:

```c
result = 0x0F + 0x01;
result = 0xFF + 0x01;
result = 0xA0 + 0x5F;
result = 0xA0 + 0x60;
```

Para cada resultado, pregúntate: ¿la suma se desbordó? ¿Cómo puedes saberlo solo con el valor hexadecimal? Si tus predicciones estuvieron equivocadas, regresa y trabaja la suma a mano en binario para entender dónde falló tu razonamiento. El depurador da retroalimentación honesta e inmediata — úsalo.

### Ejercicio 0.7 — Construyendo un número bit a bit

Comienza con `uint8_t x = 0;` y escribe las siguientes asignaciones una a la vez, colocando un breakpoint después de cada una e inspeccionando `x` en formato de visualización binario antes de pasar a la siguiente:

```c
x = 0x01;
x = 0x02;
x = 0x04;
x = 0x08;
x = 0x10;
x = 0x20;
x = 0x40;
x = 0x80;
```

El patrón que emerge — cada paso duplica el valor anterior — conecta directamente con la experiencia de simulación de registros de desplazamiento de la semana 00 de teoría, y anticipa el operador de desplazamiento que llega en la semana 01. El objetivo es internalizar la relación entre los dígitos hexadecimales y las posiciones individuales de los bits.

### Ejercicio 0.8 — El operador bang y la lógica booleana

Declara las siguientes variables y escribe tu predicción para cada resultado antes de ejecutar el código:

```c
uint8_t a = 5;
uint8_t b = 0;
uint8_t c = 255;

uint8_t r1 = !a;
uint8_t r2 = !b;
uint8_t r3 = !c;
uint8_t r4 = ~c;
```

Inspecciona los cuatro resultados en el depurador. Presta especial atención a `r3` y `r4` — se calculan a partir de la misma variable `c`, pero usando operadores diferentes. ¿Son iguales los resultados? Ahora agrega un experimento más:

```c
uint8_t r5 = ~a;
```

Compara `r1` (que es `!a`) con `r5` (que es `~a`). ¿Son iguales? La diferencia entre lo que observas aquí es una de las distinciones más importantes en la programación C para sistemas embebidos. Asegúrate de poder explicar con tus propias palabras qué hace `!` versus qué hace `~`, y por qué producen resultados diferentes cuando se aplican a un valor como 5.

---

## Semana 01 — Fundamentos del Lenguaje C

Todos los ejercicios de esta semana continúan usando el depurador como única herramienta de verificación. El patrón es el mismo que la semana 00: predecir, ejecutar, verificar, entender. El foco cambia de las representaciones numéricas a las operaciones aritméticas, los operadores de desplazamiento y las estructuras de control. Algunos ejercicios incluyen código para escribir directamente. Otros describen lo que debe hacer el código — tú debes escribirlo tú mismo.

### Ejercicio 1.1 — Operadores aritméticos y división entera

Declara las siguientes variables e inspecciona cada resultado en el depurador:

```c
uint8_t a          = 17;
uint8_t b          = 5;
uint8_t div_result = a / b;
uint8_t mod_result = a % b;
uint8_t mul_result = a * b;
```

Antes de ejecutar, predice cada resultado. Presta especial atención a `div_result` — C realiza división entera, lo que significa que la parte decimal simplemente se descarta. Después de verificar, piensa en la relación entre `div_result` y `mod_result`: si multiplicas `div_result` por `b` y le sumas `mod_result`, ¿qué obtienes? Verifica tu respuesta en el depurador.

### Ejercicio 1.2 — Desbordamiento en aritmética

Declara las siguientes variables y predice cada resultado antes de ejecutar:

```c
uint8_t x   = 200;
uint8_t y   = 100;
uint8_t sum = x + y;
```

El resultado no será 300. ¿Por qué no? Después de verificar en el depurador, piensa en esto: si `sum` es incorrecta y tu programa la usa para tomar una decisión — por ejemplo, para verificar si un valor de sensor superó un umbral de seguridad — ¿qué podría ocurrir en un sistema real? Este no es un problema hipotético. Los errores relacionados con desbordamiento han causado fallas reales en sistemas embebidos reales.

### Ejercicio 1.3 — Operadores de desplazamiento como multiplicación y división

Declara `uint8_t val = 3;` y luego declara las siguientes variables, prediciendo cada resultado antes de ejecutar:

```c
uint8_t left1  = val << 1;
uint8_t left2  = val << 2;
uint8_t left3  = val << 3;
uint8_t right1 = val >> 1;
```

Inspecciona cada resultado en formato de visualización binario para ver los bits moviéndose literalmente hacia la izquierda y la derecha. Luego prueba el caso límite: ¿qué ocurre cuando desplazas un bit fuera del extremo de la variable? Escribe una variable inicializada a `0b10000000` y desplázala a la izquierda 1 posición. ¿Qué observas?

### Ejercicio 1.4 — Evaluación booleana e if/else

Escribe un programa que declare tres variables con diferentes valores, luego use sentencias `if` e `if-else` para establecer una variable `result` basándose en condiciones. La lógica debe probar al menos un valor distinto de cero como condición, un valor cero como condición y una comparación de igualdad. Coloca un breakpoint después de cada bloque `if` y observa cómo cambia `result` en el depurador mientras recorres el código.

Piensa cuidadosamente en cómo C decide si una condición es verdadera o falsa. ¿Cuál es el único valor que C considera falso? ¿Qué te dice eso sobre cómo el lenguaje trata cualquier número distinto de cero?

### Ejercicio 1.5 — El bucle for como contador

Escribe un bucle `for` que cuente de 0 a 9 e incremente una variable llamada `counter` en cada iteración. Coloca un breakpoint después de que el bucle termine y verifica que `counter` tiene el valor que esperas. Luego modifica el bucle de tres maneras diferentes, una a la vez: haz que cuente en pasos de 2, haz que cuente hacia atrás de 10 a 1, y haz que se detenga en un valor diferente. Para cada modificación, predice el valor final de `counter` antes de ejecutar.

### Ejercicio 1.6 — El bucle while y un acumulador simple

Escribe un bucle `while` que sume todos los enteros del 1 al 10 y almacene el total acumulado en una variable llamada `sum`. Antes de ejecutar, calcula el resultado esperado en papel. Después de verificar en el depurador, modifica el ejercicio: cambia el tipo de dato de `sum` a `uint16_t` y suma todos los enteros del 1 al 100. ¿Cuál es el resultado esperado? ¿Cabría en un `uint8_t`? Verifica tu respuesta.

### Ejercicio 1.7 — El bucle do-while

Escribe dos piezas de código separadas una al lado de la otra. La primera usa un bucle `while` con una condición que es falsa desde el inicio. La segunda usa un bucle `do-while` con la misma condición falsa. Ambos bucles, si se ejecutan, deben establecer una variable resultado en 42. Predice cuál variable resultado será 42 y cuál permanecerá en 0. Verifica en el depurador. Con tus propias palabras, describe la diferencia fundamental entre `while` y `do-while`.

### Ejercicio 1.8 — El switch-case como árbol de decisiones

Escribe una estructura `switch-case` que mapee una variable `input` a una variable `output` según al menos cuatro casos diferentes, con un caso `default` para cualquier valor que no coincida. Establece `input` a diferentes valores, recompila y verifica en el depurador que `output` cambia correctamente cada vez. Luego elimina deliberadamente una de las sentencias `break` y observa qué ocurre — este comportamiento se llama "fall-through" y es importante entenderlo, aunque casi nunca quieras que ocurra accidentalmente.

---

## Semana 02 — Operadores Bit a Bit e Introducción a las FSM

Los ejercicios de esta semana tienen dos pistas. La primera pista es práctica en el IDE — aplicar los operadores lógicos bit a bit a variables y luego a registros de hardware reales. La segunda pista es trabajo de diseño en papel — construir diagramas de estados FSM para sistemas del mundo real. Ambas pistas son igualmente importantes. Los ejercicios de código construyen las herramientas; los ejercicios de FSM construyen la disciplina para usarlas bien.

En esta semana, se proporcionan fragmentos de código para los ejercicios fundamentales, pero los ejercicios posteriores se vuelven más descriptivos. Lee la descripción cuidadosamente, entiende lo que se pide y escribe el código tú mismo.

### Ejercicio 2.1 — Operador AND: la máscara que revela

Escribe y ejecuta el siguiente código, prediciendo el resultado antes de ejecutar:

```c
uint8_t value  = 0b10110101;
uint8_t mask   = 0b00001111;
uint8_t result = value & mask;
```

Inspecciona `result` en formato de visualización binario. El operador AND conserva solo los bits donde la máscara tiene un 1 y fuerza todos los demás bits a 0 — actúa como una ventana que revela bits específicos mientras oculta otros. Después de verificar, responde esta pregunta en papel antes de continuar: ¿qué máscara usarías para extraer solo los 4 bits superiores (bits 7 a 4)? Escribe el código y verifica tu respuesta.

### Ejercicio 2.2 — Operador OR: la máscara que establece

Escribe y ejecuta el siguiente código, prediciendo el resultado:

```c
uint8_t value  = 0b10100000;
uint8_t mask   = 0b00000101;
uint8_t result = value | mask;
```

Inspecciona `result` en vista binaria. Responde estas dos preguntas antes de continuar: si un bit ya es 1 y le aplicas OR con 1, ¿qué le pasa a ese bit? Si un bit es 0 y le aplicas OR con 0, ¿qué ocurre? Tus respuestas explican por qué `|=` es la forma segura de establecer bits en un registro de hardware sin perturbar los bits que no tenías intención de cambiar.

### Ejercicio 2.3 — Operador NOT: el complemento

Escribe y ejecuta el siguiente código, prediciendo cada resultado:

```c
uint8_t a       = 0b00001111;
uint8_t result1 = ~a;

uint8_t b       = 0b10100101;
uint8_t result2 = ~b;
```

Después de verificar, construye el patrón de limpieza de bits paso a paso. Declara `uint8_t value = 0b11111111;` y luego limpia los bits 0 y 1 sin cambiar ningún otro bit. Haz esto usando el patrón `&= ~()`. Antes de escribir el código, trabaja la máscara y su complemento en papel primero, luego verifica tu resultado en el depurador.

### Ejercicio 2.4 — Operador XOR: el toggle

Escribe y ejecuta el siguiente código, prediciendo cada resultado:

```c
uint8_t value   = 0b10110011;
uint8_t mask    = 0b00001111;
uint8_t result1 = value ^ mask;
uint8_t result2 = result1 ^ mask;
```

¿Qué tiene de especial `result2`? XOR aplicado dos veces con la misma máscara siempre regresa al valor original — XOR es su propio inverso. Piensa en qué significa esta propiedad para un problema práctico: si quisieras cambiar un bit entre 0 y 1 repetidamente, ¿cómo usarías XOR para hacerlo con una sola operación?

### Ejercicio 2.5 — Combinando operadores: el patrón completo

Declara `uint8_t simulated_register = 0x00;` y luego realiza la siguiente secuencia de operaciones, colocando un breakpoint después de cada una y verificando el resultado en vista binaria antes de pasar a la siguiente:

- Establece los bits 3 y 4 simultáneamente usando una sola operación `|=`
- Limpia el bit 3 sin cambiar el bit 4 usando `&= ~()`
- Cambia el estado del bit 4 usando `^=`
- Cambia el estado del bit 4 nuevamente usando `^=`

Escribe cada operación tú mismo — no busques la sintaxis. Si no estás seguro, razona a partir de lo que sabes sobre cada operador. Después de completar la secuencia, lee el comentario en tu código: esta variable se comporta exactamente como un registro de hardware. La única diferencia en el siguiente ejercicio es que el registro está conectado al silicio real.

### Ejercicio 2.6 — Primer registro real: habilitando el reloj

Abre el manual de referencia del STM32F4xx y navega a la sección RCC (Reset and Clock Control). Encuentra el registro AHB1ENR e identifica cuál bit habilita el reloj para GPIOA. Luego escribe una sola línea de código que habilite ese reloj usando el nombre de constante definido por CMSIS — no uses un número directamente. Después de ejecutar la línea, abre el visualizador SFR en el depurador, navega a RCC → AHB1ENR y confirma que el bit correcto cambió de estado. Esta es la misma operación `|=` del ejercicio 2.5 — la única diferencia es que este registro controla hardware real.

### Ejercicio 2.7 — Primer LED en GPIOA Pin 5

Con el reloj de GPIOA ya habilitado del ejercicio 2.6, configura el pin 5 como salida digital y enciende el LED de la placa. Necesitarás escribir en dos registros: el registro MODER para establecer el modo del pin y el registro ODR para establecer el valor de salida. Las posiciones exactas de los bits están descritas en el manual de referencia — búscalas. Después de cada escritura en un registro, verifica en el visualizador SFR antes de revisar el LED físico en tu placa Nucleo.

Este es el momento donde todo lo que has aprendido se conecta: las operaciones bit a bit de esta semana, las representaciones numéricas de la semana 00 y la idea de que el hardware se controla mediante bits en registros. Si el LED se enciende, has entendido todo correctamente.

### Ejercicio 2.8 — Diseño de FSM: el semáforo peatonal

Este ejercicio no requiere código — solo pensamiento y papel. Diseña una Máquina de Estados Finitos para un sistema de semáforo peatonal. El sistema controla dos conjuntos de luces simultáneamente: uno para vehículos (rojo, amarillo, verde) y uno para peatones (caminar, no caminar). Para tu diseño, identifica todos los estados posibles en los que puede estar el sistema, dibuja un diagrama de estados completo con transiciones etiquetadas entre estados y describe en una oración qué desencadena cada transición y cuáles son las salidas (qué luces están encendidas) en cada estado.

Un estado está definido por las salidas que produce. Si dos momentos en la vida del sistema producen salidas diferentes, son estados diferentes. Antes de finalizar tu diagrama, verifícalo leyendo cada transición en voz alta: "el sistema está en el estado X, y cuando ocurre Y, pasa al estado Z." Si alguna transición no lleva a ningún lado, o si algún estado no tiene forma de salir, tu diseño tiene un vacío.

### Ejercicio 2.9 — Diseño de FSM: la máquina expendedora

Diseña una Máquina de Estados Finitos para una máquina expendedora simple. La máquina acepta monedas de una en una y dispensa un producto cuando se han insertado suficientes monedas. Tu diseño debe manejar no solo el escenario esperado — el usuario inserta el número correcto de monedas y recibe el producto — sino también los inesperados: ¿qué ocurre si el usuario inserta demasiadas monedas? ¿Y si presiona cancelar antes de insertar suficientes? ¿Y si el producto no está disponible?

Una máquina de estados que solo maneja el camino esperado es un diseño incompleto. En sistemas embebidos, el hardware puede recibir entradas inesperadas en cualquier momento, y el software debe responder de manera sensata en cada estado a cada posible entrada. Cada estado en tu diagrama debe tener una respuesta definida para cada posible entrada — incluso si esa respuesta es "no hacer nada y permanecer en el estado actual."

---

## Semana 03 — Arquitectura del MCU y Programación Bare-Metal

En la semana 03, los ejercicios se alejan completamente de los fragmentos de código. Ahora se espera que leas el manual de referencia, identifiques los registros y campos de bits relevantes, y escribas las operaciones correctas tú mismo. El visualizador SFR en el depurador es tu principal herramienta de verificación — siempre verifica el estado del registro después de cada operación, no solo el resultado final.

Mantén el manual de referencia del STM32F4xx abierto junto a STM32CubeIDE en todo momento durante estos ejercicios. Si no estás seguro de qué registro usar, la respuesta está en la documentación — no en el asistente de IA, no en el código de un compañero. Búscala tú mismo primero.

### Ejercicio 3.1 — Identificando periféricos en sus buses

Abre el manual de referencia del STM32F4xx y encuentra la sección que describe la arquitectura del sistema y las conexiones de bus. Para cada uno de los siguientes periféricos, identifica a qué bus está conectado (AHB1, AHB2, APB1 o APB2) y qué bit en el registro de habilitación RCC correspondiente controla su reloj: GPIOA, GPIOB, GPIOC, USART2, TIM2, SPI1.

No adivines — encuentra cada uno en la documentación. Escribe tus respuestas como una tabla pequeña. Este conocimiento es la base de todo lo que sigue: antes de poder usar cualquier periférico, debes saber dónde vive su bit de habilitación de reloj.

### Ejercicio 3.2 — Habilitando y deshabilitando relojes de periféricos

Escribe código que habilite los relojes para GPIOA y GPIOB, luego verifica en el visualizador SFR que ambos bits están establecidos. Luego escribe una línea que deshabilite el reloj de GPIOB usando una operación `&= ~()`. Verifica en el visualizador SFR que solo el bit de GPIOB cambió y el bit de GPIOA no fue afectado. Este ejercicio prueba si tus operaciones de enmascaramiento son precisas — una máscara mal escrita puede deshabilitar accidentalmente un periférico que necesitas.

### Ejercicio 3.3 — Leyendo el registro MODER

Habilita el reloj de GPIOA. Luego lee el valor actual de `GPIOA->MODER` en el visualizador SFR antes de escribir nada en él. Anota el valor que ves. Ahora busca el registro MODER en el manual de referencia y decodifica el valor de reset bit a bit — ¿en qué modo está cada pin al reiniciar? Después de decodificar, configura el pin 5 como salida digital usando las operaciones de bits correctas. Verifica el cambio en el visualizador SFR y confirma que el modo de ningún otro pin fue afectado por tu operación.

### Ejercicio 3.4 — Explorando el conjunto completo de registros GPIO

Con el reloj de GPIOA habilitado y el pin 5 configurado como salida, explora cada uno de los siguientes registros en el visualizador SFR — leyendo sus valores de reset antes y después de escribir en ellos: OTYPER (tipo de salida), OSPEEDR (velocidad de salida), PUPDR (pull-up/pull-down), ODR (datos de salida). Para cada registro, busca su descripción en el manual de referencia antes de tocarlo. Escribe un valor que encienda el LED, luego escribe un valor que lo apague. Verifica cada cambio en el visualizador SFR. El objetivo no es memorizar los registros — es sentirte cómodo navegando entre la documentación, el código y el visualizador SFR como un flujo de trabajo de depuración natural.

### Ejercicio 3.5 — Configurando un pin de entrada

Habilita el reloj para GPIOC. Configura el pin 13 (el botón de usuario en la placa Nucleo) como entrada digital con el resistor pull-up interno habilitado. Lee el manual de referencia para encontrar los valores correctos de MODER y PUPDR para esta configuración. Después de configurar, lee el registro IDR en el visualizador SFR mientras presionas y sueltas el botón. ¿Qué cambia? ¿Cuál bit corresponde al pin 13? Piensa en la relación entre el estado físico del botón y el valor del bit que observas — y nota que la relación podría estar invertida respecto a lo que esperas, dependiendo de cómo está conectado el botón en la placa.
