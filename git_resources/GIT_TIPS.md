# Git — Consejos y trucos para el curso

Este documento es tu referencia rápida para los comandos y situaciones más comunes que encontrarás trabajando con git durante el semestre. No es necesario memorizar todo de una vez — úsalo como un manual de consulta cada vez que algo no salga como esperabas, o cuando quieras entender mejor lo que está pasando en tu repositorio.

Los comandos están organizados por tema. Lee cada explicación antes de ejecutar cualquier comando — entender qué hace un comando es tan importante como saber cuándo usarlo.

---

## 1. La regla de oro

Cada vez que necesites traer contenido desde el repositorio del profesor, sigue siempre este orden:

**`git fetch` → inspeccionar → `git merge`**

Nunca uses `git pull` directamente. `git fetch` descarga los cambios pero los deja en un área de espera sin tocar tu código. `git pull` hace las dos cosas de forma automática, sin darte la oportunidad de revisar qué llegó. En un proyecto donde tu código es valioso, siempre quieres saber qué estás integrando antes de que suceda.

---

## 2. La receta semanal completa

Este es el procedimiento completo para incorporar una nueva semana desde el repositorio del profesor, conservando tu código y actualizando correctamente la configuración del agente de IA.

```bash
# Paso 1 — Descarga todas las actualizaciones disponibles del profesor
git fetch upstream

# Paso 2 — Inspecciona qué llegó en la nueva semana antes de integrarlo
git log upstream/week-05 --oneline

# Paso 3 — Guarda y confirma tu trabajo actual si no lo has hecho
git add Src/ Inc/
git commit -m "semana-04: descripción de lo que hiciste"

# Paso 4 — Cambia a la rama de la nueva semana
git checkout week-05

# Paso 5 — Trae tu código de la semana anterior
git merge week-04

# Paso 6 — Conserva siempre la configuración de la semana actual
git checkout week-05 -- AGENTS.md ai-config/KNOWLEDGE.md

# Paso 7 — Confirma el resultado
git commit -m "semana-05: código de semana-04 integrado, configuración IA de semana-05 conservada"

# Paso 8 — Guarda en tu repositorio personal
git push origin week-05
```

---

## 3. Orientarte en tu repositorio

Antes de ejecutar cualquier comando importante, es buena práctica saber exactamente dónde estás y qué estado tiene tu repositorio.

```bash
# ¿En qué rama estoy?
git branch

# ¿Qué archivos han cambiado y cuál es el estado general?
git status

# ¿Cuáles son los últimos commits?
git log --oneline -5

# ¿Cuáles son todos los remotos configurados?
git remote -v
```

---

## 4. Ver cambios antes de confirmar

Antes de hacer un commit, siempre es útil revisar exactamente qué cambió. Estos comandos te muestran las diferencias línea por línea.

```bash
# Ver cambios en un archivo específico que aún no está en el área de preparación (staging)
git diff archivo.c

# Ver cambios en todos los archivos modificados
git diff

# Ver cambios que ya están en el área de preparación (después de git add)
git diff --staged

# Ver qué archivos son diferentes entre dos ramas (sin mostrar el contenido)
git diff week-03 week-04 --name-only
```

La diferencia entre `git diff` y `git diff --staged` es importante: el primero muestra cambios que aún no has agregado con `git add`, el segundo muestra los que ya están listos para el commit.

---

## 5. Preparar y confirmar cambios

```bash
# Agregar un archivo específico al área de preparación
git add Src/mi_archivo.c

# Agregar todos los archivos modificados en Src/ e Inc/
git add Src/ Inc/

# Quitar un archivo del área de preparación sin perder los cambios
git restore --staged archivo.c

# Descartar todos los cambios locales en un archivo (irreversible)
git restore archivo.c

# Confirmar los cambios preparados
git commit -m "semana-03: descripción clara de lo que hiciste"

# Renombrar un archivo y mantener el seguimiento de git
git mv nombre_viejo.c nombre_nuevo.c
```

⚠ `git restore archivo.c` descarta tus cambios de forma permanente. Úsalo solo cuando estés completamente seguro de que no necesitas esos cambios.

---

## 6. Inspeccionar el historial y los commits

```bash
# Ver el historial resumido de la rama actual
git log --oneline

# Ver el historial de una rama remota antes de hacer merge
git log upstream/week-05 --oneline

# Ver el detalle completo de un commit específico
git show a3f9c12

# Ver qué archivos cambió un commit específico
git show a3f9c12 --name-only

# Encontrar en qué commit apareció o desapareció un archivo
git log --all --oneline -- Src/mi_archivo.c

# Ver el contenido de un archivo en otra rama sin cambiar de rama
git show week-02:resources/EJERCICIOS_TAREA_semana00-03.md

# Ver qué archivos existen en una carpeta de otra rama
git show week-02:resources/
```

El comando `git show branch:path` es especialmente útil cuando quieres consultar un archivo de una semana anterior sin necesidad de hacer checkout — como abrir el libro de una semana pasada sin cerrar el que tienes en la mano.

---

## 7. Guardar trabajo temporalmente con stash

A veces necesitas cambiar de rama pero tienes cambios sin terminar que no quieres confirmar todavía. `git stash` guarda esos cambios temporalmente en una pila y limpia tu directorio de trabajo.

```bash
# Guardar cambios sin terminar temporalmente
git stash

# Ver qué hay guardado en el stash
git stash list

# Recuperar los cambios guardados más recientemente
git stash pop

# Recuperar los cambios sin eliminarlos del stash
git stash apply

# Descartar el stash más reciente sin recuperarlo
git stash drop
```

Piensa en `git stash` como un cajón temporal donde guardas tu trabajo a medio terminar antes de atender otra cosa. `git stash pop` abre el cajón y saca lo que dejaste, eliminándolo del cajón. `git stash apply` lo saca pero deja una copia en el cajón por si acaso.

---

## 8. Deshacer errores

Esta sección es la más importante cuando algo sale mal. Lee cada caso con cuidado y asegúrate de identificar exactamente cuál describe tu situación antes de ejecutar cualquier comando.

### 8a. Deshacer un commit que NO ha sido enviado a GitHub

Esto mueve la rama un commit hacia atrás pero conserva los archivos como cambios sin confirmar — es seguro y reversible:

```bash
git reset --soft HEAD~1
```

### 8b. Deshacer un commit que YA fue enviado a GitHub

Esto requiere un force push, que reescribe el historial remoto. Úsalo con precaución — si otros ya descargaron ese commit, puede causar problemas:

```bash
# Retroceder al commit correcto (usa el hash del commit al que quieres volver)
git reset --hard a3f9c12

# Forzar la actualización en GitHub
git push origin week-04 --force
```

### 8c. Deshacer un merge accidental que NO ha sido enviado a GitHub

Si hiciste un merge por error y aún no has hecho push, puedes volver exactamente al estado que tiene GitHub:

```bash
git reset --hard origin/week-04
```

### 8d. Restaurar archivos específicos desde otra rama

Cuando necesitas traer uno o varios archivos puntuales desde otra rama sin hacer un merge completo:

```bash
git checkout week-02 -- resources/EJERCICIOS_TAREA_semana00-03.md
```

### 8e. Quitar un archivo del área de preparación sin perder los cambios

Si hiciste `git add` de un archivo por error:

```bash
git restore --staged archivo.c
```

---

## 9. Ver quién modificó cada línea de un archivo

`git blame` muestra el historial de cada línea de un archivo: quién la escribió, en qué commit y cuándo. En un proyecto profesional se usa para entender por qué se tomó una decisión de código. En el contexto del curso, es una herramienta de aprendizaje — puedes ver la evolución de tu propio código a lo largo del semestre.

```bash
git blame Src/mi_archivo.c
```

La salida muestra el hash del commit, el autor, la fecha y el contenido de cada línea. No tiene nada de intimidante — es simplemente la historia de tu código, línea por línea.

---

## 10. Reglas generales para no meterse en problemas

- **Siempre verifica en qué rama estás** con `git branch` antes de hacer `git add` o `git commit`.
- **Siempre haz commit antes de cambiar de rama** — los cambios sin confirmar pueden perderse o contaminar otra rama.
- **Nunca uses `git push --force`** sin entender exactamente qué estás sobreescribiendo.
- **Nunca uses `git reset --hard`** sin verificar primero el historial con `git log --oneline`.
- **Cuando algo sale mal**, detente y describe el problema antes de ejecutar comandos de recuperación — un comando equivocado puede hacer las cosas más difíciles de recuperar. Consulta con tu profesor si no estás seguro.
- **Git casi nunca pierde información** — si cometiste un error, lo más probable es que tu trabajo esté en algún lugar del historial. La calma es tu mejor herramienta.
