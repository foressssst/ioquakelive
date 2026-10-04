Ioquakelive, es una implementación del juego **Quake Live**, construida sobre el motor de código abierto [ioquake3](https://ioquake3.org/), cuyo objetivo es producir un cliente y un servidor de código abierto funcionalmente equivalentes a la versión final de Quake Live (v1069).

#### ¿Por qué?

Originalmente Thomas Jones empezó este fork e hizo la parte mas dificil, leer los binarios de retail,
deducir el protocolo y la estructura de los módulos, y conseguir que un cliente y un servidor
de Quake Live llegaran a compilar sobre ioquake3.

La intención es la misma. Quake Live merece seguir vivo, y un cliente y un servidor abiertos
quizá sean sólo el primer paso hacia un mod grande, hacia otro fork, o hacia implementaciones
mejores que todo lo que cualquiera de los dos haya hecho hasta ahora.

### Estado del proyecto

En obra, y mucho más avanzado de lo que parece. Los dos programas compilan y corren: `ioquakelive`, el cliente, e `ioqlds`, el servidor dedicado. Los módulos de juego están completos contra retail, y la infraestructura del lado del motor que faltaba ya está: autenticación por Steam, la lista de servidores y el latido al master de Steam, descargas del Workshop, voz por Steam, estadísticas y rcon por ZeroMQ, y el sistema de moderación y baneos.

Las tablas de abajo se auditaron de nuevo contra el código y contra los binarios de retail. Cada fila dice qué se comprobó, y una fila sólo dice Hecho cuando el comportamiento se leyó del binario y coincidió. Dos mediciones las respaldan, las dos reproducibles con las herramientas de `re/tools/`:

| Módulo | Cvars de retail presentes en este fork | Assets de retail presentes | Faltan |
|---|---|---|---|
| `qagame` | 405 / 406 (99,8 %) | 108 / 115 | `fs_cdpath`; unos pocos assets de la época de Q3 |
| `cgame` | 363 / 365 (99,5 %) | 374 / 427 | casi todos nombres `.tga` que registramos como `.png` |
| `ui` | 201 / 211 (95,3 %) | 114 / 120 | restos de Q3 (`ui_cdkeyvalid`, `r_colorBits`) |

`python re/tools/extract_strings.py` regenera esa comparación; las listas por ítem quedan en `notes/strings_gap.md`.

La comparación a nivel de símbolos **no** es posible para el módulo de juego: `qagamex64.so` está reducido a 19 símbolos C con nombre (todos en el área de estadísticas y reportes), así que `gap_analysis.py` no puede medir cobertura. En su lugar el comportamiento se verifica leyendo los binarios en Ghidra, y las direcciones verificadas quedan citadas en el código al lado de cada port.

#### Qué funciona

- Un cliente de Quake Live puede conectarse a un servidor ioquakelive, y este cliente puede conectarse a servidores de Quake Live.
- El protocolo de red de QL (netchan propio, tablas de campos de entity/player state, formato de usercmd).
- El módulo de juego de QL (`qagame`): todos los tipos de juego, la tabla de cvars entera, físicas, armas, ítems, calentamiento, votaciones, unlagged.
- El módulo cgame de QL: HUD, marcadores, locutor, medallas, números de daño, mensajes de muerte, efectos, predicción.
- El módulo de interfaz de QL: el front end entero rehecho sobre la estructura de la web UI de retail, con 220 opciones en el orden de retail, Career Stats y Recent Matches desde Steam, el menú de amigos y los avisos legales.
- Bots con la IA de Quake Live: elección de objetos, oído, habilidad dinámica, `bot_hud`, los modos por rondas y A&D.
- El Crash Course, portado entero: Crash aparece, la cinemática, sus líneas con voz y el recorrido.
- Tipografía por FreeType, sistema de coordenadas panorámico, sobreescritura de assets por `pak01.pk3`.

#### Qué hace falta para correrlo
Necesitas un `pak00.pk3` original, sacado de una copia **comprada** de [Quake Live](https://steampowered.com/app/282440) en Steam. ioquakelive no arranca sin un `pak00.pk3` legítimo. Quake Live y sus assets siguen siendo propiedad de *id Software*.

No tengo ningún derecho sobre los assets de Quake Live y no puedo, ni **voy a**, redistribuirlos desde mi propia copia legítima.

#### Diferencias principales contra ioquake3

| Área | ioquake3 (Q3) | ioquakelive (QL) |
|------|---------------|-------------------|
| Protocolo | Q3 1.32 / ioq3 71 | el protocolo 91 de la build 1069 de QL |
| Carpeta base | `baseq3` | sistema de archivos `baseq3`, nombre de protocolo `baseqz` |
| Tipos de juego | FFA, Tourney, TDM, CTF | + CA, FT, Race, DOM, A&D, Harvester, 1FCTF, RR (y todos los modos de TA) |
| Físicas | control de aire de Q3 | + control de aire de CPM, doble salto, deslizamiento agachado, auto-hop |
| Sistema de fuentes | por escala (textFont/bigFont) | por índice de fuente (extraFonts[3]), más tipografías TrueType desde pak01 |
| Panorámico | sólo estirado | 4:3 restringido, con modos izquierda/centro/derecha para anclar elementos |
| Llaves y puertas | no existe | ítems `IT_KEY`, `func_door` con llave |
| Estadísticas | marcador básico | precisión por arma detallada, marcadores 'premium' de QL |
| Front end | archivos `.menu` de Q3 | la web UI de Quake Live rehecha de forma nativa: portada, Start Match, Training, Career Stats, Recent Matches, Replays, 220 opciones |

### Avance de la implementación

El estado es **Hecho** (portado y verificado contra el binario), **Parcial** (funciona, con un hueco nombrado), **Pendiente** (sin empezar o stub) o **No aplica**.

#### Protocolo de red

| Área | Estado | Notas |
|------|--------|-------|
| Formato del netchan | Hecho | GENCHECKSUM quitado, para coincidir con la cabecera de paquete de QL |
| Parseo del mensaje del cliente | Hecho | byte extra después de los 3 longs de cabecera en `SV_ExecuteClientMessage` |
| entityStateFields | Hecho | 58 entradas, reordenadas, gravedad como int32 |
| playerStateFields | Hecho | 58 entradas, `pm_flags` de 24 bits, `weaponPrimary`, 9 campos nuevos de QL |
| Deltas de usercmd_t | Hecho | offsets 21-25 reordenados, 2 campos byte genéricos agregados |
| Arreglo de la avalancha de configstrings | Hecho | `csUpdated[]` por cliente durante `CS_PRIMED`, sólo se reenvía lo que cambió |
| Cifrado XOR del netchan | Hecho | la clave sale de `challenge ^ serverId ^ messageAcknowledge` |
| Huffman en datos fuera de banda | Hecho | `NET_OutOfBandData` comprime los paquetes de conexión desde el byte 12 |

#### Motor: cliente

| Área | Estado | Notas |
|------|--------|-------|
| Llamada a CG_REGISTER_CVARS | Hecho | `VM_Call` antes de `CG_INIT` en `cl_cgame.c` |
| Protocolo de conexión | Hecho | autenticación de Steam en `getchallenge`, `NET_OutOfBandRaw` para cargas binarias, cvar `cl_steamId` |
| Sistema de paks de DLL | Hecho | pk3 `iobin` por arquitectura, compatible con servidor puro |
| Sistema de sonido | Hecho | registro, canales y el set de sonidos de QL, portados desde el binario |
| Temporización de cuadro | Hecho | objetivo por `Sys_Microseconds`, `com_preciseFrameTiming`; el temblor baja de 0,34 a 0,01 ms a 125 fps |
| Carga en hilos | Hecho | cola de trabajos, decodificación de imágenes y sonidos en paralelo, índice del texto de shaders; campgrounds carga en ~1 s |
| Pantalla completa sin bordes | Hecho | `r_fullscreen 2`, sin reiniciar el vídeo al cambiar de ventana |
| Grabación y reproducción de demos | Hecho | en los dos sentidos: una demo de retail se reproduce aquí, y una grabada aquí se reproduce en retail con su HUD, entidades y control de demo |
| Sistema de descargas | Hecho | la vía de QL es el Workshop de Steam: el servidor publica sus items en `CS_STEAM_WORKSHOP_IDS` y el cliente baja lo que le falta (`CL_WorkshopDownloads`) antes de entrar |
| VOIP / voz de Steam | Hecho | voz por la red entre pares de Steam, con el servidor de puente y el módulo de juego decidiendo quién oye a quién (`G_VoiceFilter`) |
| Autocompletado de consola | Hecho | `callvote`, las listas de mapas y factories, nombres de jugadores y las órdenes del propio cgame |
| Sistema de capturas | Hecho | la ruta y el nombre de retail, levelshot incluido (que escribe un TGA, dijera lo que dijera su nombre) |
| Cliente x86_64 | Hecho | `Release\|x64` es la compilación principal, con Steam, y la que se juega |

#### Motor: servidor

| Área | Estado | Notas |
|------|--------|-------|
| Sistema de instantáneas | Hecho | la serialización de entity/player state coincide con QL |
| Gestión de bots | Hecho | `sv_bot.c` funcional |
| Autenticación de Steam | Hecho | el servidor le pasa el ticket a Steam con `BeginAuthSession` y echa al que no pasa; sin Steam del lado del servidor se entra igual, que es lo único que deja jugar en red local |
| Carga del módulo de juego | Hecho | carga de DLL nativa con la tabla de punteros `gameImport_t` |
| Sistema de baneos | Hecho | retail banea por Steam ID desde `access.txt` (`g_accessFile`, `/reload_access`), no por el `SV_Ban_f` del motor. La lista se lee al conectar, y ban, tempban y unban escriben en ella |
| Estadísticas y rcon por ZMQ | Hecho | `zmq.c`: el socket de rcon y el publicador de estadísticas, con las cvars `zmq_*` de retail. Un rcon sin contraseña aquí sólo acepta conexiones locales, que es una divergencia deliberada |
| Protocolo del navegador de servidores | Hecho | la lista la da Steam, como en retail, no un master propio |
| Latido al master server | Hecho | el game server de Steam con `EnableHeartbeats`; `sv_master 0` lo saca del navegador |
| Redirección de descarga de mapas | Hecho | por el Workshop, que es como lo hace QL |
| Limitación de tasa | Hecho | el cubo que gotea de retail (`sv_floodProtect` como número con coma, el puntaje sube uno por orden y baja uno por segundo) más el contador de avalancha de userinfo del módulo de juego |
| Servidor dedicado Linux/ARM | Hecho | compilado y corriendo en aarch64 |

#### cgame (módulo de juego del lado del cliente)

| Área | Estado | Notas |
|------|--------|-------|
| Sistema de eventos | Hecho | 100 manejadores `EV_`, incluidos race, infección y premios |
| Renderizado panorámico | Hecho | `CG_AdjustFrom640` con modos `STRETCH`/`LEFT`/`CENTER`/`RIGHT` |
| Texto de ownerdraw | Hecho | las ~70 funciones de ownerdraw pasan por `CG_DrawText` |
| Parseo del serverinfo | Hecho | 29 campos, los del `CG_ParseServerinfo` del binario |
| Marcadores | Hecho | FFA, duelo y TDM portados desde el binario, más el de fin de partida con el modelo del ganador |
| HUD | Hecho | la disposición de retail, con los relojes de ítems y la tarjeta del TAB |
| Locutor, medallas y rondas | Hecho | las seis medallas animadas usan los 24 cuadros de CPMA |
| Números de daño | Hecho | `CG_DrawDamagePlums` portado entero: la reserva, no sólo los números |
| Mira | Hecho | `0x1000c2a0`, incluida la regla de "sin mira en los múltiplos de 30" |
| Mensajes de muerte | Hecho | nombres de atacante y víctima con el icono del arma |
| Predicción y pmove | Hecho | el pmove entero reescrito desde el binario, más 9 arreglos verificados encima |
| Clan tags | Hecho | `0x1003e640`, la clave `cn` con sus 26 colores |
| Chispas de impacto, estilos de arma, zoom, viñeta | Hecho | portados con sus cvars |
| Publicidad | Hecho | `CG_DrawAdvertisements` |
| Efectos de congelación | Hecho | los tres shaders de hielo sobre el jugador congelado, elegidos por `generic1` mientras lo descongelan, y los pedazos con su estela (`CG_IceTrail`) |
| Visuales de infección | Hecho | el POI y los nombres de Red Rover, y el infectado: baba sobre el modelo, el zumbido, y sin arma |
| Sonidos de aviso del chat | Hecho | `cg_chatbeep` y `cg_teamChatBeep` con `sound/player/talk.ogg`. La cvar lleva `b` minúscula, que es por lo que parecía no existir |
| Cámara del intermedio | Hecho | `FindIntermissionPoint` / `MoveClientToIntermission` contra el binario (0x10054f16), incluido el respaldo cuando un mapa no tiene `info_player_intermission` |

#### Interfaz (módulo de UI)

| Área | Estado | Notas |
|------|--------|-------|
| Exports nuevos | Hecho | `UI_REGISTER_CVARS`, `UI_CHECK_ACTIVE_MENU`, `UI_WALK_MENUS`, `UI_DRAW_ADVERTISEMENT` |
| Soporte de fontIndex | Hecho | los 26+ puntos de llamada pasan `item->fontIndex` |
| Cvars | Hecho | ~78 cvars nuevas registradas en `ui_main.c` |
| Carga de archivos de menú | Hecho | sistema de sobreescritura por `pak01.pk3` |
| Portada | Hecho | rehecha de forma nativa: menú en columna, arte de fondo, créditos, franja de amigos |
| Start Match | Hecho | la página de retail: factories, selector de mapa, bots |
| Ajustes | Hecho | 220 opciones en el orden de retail, sacadas del CSS y los scripts de `web.pak` |
| Centro de entrenamiento | Hecho | Crash Course, Accelerate, Elevate, con las órdenes de retail |
| Replays | Hecho | la página de retail: cada demo grabada en una partida se muestra como esa partida, con acceso a su página |
| Career Stats | Hecho | Statistics y Medals, leídas de las estadísticas de Steam del jugador como en retail; victorias y KDR en la tarjeta de perfil |
| Recent Matches | Hecho | el servidor le manda a cada jugador el reporte de la partida por la red peer to peer de Steam, como el de retail; la lista y la página de la partida con Basic, Medals, Items, Kills, Deaths, Accuracy, Damage y Events |
| Avisos legales | Hecho | la página y su enlace, con el texto leído del `web.pak` del jugador |
| Marcadores (archivos .menu) | Hecho | los de equipo y los de no-equipo dibujan correctamente |
| Navegador de servidores | Hecho | `ui_browser.c`: la lista desde Steam, columnas que ordenan, filtros y el cuadrito del mapa por fila |
| Acciones de scripts de UI | Hecho | los 13 ejecutan sus órdenes de verdad: `clientViewProfile`, `modPlayer`, `putspec`, `banPlayer` y el resto |
| Perfil del jugador | Hecho | Career Stats, avatares de Steam y el perfil en la superposición de Steam, como retail |
| Amigos y funciones sociales | Hecho | la lista de amigos de Steam con el menú de retail en todas las páginas: Join Match, Send Message, Send Invite, View Statistics, View Achievements, Show Steam Profile; también en `/friends` |
| UI de votación de mapa | Hecho | la cadena entera: el servidor arma `nextmaps`, el cgame carga `ui/endgamevote.menu` y el botón vota. Hace falta un `mappool.txt`, igual que en retail |

#### Módulo de juego (lógica del lado del servidor)

| Área | Estado | Notas |
|------|--------|-------|
| Tipos de juego | Hecho | FFA, Duel, TDM, CA, CTF, 1FCTF, Harvester, FT, DOM, A&D, Race, RR |
| Físicas (bg_pmove) | Hecho | reescritas desde el binario: control de aire, doble salto, deslizamiento agachado, auto-hop, escaleras |
| Gancho | Hecho | tirón a pared y a enemigo, frenado de cerca, amortiguación en agua |
| Definiciones de armas | Hecho | todas las de QL con sus tiempos de recarga |
| Daño y salpicadura | Hecho | portados desde el binario |
| Llaves y puertas | Hecho | `IT_KEY`, llaves plateada, dorada y maestra, spawnflags de `func_door` |
| Máquina de estados del calentamiento | Hecho | `g_gameState`, porcentaje de listos, lógica de abandono |
| Sistema de votación | Hecho | `VF_ENDMAP_VOTING`, `IntermissionVote`, parseo de `nextmaps` |
| Tabla de cvars | Hecho | ~390 cvars con 56 callbacks `OnChanged`; 405 de las 406 de retail están presentes |
| Factories | Hecho | el motor las parsea y las aplica; `map` sin factory imprime el uso, como retail |
| Race | Hecho | checkpoints, cronometraje y el HUD de carrera |
| Unlagged (`lagHax`) | Hecho | historial de posiciones, grabado y rebobinado |
| Armadura por niveles, anillos de escopeta, caída por distancia | Hecho | `CheckArmor` reescrito y los sistemas de armas |
| Modo entrenamiento | Hecho | `g_training.c`: `info_tour_point`, `G_TrainingFrame`, los nodos de la IA, `Cmd_SkipTraining` |
| Sistema de loadout | Hecho | de punta a punta: el servidor publica el modo, el cgame arma el arma elegida, pmove la copia y el spawn la entrega con su munición |
| Estadísticas en JSON | Hecho | los documentos son los que arma retail, con las claves leídas del binario, y salen por el socket de ZeroMQ |
| Premium / suscripción | No aplica | la build 1069 no tiene ninguna compuerta de suscripción: `premium`, `subscription`, `paid` y `pro account` no aparecen en ninguno de los cuatro binarios. id lo sacó cuando QL pasó a Steam |
| Listas de control de acceso | Hecho | `g_access`: el archivo de `g_accessFile`, con ban, mod y admin por Steam ID, reescrito al cambiar de mapa |
| Comandos de administración | Hecho | el sistema de privilegios y sus órdenes reescritos contra el binario: las 16 compuertas de votación dejan pasar al administrador, `kick` y `clientkick` son ramas distintas, los parámetros numéricos se validan |
| Override de entidades del mapa | Hecho | `sv_altEntDir` lee `<dir>/<mapa>.ent` en vez del BSP, y `sv_dumpEntities` las escribe |

#### Renderer

| Área | Estado | Notas |
|------|--------|-------|
| RT_RAIL_CORE | Hecho | la cadena del gancho |
| Decodificación de texturas en hilos | Hecho | JPEG, TGA y PNG decodificados en hilos trabajadores |
| Barrera de cuadro en la GPU | Hecho | `r_maxQueuedFrames`, por defecto 0 para igualar la latencia de retail |
| Shaders específicos de QL | Hecho | el vocabulario entero del parser sacado del binario del motor (134 palabras) y contrastado contra `tr_shader.c`. Faltaba una: `nobotclip`, que Quake 3 llama `botclip`. `nodefault` y `depthTest disable` están |
| Shader de congelado | Hecho | tres shaders de hielo sobre el modelo, elegidos según cuánto avanzó el descongelado |
| Post-procesado (bloom) | Hecho | `tr_qlpost.c`, con los valores de retail |

### Lo que sigue abierto

Las tablas de arriba van por área. Esto es lo concreto que se sabe que falta, y es la lista
entera:

- **Una parte de la IA de los bots todavía no se comparó con el binario.** La elección de
  objetivos, el combate, el chat, las órdenes y los modos por rondas sí; los papeles de equipo
  de CTF y A&D (retail los reparte sin líder de equipo) y las órdenes por chat de voz no.
- **La página escondida "???" del menú principal** (escribiendo `iddqd` en él) no está portada.
- **Las capturas de pantalla no quedan asociadas a la partida en que se sacaron.** Las demos sí.
- **`vmCvar_t` tiene un campo menos aquí que en retail.** El binario lee la cadena en +0x14 y
  este árbol la tiene en +0x10. No rompe nada, porque el motor y los módulos comparten esta
  definición; solo importaría para cargar un módulo de retail. Anotado a propósito, sin cambiar.

### Compilar

Esto produce dos programas: **`ioquakelive`**, el cliente, e **`ioqlds`**, el servidor dedicado. Son el mismo motor, con la mitad de cliente compilada afuera en el segundo.

#### Windows (Visual Studio)

```
cd misc\msvc
MSBuild ioquakelive.sln -p:Configuration=Release -p:Platform=Win32
MSBuild ioqlds.vcxproj  -p:Configuration=Release -p:Platform=Win32
```

El servidor dedicado es un proyecto aparte y no está en la solución, así que lleva una segunda llamada. Todo queda en `build\<config>\<plataforma>\`.

Proyectos individuales: `cgame.vcxproj`, `ui.vcxproj`, `qagame.vcxproj`, `ioquakelive.vcxproj`, `opengl2.vcxproj`, `ioqlds.vcxproj`

**Toolset:** v145 (VS 2022+), **Plataforma:** Win32/x64. Tras cambiar un struct en un header, compilar con `-t:Rebuild`: el build incremental deja objetos viejos.

#### Linux / macOS

El build estándar de ioquake3 por Makefile; los binarios son `ioquakelive` e `ioqlds`. El servidor dedicado se compiló y se corrió en Linux aarch64.

### Publicaciones

Una versión se publica desde `main` por el mensaje del commit, no a mano:

```
git commit --allow-empty -m "release: 1.0.0"
git push origin main
```

El CI compila todas las plataformas, etiqueta el commit `v1.0.0` y publica un Release en GitHub. Un parche chico sale igual con `prerelease: 1.0.1`: el CI compila solo Windows x86 y x64 y lo publica marcado como pre-release. El cliente y el servidor dedicado salen de una sola compilación pero son dos cosas de instalar, así que son dos descargas:

```
ioquakelive-windows-x64.zip     el cliente: motor, renderer, módulos de juego, baseq3/
ioquakelive-linux-x86_64.zip
ioquakelive-macos-arm64.zip     un paquete ioquakelive.app, con ioqlds adentro

ioqlds-windows-x64.zip          el servidor dedicado: ioqlds, baseq3/, y un
ioqlds-linux-x86_64.zip         server.cfg y un access.txt de ejemplo para editar
```

GitHub adjunta al release el código del tag por su cuenta, como `Source code (zip)`
y `(tar.gz)`, que es lo que pide la GPL v2.

Ninguno trae `pak00.pk3`. Se descomprime, se pone el `pak00.pk3` propio en `baseq3/` y se ejecuta. En Windows el cliente hace ese paso solo: si falta `pak00.pk3`, busca Quake Live en la biblioteca de Steam y ofrece copiarlo.

Todo lo demás vive en esa misma carpeta. El `baseq3/` que está al lado del ejecutable es el único que hay: los pk3s, las configuraciones, los archivos de menú y de HUD, las demos y las capturas van ahí, y no se escribe nada en el perfil del usuario. Para tenerlos en el perfil, se arranca con `+set com_homepath ioquakelive`; para tenerlos donde sea, `+set fs_homepath <ruta>`.

El servidor dedicado lee `baseq3/server.cfg` por su cuenta al arrancar, igual que el qzeroded de retail, así que no hay nada que ejecutar a mano:

```
ioqlds +set dedicated 2
```

`dedicated 1` es una red local, `dedicated 2` es internet.

No hay actualizador y nada llama a casa. Una versión nueva es una descarga nueva.

El paquete de macOS se firma y se notariza sólo cuando los secretos de Apple (`MACOS_CERTIFICATE_P12` y los demás) están cargados en el repositorio; sin ellos el job corre igual y publica un paquete sin firmar.

### Disposición de carpetas

```
code/
  cgame/          módulo de juego del lado del cliente (HUD, marcador, efectos, predicción)
  game/           módulo de juego del lado del servidor (lógica, entidades, físicas)
  ui/             módulo de interfaz (menús, navegador de servidores)
  client/         código de cliente del motor
  server/         código de servidor del motor
  qcommon/        código compartido del motor (red, sistema de archivos, órdenes)
  renderercommon/ código compartido del renderer
  renderergl2/    renderer de OpenGL 2
misc/
  msvc/           solución y proyectos de Visual Studio
content/pak01/    sobreescritura de assets (menús, fuentes, shaders) → pak01.pk3
content/server/   server.cfg y access.txt de ejemplo, para el paquete de ioqlds
```

Las releases suman a `pak01.pk3` el arte que este repositorio no puede llevar: Handel Gothic Ex, los cuadros de las medallas y del globo de chat de CPMA 1.53, y el fondo y los logos del menú. Sale de un repositorio privado que el CI lee con una deploy key. Un build sin eso funciona igual, con las medallas quietas, el frag message en la fuente de Quake Live y la captura de un mapa detrás del menú.

## Ancestry

This project is a fork of [ioquake3](https://github.com/ioquake/ioq3) and retains its GPL v2 license. The original ioquake3 README is preserved below.

---

<details>
<summary>Original ioquake3 README</summary>

![Build](https://github.com/ioquake/ioq3/workflows/Build/badge.svg)

                   ,---------------------------------------.
                   |   _                     _       ____  |
                   |  (_)___  __ _ _  _ __ _| |_____|__ /  |
                   |  | / _ \/ _` | || / _` | / / -_)|_ \  |
                   |  |_\___/\__, |\_,_\__,_|_\_\___|___/  |
                   |            |_|                        |
                   |                                       |
                   `--------- https://ioquake3.org --------'

The intent of this project is to provide a baseline Quake 3 which may be used
for further development and baseq3 fun.
Some of the major features currently implemented are:

  * SDL 2 backend
  * OpenAL sound API support (multiple speaker support and better sound
    quality)
  * Full x86_64 support on Linux
  * VoIP support, both in-game and external support through Mumble.
  * MinGW compilation support on Windows and cross compilation support on Linux
  * AVI video capture of demos
  * Much improved console autocompletion
  * Persistent console history
  * Colorized terminal output
  * Optional Ogg Vorbis support
  * Much improved QVM tools
  * Support for various esoteric operating systems
  * cl_guid support
  * HTTP/FTP download redirection (using cURL)
  * Multiuser support on Windows systems (user specific game data
    is stored in "%APPDATA%\Quake3")
  * PNG support
  * Web support via Emscripten
  * Many, many bug fixes

The map editor and associated compiling tools are not included. We suggest you
use a modern copy from http://icculus.org/gtkradiant/.

The original id software readme that accompanied the Q3 source release has been
renamed to id-readme.txt so as to prevent confusion. Please refer to the
website for updated status.

More documentation including a Player's Guide and Sysadmin Guide are on:
https://ioquake3.org/help/

If you've got issues that you aren't sure are worth filing as bugs, or just
want to chat, please visit our forums:
https://discourse.ioquake.org

# Credits

Maintainers

  * James Canete <use.less01@gmail.com>
  * Ludwig Nussel <ludwig.nussel@suse.de>
  * Thilo Schulz <arny@ats.s.bawue.de>
  * Tim Angus <tim@ngus.net>
  * Tony J. White <tjw@tjw.org>
  * Jack Slater <jack@ioquake.org>
  * Zack Middleton <zturtleman@gmail.com>

Significant contributions from

  * Ryan C. Gordon <icculus@icculus.org>
  * Andreas Kohn <andreas@syndrom23.de>
  * Joerg Dietrich <Dietrich_Joerg@t-online.de>
  * Stuart Dalton <badcdev@gmail.com>
  * Vincent S. Cojot <vincent at cojot dot name>
  * optical <alex@rigbo.se>
  * Aaron Gyes <floam@aaron.gy>

</details>
