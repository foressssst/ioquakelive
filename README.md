# ioquakelive

ioquakelive es una implementacion del **Quake Live original** sobre el motor de código abierto [ioquake3](https://ioquake3.org/). La meta es tener un cliente y un servidor abiertos que funcionen igual que la última versión de Quake Live (build 1069), y posteriormente sumarle mejoras y calidad de vida.

Este proyecto lo empezó [Thomas Jones](https://github.com/tjone270/ioquakelive), creditos totales para el, descifró el protocolo y la estructura de los módulos, logró que un cliente y un servidor de Quake Live compilaran sobre ioquake3, este repositorio sigue desde ahí y sigue activo.

## Cómo probarlo

**Este fork solo funciona si tienes Quake Live comprado en Steam.** ioquakelive no trae los archivos del juego, los saca de tu instalación.

1. Baja la última versión desde [Releases](https://github.com/foressssst/ioquakelive/releases).
2. Descomprímela en cualquier carpeta.
3. Abre `ioquakelive.x86_64.exe`, para jugar con Vulkan abre `ioquakelive-vulkan.x86_64.exe`, que es el mismo juego.

A tener en cuenta:

- La primera ejecución en Windows el juego buscara tu Quake Live en tu biblioteca de Steam, copiara `pak00.pk3` a su carpeta `baseq3`. Si no lo encuentra, cópialo tú desde tu instalacion de QUAKE LIVE.
- Los ejecutables no van firmados, así que es probable (muy poco) que Windows Defender bloquee la descarga o no te deje abrir el juego.
- Para poder actualizar de version tendras que bajar el zip nuevo manualmente y descomprímirlo encima.
- Las pre-releases traen solo Windows. Linux y macOS salen en las releases completas.

Si algo falla, abre un issue acá.

### Novedades

- Tres renderers para el juego: OpenGL, que viene por defecto, Vulkan y OpenGL 2.
- FSR.
- Soporte de NVIDIA REFLEX y AMD Anti-Lag 2.
- Los mapas cargan más rápido, las imágenes y los sonidos ahora se leen en varios hilos.
- El DEMO PLAYER fue totalmente renovado.
- `cg_xray` muestra a los jugadores a través de las paredes, en demos y como espectador.
- `help <comando o cvar>` explica qué hace, y `searchconsole` busca texto en la consola.
- `frametimes` mide el tiempo real entre cuadros, e `inputrate` cuántos datos manda tu mouse por segundo.
- La consola se anima al abrirse. `con_animate 0` la deja quieta.
- En los menús, la consola y las demos se usa la flecha del sistema.

### Estado del fork

Los tres módulos del juego (`qagame`, `cgame` y `ui`) están portados desde los binarios de la build 1069, cada función portada lleva anotadas las direcciones del binario contra las que se revisó en Ghidra.
Para tener una idea del avance, esta tabla cuenta cuántos nombres de cvars y de archivos del módulo original aparecen también acá. No dice si se comportan igual; eso se revisa en el binario.

| Módulo | Cvars | Archivos |
|---|---|---|
| `qagame` | 405 de 406 | 114 de 115 |
| `cgame` | 364 de 365 | 389 de 427 |
| `ui` | 202 de 211 | 119 de 120 |

Lo que falta en realidad es casi nada, muy poco, algunos archivos `.tga` acá se cargan con otra extensión, y algunos cvars que quedaron de Quake 3 o que solo cambian en como se escribe. Hay además un detalle interno, `vmCvar_t` tiene un campo menos que en el original, esto solo se importaría para cargar un módulo original y no afecta al juego final.

#### Protocolo de red

| Área | Estado | Notas |
|------|--------|-------|
| Formato del netchan | Hecho | sin GENCHECKSUM, como la cabecera de paquete de QL |
| Mensaje del cliente | Hecho | el byte extra después de los tres enteros de cabecera |
| entityStateFields | Hecho | 58 campos reordenados, gravedad como int32 |
| playerStateFields | Hecho | 58 campos, `pm_flags` de 24 bits, `weaponPrimary` y 9 campos nuevos |
| Deltas de usercmd_t | Hecho | offsets 21 a 25 reordenados y dos campos byte nuevos |
| Configstrings | Hecho | al conectar solo se reenvían las que cambiaron |
| Cifrado XOR del netchan | Hecho | la clave sale de `challenge ^ serverId ^ messageAcknowledge` |
| Huffman fuera de banda | Hecho | los paquetes de conexión se comprimen desde el byte 12 |

#### Motor: cliente

| Área | Estado | Notas |
|------|--------|-------|
| Cvars del cgame | Hecho | se registran al arrancar, antes de leer la configuración |
| Conexión | Hecho | ticket de Steam en `getchallenge`, cvar `cl_steamId` |
| Módulos del juego | Hecho | `iobin.pk3` lleva los de cada arquitectura; sirve en servidores puros |
| Sonido | Hecho | registro, canales y el set de sonidos de QL |
| Temporización de cuadro | Hecho | `com_preciseFrameTiming`: a 125 fps la variación baja de 0,34 a 0,01 ms |
| Carga en hilos | Hecho | imágenes y sonidos en paralelo; campgrounds carga en cerca de 1 s |
| Pantalla completa sin bordes | Hecho | `r_fullscreen 2` |
| Demos | Hecho | compatibles con el original en los dos sentidos |
| Workshop | Hecho | el cliente baja lo que le falta antes de entrar, como el original |
| Carteles del servidor | Hecho | los packs del Workshop que los cambian valen solo en ese servidor |
| Voz | Hecho | por la red entre pares de Steam; el módulo de juego decide quién oye a quién |
| Autocompletado de consola | Hecho | `callvote`, mapas, factories, nombres de jugadores y comandos del cgame |
| Capturas | Hecho | la ruta y el nombre del original, levelshot incluido |
| Modo seguro | Hecho | tras un cierre anormal ofrece arrancar con los ajustes de sistema por defecto |
| Cliente de 64 bits | Hecho | `Release\|x64` es la compilación principal |

#### Motor: servidor

| Área | Estado | Notas |
|------|--------|-------|
| Instantáneas | Hecho | la serialización de entidades y jugadores coincide con QL |
| Bots | Hecho | `sv_bot.c` |
| Autenticación de Steam | Hecho | valida el ticket con `BeginAuthSession`; sin Steam en el servidor se entra igual, para red local |
| Carga del módulo de juego | Hecho | DLL nativa con la tabla `gameImport_t` |
| Baneos | Hecho | por Steam ID desde `access.txt`; ban, tempban y unban escriben ahí |
| Estadísticas y rcon por ZeroMQ | Hecho | las cvars `zmq_*` del original; un rcon sin contraseña solo acepta conexiones locales |
| Lista de servidores | Hecho | la da Steam, como en el original |
| Latido al master | Hecho | el game server de Steam; `sv_master 0` lo saca de la lista |
| Workshop | Hecho | baja lo de `workshop.txt` antes de arrancar y lo anuncia a los clientes |
| `sv_fps` | Hecho | fijo en 40, como el original |
| Limitación de tasa | Hecho | la de `sv_floodProtect` y el contador de userinfo del módulo de juego |
| Servidor lleno | Hecho | `sv_fullRedirect` manda a los jugadores de ioquakelive a otro servidor con lugar |
| Linux ARM | Hecho | compilado y corriendo en aarch64 |

#### cgame

| Área | Estado | Notas |
|------|--------|-------|
| Eventos | Hecho | 100 manejadores `EV_`, con race, infección y premios |
| Panorámico | Hecho | `CG_AdjustFrom640` con los modos estirado, izquierda, centro y derecha |
| Texto de ownerdraw | Hecho | las cerca de 70 funciones pasan por `CG_DrawText` |
| Serverinfo | Hecho | los 29 campos de `CG_ParseServerinfo` |
| Marcadores | Hecho | FFA, duelo y TDM, y el de fin de partida con el modelo del ganador |
| HUD | Hecho | la disposición del original, con los relojes de ítems y la tarjeta del TAB |
| Locutor, medallas y rondas | Hecho | las seis medallas animadas usan 24 cuadros de CPMA |
| Números de daño | Hecho | `CG_DrawDamagePlums` completo |
| Mira | Hecho | incluida la regla de no dibujarla en los múltiplos de 30 |
| Mensajes de muerte | Hecho | los clásicos del original: atacante, víctima e icono del arma |
| Predicción y pmove | Hecho | el pmove reescrito desde el binario |
| `cl_timeNudge` | Hecho | con la extrapolación de jugadores y el adelanto de proyectiles del original |
| Clan tags | Hecho | la clave `cn` con sus 26 colores |
| Chispas de impacto, estilos de arma, zoom y viñeta | Hecho | con sus cvars |
| Congelación | Hecho | los tres shaders de hielo según avanza el descongelado, y los pedazos con su estela |
| Infección | Hecho | el punto de interés y los nombres de Red Rover, y el aspecto del infectado |
| Avisos del chat | Hecho | `cg_chatbeep` y `cg_teamChatBeep` |
| Cámara del intermedio | Hecho | incluido el caso de un mapa sin `info_player_intermission` |
| Cuerpo del jugador seguido | Hecho | al morir sin desintegrarse, el espectador ve el cuerpo |

#### Menús (módulo ui)

| Área | Estado | Notas |
|------|--------|-------|
| Exports nuevos | Hecho | `UI_REGISTER_CVARS`, `UI_CHECK_ACTIVE_MENU`, `UI_WALK_MENUS`, `UI_DRAW_ADVERTISEMENT` |
| Fuentes por índice | Hecho | todos los puntos de dibujo pasan `item->fontIndex` |
| Archivos de menú | Hecho | los de ioquakelive van en `pak01.pk3`, en `ui/ioql/`, y valen también en servidores puros |
| Medidas | Hecho | se escala por el alto de la pantalla, como la web del original, con sus reglas para 4:3 y 5:4 |
| Portada | Hecho | menú en columna, arte de fondo, avisos legales y la lista de amigos; dentro de una partida, con la barra verde |
| Start Match | Hecho | factories, selector de mapa y bots |
| Lista de servidores | Hecho | desde Steam, con columnas que ordenan, filtros, la imagen del mapa y la ventana de info de partida |
| Settings | Hecho | 220 opciones en el orden del original, también dentro de una partida |
| Training | Hecho | Crash Course, Accelerate y Elevate; aparte, la sala de tracking de ioquakelive |
| Replays | Hecho | cada demo grabada en una partida aparece como esa partida |
| Career Stats | Hecho | Statistics y Medals, leídas de las estadísticas de Steam |
| Recent Matches | Hecho | el servidor manda el reporte de la partida por Steam y el cliente lo guarda |
| Avisos legales | Hecho | el texto se lee del `web.pak` del jugador |
| Marcadores (`.menu`) | Hecho | los de equipo y los individuales |
| Acciones de menú | Hecho | las 13: `clientViewProfile`, `modPlayer`, `putspec`, `banPlayer` y el resto |
| Votaciones desde el menú | Hecho | el voto de mapa usa el modo elegido en el menú, como el original |
| Perfil del jugador | Hecho | Career Stats, avatar de Steam y el perfil en la superposición de Steam |
| Amigos | Hecho | la lista de Steam con los colores y el menú del original: Join Match, Send Message, Send Invite y demás |
| Voto de mapa al terminar | Hecho | necesita un `mappool.txt`, igual que en el original |

#### Módulo de juego (qagame)

| Área | Estado | Notas |
|------|--------|-------|
| Modos de juego | Hecho | FFA, Duel, TDM, CA, CTF, 1FCTF, Harvester, FT, DOM, A&D, Race y RR |
| Físicas | Hecho | control de aire, doble salto, deslizamiento agachado, auto-hop y escaleras |
| Gancho | Hecho | tirón a pared y a enemigo, frenado de cerca y amortiguación en agua |
| Armas | Hecho | todas las de QL con sus tiempos de recarga |
| Daño y salpicadura | Hecho | |
| Llaves y puertas | Hecho | llaves plateada, dorada y maestra |
| Calentamiento | Hecho | `g_gameState`, porcentaje de listos y abandono |
| Votaciones | Hecho | incluido el voto de mapa al terminar |
| Cvars | Hecho | 405 de las 406 del original, con sus funciones de cambio |
| Factories | Hecho | `map` sin factory imprime el uso, como el original |
| Race | Hecho | checkpoints, cronometraje y HUD |
| Compensación de lag | Hecho | historial de posiciones y rebobinado |
| Armadura por niveles, perdigones en anillos y caída de daño por distancia | Hecho | |
| Entrenamiento | Hecho | `g_training.c` |
| Loadouts | Hecho | del menú al spawn, con su munición |
| Estadísticas en JSON | Hecho | los mismos documentos del original, por ZeroMQ |
| Premium | No aplica | la build 1069 no tiene suscripciones |
| Permisos | Hecho | `access.txt`: ban, mod y admin por Steam ID |
| Comandos de administración | Hecho | privilegios y comandos reescritos contra el binario |
| Entidades por archivo | Hecho | `sv_altEntDir` lee `<mapa>.ent`; `sv_dumpEntities` las escribe |

#### Renderer

| Área | Estado | Notas |
|------|--------|-------|
| OpenGL clásico (`opengl1`) | Hecho | el de Quake3e con lo de QL portado; viene por defecto |
| Vulkan | Hecho | el de Quake3e, con el bloom y la corrección de color de QL |
| OpenGL 2 (`opengl2`) | Hecho | el de ioquake3, de respaldo |
| Reflex y Anti-Lag 2 | Parcial | en Vulkan, con `r_lowLatency`; Reflex medido en NVIDIA, Anti-Lag 2 sin probar en AMD |
| FSR | Hecho | `r_fsr`, solo en OpenGL clásico |
| Shaders de QL | Hecho | las 134 palabras del parser del original |
| Shaders repetidos | Hecho | gana la primera definición, como en el original |
| Carteles | Hecho | el renderer agrega la superficie de cada cartel del mapa |
| Cadena del gancho | Hecho | `RT_RAIL_CORE` |
| Hielo | Hecho | los tres shaders sobre el modelo congelado |
| Bloom | Hecho | con los valores del original |
| Texturas en hilos | Hecho | JPEG, TGA y PNG |
| Cuadros en cola | Hecho | `r_maxQueuedFrames`, 0 por defecto |

</details>

### Compilar

Salen dos programas: `ioquakelive`, el cliente, e `ioqlds`, el servidor dedicado. Antes de compilar hay que traer los submódulos:

```
git submodule update --init --recursive
```

#### Windows

Con Visual Studio 2026 o sus Build Tools (toolset v145):

```
cd misc\msvc
MSBuild ioquakelive.sln -p:Configuration=Release -p:Platform=x64
MSBuild ioqlds.vcxproj  -p:Configuration=Release -p:Platform=x64
```

El servidor dedicado es un proyecto aparte de la solución, por eso va en su propia línea. Todo queda en `build\Release\x64\`.

Las plataformas son `x64`, `Win32` y `ARM64`. Para ARM64 hay que correr antes `code\tools\Build-SDL3.ps1`. Si cambias un struct en un header, compila con `-t:Rebuild`: la compilación incremental a veces deja objetos viejos.

#### Linux y macOS

```
make release
```

Es el Makefile de ioquake3. Necesita SDL3 instalado y visible para `pkg-config`. Los binarios son `ioquakelive`, `ioquakelive-vulkan` e `ioqlds`.

### Carpetas

```
code/
  cgame/           módulo de juego del cliente: HUD, marcadores, efectos, predicción
  game/            módulo de juego del servidor: reglas, entidades, físicas
  ui/              menús
  client/          el cliente del motor
  server/          el servidor del motor
  qcommon/         lo compartido: red, sistema de archivos, comandos, Steam
  renderercommon/  lo compartido entre renderers
  renderergl1/     OpenGL clásico
  renderergl2/     OpenGL 2
  renderervk/      Vulkan
  sdl/, sys/       ventana, entrada y sistema operativo
  SDL3/            submódulo
  tools/           scripts de compilación y de empaquetado
misc/msvc/         solución y proyectos de Visual Studio
content/pak01/     lo que agrega ioquakelive a los archivos del juego, que termina en pak01.pk3
content/server/    server.cfg y access.txt de ejemplo, para el paquete de ioqlds
```

## Créditos y licencia

- [Thomas Jones](https://github.com/tjone270/ioquakelive)
- [ioquake3](https://github.com/ioquake/ioq3)
- [Quake3e](https://github.com/ec-/Quake3e)
- [CPMA](https://playmorepromode.com/)
- Soporte emojis: Noto Emoji.
- Texto: `sans-serif` y Liberation Sans.
- SDL3, MoltenVK en macOS, y fontstash con stb_truetype para texto.

Quake Live y sus archivos son propiedad de id Software, este repositorio no los incluye ni los redistribuye.
Código distribuido bajo la GPL v2.

---

<details>
<summary>README original de ioquake3</summary>

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
