# Lista de verificación para el cambio de semana

**Antes de comenzar la semana N — haz esto cada vez, sin excepciones.**

---

## Pasos a seguir

**Paso 1 — Guarda y confirma tu trabajo actual.**

Antes de tocar cualquier rama, asegúrate de que todo lo que hiciste la semana pasada esté guardado de forma segura. Si haces esto primero, nada se perderá.

```bash
git add Src/ Inc/
git commit -m "semana-04: describe aquí lo que hiciste"
```

**Paso 2 — Cambia a la rama de la nueva semana.**

```bash
git checkout week-05
```

**Paso 3 — Trae tu trabajo anterior hacia adelante.**

```bash
git merge week-04
```

**Paso 4 — Resuelve el conflicto de configuración de la IA (siempre conserva la semana actual).**

Después del merge, los archivos de configuración pueden entrar en conflicto. Siempre resuelve a favor de la semana actual:

```bash
git checkout week-05 -- AGENTS.md ai-config/KNOWLEDGE.md
git commit -m "semana-05: código de semana-04 integrado, configuración IA de semana-05 conservada"
```

**Paso 5 — Guarda la nueva semana en tu repositorio personal (origin).**

Este paso es fundamental y fácil de olvidar. Hasta este momento, la nueva semana solo existe en tu máquina local — si algo le pasa a tu computador, perderías la configuración. Envíala a tu repositorio en GitHub para que quede guardada en la nube:

```bash
git push origin week-05
```

**Paso 6 — Verifica tu entorno.**

Abre STM32CubeIDE, confirma que el proyecto compila sin errores, y verifica que Copilot CLI reconoce el nuevo `AGENTS.md`. La IA ahora sabrá que estás en la semana 5.

**Paso 7 — Completa la autoevaluación inicial.**

Antes de escribir código nuevo, inicia una conversación con el asistente de IA. Te hará 3 o 4 preguntas de la semana anterior para confirmar que tu base es sólida. Si tienes dificultades con 2 o más preguntas, repasa el material antes de continuar.

---

## Cuando el profesor actualiza una semana ya publicada

De vez en cuando, el profesor puede corregir un error o mejorar el contenido de una rama que ya tienes en tu repositorio — por ejemplo, una corrección en `AGENTS.md` de `week-00` mientras tú ya estás trabajando en `week-01`. Cuando esto ocurra, el profesor lo anunciará en clase. No es algo que debas revisar manualmente todos los días.

El procedimiento tiene dos partes: primero actualizas la rama afectada, y luego traes esa corrección hacia tu rama de trabajo actual para que no pierdas la información nueva.

**Parte A — Actualiza la rama que el profesor corrigió.**

En este ejemplo el profesor corrigió `week-00`, pero el procedimiento es idéntico para cualquier otra semana:

```bash
# Descarga los cambios más recientes del repositorio del profesor
git fetch upstream week-00

# Cambia a la rama que fue corregida
git checkout week-00

# Incorpora los cambios del profesor en tu rama local
git merge upstream/week-00

# Guarda la rama actualizada en tu repositorio personal
git push origin week-00
```

**Parte B — Lleva la corrección a tu rama de trabajo actual.**

Ahora que `week-00` está actualizada, necesitas traer esa corrección hacia la semana en la que estás trabajando, en este caso `week-01`. La regla de siempre conservar la configuración de la semana actual sigue aplicando:

```bash
# Regresa a tu rama de trabajo
git checkout week-01

# Incorpora los cambios de week-00
git merge week-00

# Conserva siempre la configuración de la semana actual
git checkout week-01 -- AGENTS.md ai-config/KNOWLEDGE.md

# Confirma el resultado
git commit -m "week-01: incorporada corrección del profesor desde week-00"

# Guarda en tu repositorio personal
git push origin week-01
```

Si la corrección viene de una semana más antigua (por ejemplo `week-00`) pero tú estás en `week-03`, debes repetir la Parte B para cada semana intermedia en orden: primero `week-01`, luego `week-02`, y finalmente `week-03`. Esto garantiza que el historial de git sea limpio y que la corrección fluya correctamente hacia adelante sin saltos.

---

## Solución de problemas

**"git me pide que confirme o guarde los cambios antes de cambiar de rama"**

Esto significa que tienes archivos modificados que aún no están confirmados. Git te está protegiendo — no te permitirá cambiar de rama y arriesgarte a perder trabajo. Regresa al Paso 1 y confirma todo primero. Si hiciste cambios que no quieres conservar, puedes descartarlos con `git checkout -- .`, pero ten cuidado, esto es irreversible.

**"Después del merge, el código de la semana anterior desapareció"**

Probablemente olvidaste el Paso 3, o lo ejecutaste en el orden incorrecto. Verifica en qué rama estás con `git branch` y revisa el historial de confirmaciones con `git log --oneline`. Tu trabajo casi con certeza no se perdió — todavía está en la rama de la semana anterior. Regresa, verifica que esté allí y repite la lista desde el Paso 2.

**"git dice que hay conflictos de merge en Src/ o Inc/"**

Esto es poco frecuente pero puede ocurrir si el mismo archivo fue modificado en ambas ramas. Abre el archivo en conflicto — git habrá marcado las secciones conflictivas con `<<<<<<<`, `=======` y `>>>>>>>`. Lee ambas versiones con cuidado y conserva la que representa tu código más reciente. Luego marca el conflicto como resuelto:

```bash
git add Src/el_archivo_en_conflicto.c
git commit -m "semana-05: conflicto de merge resuelto en archivo"
```

Si no estás seguro de cuál versión conservar, pregúntale a tu asistente de IA — esta es exactamente el tipo de situación donde hablar en voz alta sobre el problema ayuda.

**"El asistente de IA sigue comportándose como la semana pasada"**

Esto significa que Copilot CLI no está reconociendo el nuevo `AGENTS.md`. Verifica que completaste el Paso 4 correctamente revisando el contenido del archivo: `cat AGENTS.md`. Si el archivo todavía muestra el contexto de conocimiento de la semana pasada, repite el Paso 4. Si el archivo se ve correcto pero la IA sigue comportándose de forma extraña, cierra y vuelve a abrir la sesión de terminal.

**"git merge dice 'Already up to date'"**

Esto en realidad significa que todo está bien — git verificó y encontró que `week-05` ya contiene todo lo de `week-04`. Esto puede ocurrir si ya habías hecho el merge anteriormente. Puedes continuar tranquilamente al Paso 6.

**"Confirmé algo en la rama equivocada"**

No entres en pánico — este es uno de los errores más comunes en git y es completamente recuperable. Llama a tu profesor antes de intentar corregirlo por tu cuenta, porque el comando de recuperación (`git cherry-pick` o `git reset`) depende exactamente de lo que ocurrió. Intentar corregirlo sin entender la situación puede hacer las cosas más difíciles de recuperar.
