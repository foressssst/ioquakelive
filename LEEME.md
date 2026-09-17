# ioquakelive — versión de attaraxia

Una implementación de **Quake Live** sobre el motor open source [ioquake3](https://ioquake3.org/).
El objetivo es un cliente y un servidor open source **equivalentes al Quake Live final (v1069)**.

Este repositorio es una rama propia del fork de [Thomas Jones](https://github.com/tjone270/ioquakelive)
(GPLv2). Su README original, en inglés, sigue estando en `README.md`. Este documento es **el estado
real de nuestra rama**, que ya se separó bastante.

> **La regla del proyecto.** Cada sistema se porta **desde el binario de retail**, no desde Quake 3
> ni desde el código del fork. Toda diferencia de comportamiento contra Quake Live retail cuenta
> como bug a arreglar, incluidas las reglas de juego y no solo lo visual. Cuando algo se aparta de
> retail es porque se decidió a propósito, y está anotado como excepción.

---

## Hasta dónde llega: ¿quién puede jugar con quién?

Esto es lo que el autor original dice, y **lo verifiqué en el código** porque es la pregunta que
define el alcance del proyecto.

### ✅ Un cliente de Quake Live retail SÍ puede entrar a un servidor ioquakelive

Es cierto, y la razón es concreta:

- Hablamos el **protocolo 91**, el de la build 1069 (`qcommon.h:243`).
- `SV_ValidateSteamAuth` (`code/server/sv_client.c`) es un *stub* que devuelve `qtrue`: el servidor
  **confía** en el Steam ID que el cliente declara en `getchallenge`, sin validar el ticket.

O sea que cualquier cliente que hable ese protocolo entra. **Un jugador con su Quake Live de Steam
puede conectarse a un servidor nuestro sin instalar absolutamente nada.**

### ❌ Nuestro cliente NO puede entrar a un servidor retail oficial

También cierto. En `code/client/cl_main.c:1727` mandamos un **token de autenticación falso**: 16
bytes en cero. Un servidor retail valida ese ticket contra Steam (`BeginAuthSession`) y nos rechaza.

### ¿Se podría arreglar algún día?

Técnicamente sí —habría que integrar Steamworks en el cliente para pedir un ticket real—, **pero no
creo que sea el camino que querés**, por dos motivos: entrar a servidores de id/Bethesda con un
cliente de terceros es zona gris, y sobre todo **no aporta lo que buscás**.

**El camino que sí vale la pena es el otro, y es mejor.** La asimetría juega a favor: *nosotros*
podemos hostear y *ellos* entran con su cliente de siempre. Para que eso funcione de verdad falta
**una sola pieza grande**:

| Falta | Por qué importa |
|---|---|
| **Master server / browser de servidores** | Hoy hay que entrar por IP con `/connect`. Sin lista de servidores, nadie encuentra nada. |

Todo lo demás para hostear ya está. Esa es, en mi opinión, la pieza que convierte esto de un
proyecto técnico en algo que la gente puede jugar.

---

## Qué hace falta para correrlo

Necesitás un `pak00.pk3` original de una copia **comprada** de
[Quake Live](https://steampowered.com/app/282440) en Steam. Sin él no arranca.

Quake Live y sus assets son propiedad de *id Software*. **No se redistribuyen**, ni siquiera desde
una copia legítima. El launcher busca el `pak00.pk3` en el Steam de cada jugador y lo vincula.

---

## Estado del proyecto

**En obra, y avanzando rápido.** Sobre la base del fork hay **61 commits propios**. Lo que sigue es
lo nuestro; lo que ya venía del fork está en su README.

### Físicas — **completo y verificado** ✅

El trabajo más a fondo de esta rama. Las **49 funciones** del `bg_pmove.c` de retail extraídas del
binario y comparadas **una por una** contra nuestro código.

- Se encontró el `bg_pmove` completo siguiendo `OVERCLIP = 1.001f` hasta el puntero estático `pm`
  del archivo; a partir de ahí, "quien toca `pm` es pmove y nadie más" da la lista exacta.
- 46 ya coincidían. Varias se verificaron **a nivel de desensamblado** porque el decompilado mentía:
  la rampa de `PM_Accelerate` usa velocidad 2D pero su producto punto es 3D; `PM_CorrectAllSolid`
  **lee `origin[0]` en los tres ejes** (es un bug de retail, y lo reproducimos).
- Tres cosas estaban mal y se portaron:

| Qué estaba mal | Efecto que tenía |
|---|---|
| Las cvars nunca llegaban a sus variables en C: `G_RegisterCvars` no llamaba a `onChanged` | `pm_stepJump` quedaba en 0 → **el step-jump entero muerto**: no podías montarte a un borde |
| `CG_UpdateStepChange` descartaba escalones y no mezclaba | Subir escaleras **de a una y con la cámara temblando** |
| El suavizado de escalón faltaba en tercera persona | Retail lo tiene en las dos vistas |

El mapa completo, con direcciones y evidencia, está en `../notes/pmove_port_checklist.md`.

### Interfaz — rediseñada entera

La estructura y el orden son los de retail; el diseño se moderniza con el lenguaje visual de
Quake Club. Menú principal, Settings con las **201 opciones reales** de retail, Training Center,
Start Match, Replays, menú de pausa, selector de modelo con vista previa 3D, cursor del sistema.

### Juego y presentación

| Área | Estado |
|---|---|
| FOV | ✅ Horizontal Hor+ como retail, y el aspecto corregido en el renderer |
| Timers de respawn en los ítems | ✅ La cara del ítem flota donde estaba, teñida, con el anillo de gajos |
| Iconos POI a través de las paredes | ✅ Quad Hog y portador de la bandera neutral |
| Consola | ✅ Se lee como la de retail |
| Mouse y zoom | ✅ Curva de retail y sensibilidad del zoom |
| Renderer | ✅ Cvars, corrección de color y bloom de QL |
| Chispas de impacto | ✅ Era una función vacía |
| Simple items | ✅ Flotan y obedecen sus tres cvars |
| Contador de medallas | ✅ |
| Estadísticas | ✅ Sobreviven a la muerte, como retail |
| Contaminación de gametypes | ✅ Las cvars se registran al arrancar |
| Bots | ✅ Piensan por su propia exportación, no desde `G_RunFrame` |
| Crash Course | ✅ Portado, con Crash caminando el tour |
| Skins *bright* / *fb* | ✅ |
| Humo, granadas, lead de proyectil | ✅ |
| Hitsounds | ⚠️ **Excepción a retail**, por decisión: siguen a WolfcamQL |

### FPS destapado — **excepción a retail, pedida** ⚠️

`com_maxfps` pasa de 30–250 a **0–1000**, con 0 = sin límite.

El candado era **doble**: el motor lo registra con ese rango y el cgame **lo vuelve a registrar**
igual (tabla de cvars en `cgamex86.dll 0x10078488`), así que subirlo solo en el motor funcionaba en
el menú y volvía a caer a 250 al entrar a un mapa.

Y el tope de retail **no era arbitrario**: `PM_Footsteps` avanza el ciclo de pasos con
`(int)(old + bobmove * msec)`, y esa truncación tira la parte decimal cada frame. Pasando de ~330
fps el producto queda bajo 1, se trunca a 0 y **el ciclo se congela**: no suenan los pasos y se
frena el bob del arma. El arreglo guarda lo que sobra en los bits 8–15 del propio `bobCycle` —el
único lugar seguro, porque esos bits no viajan por red y la predicción del cliente los reinicia de
forma determinista en cada snapshot.

> Efecto secundario que conviene saber: el ciclo pasa a avanzar **al ritmo real** en vez del
> truncado, así que a 125 fps los pasos suenan ~7 % más seguidos que en retail sin parchar. No se
> pueden tener las dos cosas: el ritmo de retail depende del fps, y eso *es* el bug.

### Distribución

Beta privada con launcher que se actualiza solo, manifiesto firmado y verificación de archivos.
**Exige Quake Live comprado en Steam**; `pak00.pk3` no se publica nunca.

---

## Lo grande que falta

Ordenado por lo que más cambiaría las cosas:

| Área | Estado | Por qué importa |
|---|---|---|
| **Master server / browser** | ❌ | **La pieza que falta para que esto sea jugable por gente.** Hoy solo por IP. |
| Sonido | ❌ | El sistema de sonido de QL sin auditar |
| Demos | ❌ | Grabación y reproducción sin probar con el protocolo QL |
| Descargas | ❌ | Redirect HTTP y descarga de pk3 sin auditar |
| Voz | ❌ | QL usa voz P2P de Steam; hoy tenemos el VOIP de ioquake3 |
| Scoreboards por equipos (UI) | ⚠️ | Bastantes cosas no dibujan bien |
| Sistema de baneos | ❌ | `SV_Ban_f` imprime "Not yet implemented" |
| ZMQ stats / rcon | ❌ | QL usa ZeroMQ |
| Factories | ⚠️ | El parseo está; falta aplicar la configuración JSON |
| Efectos de Freeze Tag y Red Rover | ❌ | Hielo, progreso de descongelado, infección |
| Loadouts | ⚠️ | La cvar se propaga; la lógica completa sin auditar |
| Modo entrenamiento | ⚠️ | Sin auditar |

### Hallazgos propios, esperando tu decisión

Cosas que encontré comparando contra el binario y **no toqué** porque no estaban en tu lista:

- **`PW_INVIS` y `PW_REGEN` están invertidos** respecto a retail (retail: invis = 7, regen = 8).
  Adentro del fork es coherente, así que no se ve jugando; importa donde crucemos números con
  retail (protocolo, demos, datos del binario).
- **El Quad Hog no arranca**: `G_QG_Think`, que respawnea el quad del modo, no lo llama nadie.
- **Falta `CG_OffsetThirdPersonViewSpecial`**, la vista de `cg_thirdPerson 2`.

---

## Compilar

```powershell
$msbuild = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
& $msbuild "misc\msvc\ioquakelive.sln" -p:Configuration=Release -p:Platform=Win32 -m:3
```

Proyectos sueltos: `cgame`, `ui`, `qagame`, `quakelive`, `opengl2`.
**Toolset:** v145 · **Plataforma:** Win32/x64

> El README original dice `misc\msvc142` y `pak01_content/`. Las dos rutas están desactualizadas:
> son `misc\msvc` y `content/pak01`.

**Tras cambiar un struct en un header, compilar con `-t:Rebuild`.** El build incremental de MSBuild
no siempre recompila los `.c` que lo incluyen, y eso ya provocó un crash difícil de encontrar.

---

## Cómo se trabaja acá

Lo que distingue a esta rama es el método. Hay un toolkit de ingeniería inversa montado sobre los
binarios de retail (que **no** están en el repo):

- **Ghidra en headless** con scripts propios para decompilar por dirección.
- **Doce scripts de análisis** en `re/tools/`: xrefs de strings y de globales, ventanas de
  desensamblado, barrido de instrucciones con cobertura informada, lectura de constantes.
- **clangd** indexando las ~304.000 líneas de código propio para `findReferences` e `incomingCalls`.

Tres trampas que ya costaron tiempo y quedan anotadas:

1. **Un barrido de instrucciones puede inventar un sitio.** Capstone se detiene en el primer byte
   indecodificable; un resultado negativo no vale nada sin informar la cobertura, y un `push`
   desalineado puede parecer una llamada que no existe.
2. **El decompilado miente con los tipos.** `0.001` y `2` aparecían como *doubles* con valores
   basura. Para cualquier umbral: leer los bytes crudos.
3. **Leer el fuente prueba lo que está escrito, no lo que corre.** El bug de las cvars sobrevivió a
   una comparación función por función contra el binario, porque las 36 cvars de pmove tenían los
   defaults correctos —solo que nadie los aplicaba.

---

## Licencia y origen

Fork de [ioquake3](https://github.com/ioquake/ioq3) vía
[tjone270/ioquakelive](https://github.com/tjone270/ioquakelive), bajo **GPL v2**.

Nunca se copia código decompilado al repositorio: se documenta el comportamiento observado y se
reimplementa. Los binarios y assets de id Software no se redistribuyen.
