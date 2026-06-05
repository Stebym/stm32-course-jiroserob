# Ejercicios de Practica — Semana 03
## Curso Taller V (Sistemas Embebidos STM32F4xx)
### Arquitectura del MCU y Programación Bare-Metal

En la semana 03, los ejercicios se alejan completamente de los fragmentos de código. Ahora se espera que leas el manual de referencia, identifiques los registros y campos de bits relevantes, y escribas las operaciones correctas tú mismo. El visualizador SFR en el depurador es tu principal herramienta de verificación — siempre verifica el estado del registro después de cada operación, no solo el resultado final.

Mantén el manual de referencia del STM32F4xx abierto junto a STM32CubeIDE en todo momento durante estos ejercicios. Si no estás seguro de qué registro usar, la respuesta está en la documentación — no en el asistente de IA, no en el código de un compañero. Búscala tú mismo primero.

Los ejercicios de esta semana siguen un camino incremental. Los primeros ejercicios se enfocan en registros individuales y operaciones aisladas. Los ejercicios posteriores combinan múltiples periféricos y registros en sistemas completos y funcionales. Para el ejercicio 10, tendrás una interacción real de entrada-salida funcionando en el microcontrolador, verificada tanto a través del visualizador SFR como mediante la observación física del hardware.

---

### Ejercicio 3.1 — Identificando periféricos en sus buses

Abre el manual de referencia del STM32F4xx y encuentra la sección que describe la arquitectura del sistema y las conexiones de bus. Para cada uno de los siguientes periféricos, identifica a qué bus está conectado (AHB1, AHB2, APB1 o APB2) y qué bit en el registro de habilitación RCC correspondiente controla su reloj: GPIOA, GPIOB, GPIOC, USART2, TIM2, SPI1.

No adivines — encuentra cada uno en la documentación. Escribe tus respuestas como una tabla pequeña. Este conocimiento es la base de todo lo que sigue: antes de poder usar cualquier periférico, debes saber dónde vive su bit de habilitación de reloj.

---

### Ejercicio 3.2 — Habilitando y deshabilitando relojes de periféricos

Escribe código que habilite los relojes para GPIOA y GPIOB, luego verifica en el visualizador SFR que ambos bits están establecidos. Luego escribe una línea que deshabilite el reloj de GPIOB usando una operación `&= ~()`. Verifica en el visualizador SFR que solo el bit de GPIOB cambió y el bit de GPIOA no fue afectado.

Este ejercicio prueba si tus operaciones de enmascaramiento son precisas — una máscara mal escrita puede deshabilitar accidentalmente un periférico que necesitas. Siempre verifica tanto lo que cambió como lo que no cambió.

---

### Ejercicio 3.3 — Leyendo el registro MODER

Habilita el reloj de GPIOA. Luego lee el valor actual de `GPIOA->MODER` en el visualizador SFR antes de escribir nada en él. Anota el valor que ves. Ahora busca el registro MODER en el manual de referencia y decodifica el valor de reset bit a bit — ¿en qué modo está cada pin al reiniciar?

Después de decodificar, configura el pin 5 como salida digital usando las operaciones de bits correctas. Verifica el cambio en el visualizador SFR y confirma que el modo de ningún otro pin fue afectado por tu operación. Recuerda: cada pin ocupa dos bits en MODER, así que sé preciso sobre qué posiciones de bits estás modificando.

---

### Ejercicio 3.4 — Explorando el conjunto completo de registros GPIO

Con el reloj de GPIOA habilitado y el pin 5 configurado como salida, explora cada uno de los siguientes registros en el visualizador SFR — leyendo sus valores de reset antes y después de escribir en ellos: OTYPER (tipo de salida), OSPEEDR (velocidad de salida), PUPDR (pull-up/pull-down), ODR (datos de salida). Para cada registro, busca su descripción en el manual de referencia antes de tocarlo.

Escribe un valor que encienda el LED, luego escribe un valor que lo apague. Verifica cada cambio en el visualizador SFR. El objetivo no es memorizar los registros — es sentirte cómodo navegando entre la documentación, el código y el visualizador SFR como un flujo de trabajo de depuración natural.

---

### Ejercicio 3.5 — Configurando un pin de entrada

Habilita el reloj para GPIOC. Configura el pin 13 (el botón de usuario en la placa Nucleo) como entrada digital con el resistor pull-up interno habilitado. Lee el manual de referencia para encontrar los valores correctos de MODER y PUPDR para esta configuración.

Después de configurar, lee el registro IDR en el visualizador SFR mientras presionas y sueltas el botón. ¿Qué cambia? ¿Cuál bit corresponde al pin 13? Piensa en la relación entre el estado físico del botón y el valor del bit que observas — y nota que la relación podría estar invertida respecto a lo que esperas, dependiendo de cómo está conectado el botón en la placa.

---

### Ejercicio 3.6 — Leyendo el estado completo del periférico en el visualizador SFR

Con ambos GPIOA y GPIOC configurados de los ejercicios anteriores, abre el visualizador SFR y expande ambos periféricos simultáneamente para poder ver todos sus registros a la vez — MODER, OTYPER, OSPEEDR, PUPDR, IDR, ODR — para ambos puertos al mismo tiempo.

Sin ejecutar ningún código nuevo, lee el estado actual de cada registro para ambos periféricos y responde las siguientes preguntas por escrito: ¿qué registros aún tienen sus valores de reset? ¿Cuáles han sido modificados por tus ejercicios anteriores? Para cada registro modificado, ¿puedes explicar por qué cada bit está en su valor actual? Este ejercicio trata de desarrollar el hábito de leer e interpretar el estado completo del periférico, no solo el registro que tocaste por última vez. Un buen desarrollador de sistemas embebidos siempre conoce el estado completo del hardware con el que está trabajando.

---

### Ejercicio 3.7 — Leer antes de escribir

Este ejercicio introduce una de las disciplinas más importantes en la programación embebida: siempre leer y entender el estado actual de un registro antes de modificarlo.

Habilita el reloj para GPIOB. Antes de escribir una sola línea que modifique `GPIOB->MODER`, abre el visualizador SFR y registra el valor de reset de ese registro. Ahora configura el pin 7 de GPIOB como salida digital. Después de escribir tu operación, regresa al visualizador SFR y verifica dos cosas: primero, que los bits 15 y 14 (que controlan el pin 7) ahora muestran el valor del modo de salida. Segundo, que cada otro campo de bits en el registro es idéntico al valor de reset que registraste antes. Si algún otro bit cambió, tu operación de enmascaramiento es incorrecta — regresa y corrígela antes de continuar.

---

### Ejercicio 3.8 — Combinando registros en la secuencia correcta

Configura tanto el pin 5 de GPIOA (salida — LED) como el pin 13 de GPIOC (entrada — botón) desde cero en una secuencia única y limpia. Escribe las operaciones en el orden correcto: primero las habilitaciones de reloj, luego las configuraciones de modo, luego la configuración de pull-up/pull-down. No copies código de ejercicios anteriores — escribe todo de nuevo desde la memoria y desde el manual de referencia.

Después de escribir la secuencia completa de configuración, recorre cada línea una a la vez en el depurador y verifica en el visualizador SFR que cada registro cambia correctamente después de cada línea. El objetivo es practicar la escritura de una secuencia completa de inicialización de periféricos y desarrollar el hábito de verificar cada paso individualmente en lugar de ejecutar todo de una vez y esperar que funcione.

---

### Ejercicio 3.9 — Conectando entrada con salida: el botón controla el LED usando BSRR

Con el pin 5 de GPIOA configurado como salida y el pin 13 de GPIOC configurado como entrada con pull-up, escribe un programa que lea el estado del botón continuamente dentro del bucle `while(1){}` y controle el LED en consecuencia. Usa el registro BSRR para encender y apagar el LED — no el registro ODR.

Busca el registro BSRR en el manual de referencia cuidadosamente antes de escribir cualquier código. Entiende cómo están organizados sus campos de bits y cómo difiere de ODR en la forma en que escribes en él. Después de que tu programa esté funcionando y el LED siga correctamente el estado del botón, implementa el mismo comportamiento una segunda vez usando ODR en lugar de BSRR — puedes comentar la versión con BSRR y escribir la versión con ODR debajo.

Una vez que ambas versiones funcionen correctamente, escribe una descripción breve — en comentarios dentro de tu código — respondiendo la siguiente pregunta: ¿cuál es la diferencia práctica entre controlar el LED a través de ODR versus a través de BSRR? Piensa en qué ocurre a nivel de registro cuando escribes en cada uno, y qué podría salir mal con ODR en un sistema más complejo donde múltiples partes del código intentan controlar diferentes pines del mismo puerto simultáneamente.

---

### Ejercicio 3.10 — Observando el sistema en vivo en el visualizador SFR

Con tu programa botón-LED del ejercicio 3.9 ejecutándose en modo de depuración, configura el visualizador SFR para mostrar los registros de GPIOA y GPIOC simultáneamente. Ahora, en lugar de recorrer el código línea por línea, deja que el programa se ejecute libremente y observa el visualizador SFR mientras presionas y sueltas físicamente el botón en la placa Nucleo.

Observa el registro IDR de GPIOC — específicamente el bit 13 — mientras presionas y sueltas el botón. Luego observa el registro ODR de GPIOA — específicamente el bit 5 — y observa cómo responde al estado del botón. Nota el tiempo de respuesta: ¿el LED responde inmediatamente? ¿Hay algún comportamiento que parezca inconsistente o inesperado, como el LED parpadeando ligeramente? Si observas un comportamiento inesperado, piensa en qué podría causarlo — la respuesta se conecta directamente con un concepto que estudiarás en la semana 05.

Este ejercicio trata de aprender a leer un sistema en ejecución en tiempo real a través del visualizador SFR, que es una de las habilidades de depuración más poderosas en el desarrollo de sistemas embebidos. El hardware te está hablando a través de esos valores de registro — tu trabajo es aprender a escuchar.
