# Configuración de tu repositorio personal

Este documento te guía paso a paso para crear tu propio repositorio en GitHub y conectarlo con el repositorio oficial del curso. Al finalizar, tendrás un espacio de trabajo personal donde guardarás todo tu código durante el semestre, completamente integrado con el sistema de control de versiones que usarás como un ingeniero profesional.

---

## Antes de comenzar

Asegúrate de haber completado la verificación de herramientas (`VERIFICACION_HERRAMIENTAS.md`) y de que el siguiente comando responde con tu nombre de usuario de GitHub:

```bash
ssh -T git@github.com
```

Si ves `Hi tuusuario! You've successfully authenticated`, estás listo para continuar. Si ves un error, regresa al documento de verificación y resuelve el problema de SSH antes de continuar — todos los pasos que siguen dependen de que esta conexión funcione correctamente.

---

## El modelo mental: dos remotos, un repositorio local

Antes de escribir cualquier comando, es importante entender la arquitectura que vas a construir. Trabaja con tres actores simultáneamente, y confundirlos es el error más común en este proceso.

El **repositorio del profesor** (`upstream`) es el repositorio oficial del curso, alojado en la cuenta de GitHub del profesor. Aquí es donde aparecerán las nuevas semanas a medida que avance el curso. Tú nunca escribes en este repositorio — solo lees de él para obtener el material nuevo.

Tu **repositorio personal en GitHub** (`origin`) es una copia tuya, alojada en tu propia cuenta de GitHub. Aquí es donde guardas tu trabajo permanentemente en la nube. Solo tú escribes aquí — es tu portafolio del curso.

Tu **máquina local** es donde ocurre todo el trabajo real: escribes código, compilas, depuras, haces commits. Es el puente entre los dos repositorios remotos: trae el material nuevo desde `upstream` y envía tu trabajo hacia `origin`.

La regla de oro que debes memorizar es esta: **siempre traes (`fetch/pull`) desde `upstream`, y siempre envías (`push`) hacia `origin`**. Nunca al revés.

---

## Paso 1 — Crea tu repositorio vacío en GitHub

Abre GitHub en tu navegador e inicia sesión en tu cuenta. Crea un nuevo repositorio haciendo clic en el botón **"New"** en tu página principal.

Configura el nuevo repositorio con los siguientes parámetros:

- **Repository name:** `stm32-course-usuariounal` (reemplaza `usuariounal` con tu usuario del correo de la Universidad Nacional, sin espacios, sin mayúsculas ni caracteres especiales)
- **Visibility:** Public
- **Initialize this repository with:** — deja todas las opciones **sin marcar**

Este último punto es crítico. Si GitHub agrega automáticamente un `README`, un `.gitignore` o una licencia, el repositorio ya no estará verdaderamente vacío y el primer `push` generará un conflicto. Necesitas un repositorio **completamente en blanco** para que los pasos siguientes funcionen sin problemas.

Haz clic en **"Create repository"** y copia la dirección SSH que GitHub te muestra — se verá así:

```
git@github.com:tuusuario/stm32-course-usuariounal.git
```

---

## Paso 2 — Clona tu repositorio vacío en tu máquina local

Abre una terminal y navega a la carpeta donde guardarás el trabajo del curso. Se recomienda usar una carpeta `github` en tu directorio personal para mantener todo organizado:

```bash
# Crea la carpeta de trabajo si no existe todavía
mkdir -p ~/github
cd ~/github

# Clona tu repositorio vacío (reemplaza con tu URL real)
git clone git@github.com:tuusuario/stm32-course-usuarioUnal.git

# Entra a la carpeta del repositorio
cd stm32-course-usuariounal
```

Git mostrará una advertencia diciendo que has clonado un repositorio vacío — eso es completamente normal y esperado. Verifica que estás en el lugar correcto:

```bash
# Muestra el directorio actual
pwd

# Muestra el estado del repositorio
git status
```

---

## Paso 3 — Agrega el repositorio del profesor como remoto `upstream`

Ahora vas a conectar tu repositorio local con el repositorio oficial del curso. Este es el paso que establece el canal por donde recibirás el material de cada semana:

```bash
# Agrega el repositorio del profesor como remoto llamado "upstream"
git remote add upstream git@github.com:namontoy/stm32-embedded-course.git
```

Verifica que ambos remotos quedaron correctamente registrados:

```bash
git remote -v
```

Deberías ver exactamente cuatro líneas — dos para `origin` (tu repositorio) y dos para `upstream` (el repositorio del profesor):

```
origin    git@github.com:tuusuario/stm32-course-usuariounal.git (fetch)
origin    git@github.com:tuusuario/stm32-course-usuariounal.git (push)
upstream  git@github.com:namontoy/stm32-embedded-course.git (fetch)
upstream  git@github.com:namontoy/stm32-embedded-course.git (push)
```

Aunque `upstream` aparece con una dirección de `push`, en la práctica nunca ejecutarás `git push upstream` — no tienes permiso de escritura en el repositorio del profesor, y hacerlo sería un error.

---

## Paso 4 — Obtén la semana 0 desde el repositorio del profesor

Con los remotos configurados, ya puedes traer el primer material del curso. La semana 0 contiene los documentos de referencia que usarás durante todo el semestre:

```bash
# Descarga la rama week-00 desde el repositorio del profesor
git fetch upstream week-00

# Crea una rama local llamada week-00 que apunta al contenido descargado
git checkout -b week-00 upstream/week-00
```

Verifica que los archivos llegaron correctamente:

```bash
ls -la
```

Deberías ver los documentos de referencia del curso, incluyendo `VERIFICACION_HERRAMIENTAS.md` y `CHECKLIST_SEMANAL.md`.

---

## Paso 5 — Sube la semana 0 a tu repositorio personal en GitHub

El último paso es enviar este contenido a tu propio GitHub, para que quede guardado en la nube y puedas acceder desde cualquier máquina:

```bash
# Envía la rama week-00 a tu repositorio personal
git push origin week-00
```

Abre tu repositorio en GitHub y verifica que la rama `week-00` aparece con los archivos del curso. Si la ves, la configuración está completa.

---

## Verificación final

Puedes confirmar el estado completo de tu repositorio con estos dos comandos:

```bash
# Muestra todas las ramas locales y remotas disponibles
git branch -a

# Muestra el historial de commits de la rama actual
git log --oneline
```

Si todo está correcto, verás la rama `week-00` tanto localmente como en `origin`, y el historial mostrará los commits iniciales del curso.

---

## ¿Qué viene ahora?

Esta configuración es todo lo que necesitas hacer una sola vez. A partir de aquí, cada semana seguirás el procedimiento descrito en `CHECKLIST_SEMANAL.md`: el profesor publicará una nueva rama en su repositorio, tú la traerás con `git fetch upstream week-XX`, la crearás localmente, subirás tu trabajo anterior y continuarás construyendo sobre lo que ya tienes.

Tu código crecerá semana a semana, acumulando funcionalidad exactamente como lo hace un proyecto de firmware profesional — no borrando y empezando de nuevo, sino evolucionando con control y trazabilidad completa. Eso es precisamente lo que git fue diseñado para hacer.

Si algo no funciona como se describe en este documento, no intentes "arreglarlo" con comandos que encontraste en internet sin entenderlos primero. Git es muy poderoso pero también muy literal — un comando equivocado puede hacer las cosas más difíciles de recuperar. Consulta con tu profesor antes de continuar.
