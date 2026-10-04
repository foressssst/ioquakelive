# ioquakelive

ioquakelive es una implementación de **Quake Live** construida sobre [ioquake3](https://ioquake3.org/), un motor de código abierto. El objetivo es tener un cliente y un servidor de código abierto que funcionen igual que la versión final de Quake Live (build 1069).

En este documento, "el original" es el Quake Live que se compra en Steam.

## ¿Por qué?

El proyecto lo empezó [Thomas Jones](https://github.com/tjone270/ioquakelive), que hizo la parte más difícil: leer los binarios originales, deducir el protocolo y la estructura de los módulos, y lograr que un cliente y un servidor de Quake Live compilaran sobre ioquake3. Este repositorio sigue desde ahí.

La intención es la misma. Quake Live merece seguir vivo, y tener un cliente y un servidor abiertos puede ser el primer paso para un mod grande, para otro fork o para versiones mejores que esta.

## Cómo probarlo

Hace falta tener [Quake Live](https://store.steampowered.com/app/282440) comprado e instalado en Steam. ioquakelive no trae los archivos del juego: los toma de tu instalación.

1. Abre Steam con la cuenta que tiene Quake Live. La lista de servidores, los amigos y el ingreso a los servidores pasan por Steam.
2. Baja la última versión desde [Releases](https://github.com/foressssst/ioquakelive/releases). En Windows es `ioquakelive-windows-x64.zip`.
3. Descomprime el zip en una carpeta cualquiera.
4. Abre `ioquakelive.x86_64.exe`. El otro ejecutable, `ioquakelive-vulkan.x86_64.exe`, es el mismo juego dibujando con Vulkan.

Cosas que conviene saber:

- La primera vez, en Windows, el juego busca Quake Live en tu biblioteca de Steam y copia `pak00.pk3` a su carpeta `baseq3`. Si no lo encuentra, cópialo a mano desde `steamapps\common\Quake Live\baseq3`.
- Los ejecutables no están firmados, así que Windows Defender a veces bloquea la descarga o marca el juego con una detección terminada en `!ml`. Es un falso positivo. Se permite desde Seguridad de Windows, en Historial de protección.
- La configuración, las demos y las capturas se guardan en la carpeta `baseq3` del juego. No se escribe nada en tu perfil de Windows.
- El juego no se actualiza solo. Para pasar a una versión nueva, baja el zip y descomprímelo encima.
- Las versiones marcadas como pre-release traen sólo Windows. Las releases completas traen también Linux y macOS.

Si algo falla, abre un issue en este repositorio. Sirve saber qué versión es y qué estabas haciendo. La versión aparece en la consola, que se abre con la tecla que está a la izquierda del 1.

## Estado

El cliente (`ioquakelive`) y el servidor dedicado (`ioqlds`) compilan y funcionan. Con el cliente se juega en los servidores de Quake Live. Los tres módulos del juego (`qagame`, `cgame` y `ui`) están portados a partir de los binarios de la build 1069, y el motor tiene la integración con Steam: ingreso a los servidores, lista de servidores, Workshop, voz, amigos y estadísticas.

Como medida gruesa, esta tabla cuenta cuántos nombres de cvars y de archivos que usa cada módulo original aparecen también en este código:

| Módulo | Cvars | Archivos |
|---|---|---|
| `qagame` | 405 de 406 | 114 de 115 |
| `cgame` | 364 de 365 | 389 de 427 |
| `ui` | 202 de 211 | 119 de 120 |

Lo que falta son, casi todos, nombres `.tga` que acá se cargan con otra extensión y cvars que quedaron de Quake 3 o que sólo cambian en las mayúsculas. La cuenta se hace con herramientas que no están en este repositorio.

Una comparación así no dice si el comportamiento es el mismo. Eso se comprueba leyendo los binarios en Ghidra, y las direcciones revisadas quedan anotadas en el código, al lado de cada función portada.

### Qué funciona

- Jugar en servidores de Quake Live con este cliente y levantar un servidor propio con `ioqlds`.
- El protocolo de red de Quake Live.
- Todos los modos de juego: FFA, Duel, TDM, CA, CTF, 1FCTF, Harvester, FT, DOM, A&D, Race y RR.
- Físicas, armas, ítems, calentamiento, votaciones y compensación de lag como en el original.
- HUD, marcadores, locutor, medallas, números de daño y mensajes de muerte.
- Los menús de Quake Live, rehechos dentro del juego: portada, Start Match, lista de servidores, Training, Career Stats, Recent Matches, Replays y las 220 opciones de Settings.
- Steam: ingreso a los servidores, lista de servidores, amigos, avatares, voz, descargas del Workshop y estadísticas.
- Demos en los dos sentidos: las del original se reproducen acá y las grabadas acá se reproducen en el original.
- Bots con la IA de Quake Live.
- El Crash Course completo.
- En el servidor: baneos y permisos por Steam ID, y estadísticas y rcon por ZeroMQ.

### Qué agrega sobre el original

- Tres renderers: OpenGL clásico, que es el que viene por defecto, Vulkan y OpenGL 2. Los dos primeros vienen de [Quake3e](https://github.com/ec-/Quake3e).
- Los mapas cargan más rápido, porque las imágenes y los sonidos se decodifican en varios hilos.
- Pantalla completa sin bordes, y la ventana cambia de tamaño sin reiniciar el video.
- En Vulkan, baja latencia con Reflex (NVIDIA) y Anti-Lag 2 (AMD).
- Versiones para Windows ARM64 y Linux ARM64, y Vulkan en macOS.
- Emojis a color en el chat, la consola y los nombres.
- Medallas animadas y un chat que aparece y se va con animación, tomados de CPMA. El chat animado se apaga con `ch_animateScroll 0`.
- `cg_xray`: en demos y como espectador, muestra a los jugadores a través de las paredes.
- Los carteles propios de cada servidor se ven sólo en ese servidor. El original los deja pegados al pasar a otro.

### Qué falta

- Parte de la IA de los bots no está comparada con el original: los papeles de equipo en CTF y A&D, y las órdenes por chat de voz.
- El servidor dedicado no lee `workshop.txt`. La cvar `sv_workshopFile` existe pero no hace nada.
- La página escondida "???" del menú principal.
- Las capturas de pantalla no quedan asociadas a su partida. Las demos sí.
- `vmCvar_t` tiene un campo menos que en el original. Sólo importaría para cargar un módulo original, y no afecta al juego.

Y esto está hecho pero sin probar:

- Que un cliente del Quake Live original entre a un servidor `ioqlds`.
- Reflex y Anti-Lag 2 medidos en una placa que los tenga.
- macOS y Windows ARM64 en ejecución. Compilan y se empaquetan.

## Servidor dedicado

`ioqlds` viene en su propio zip, con un `server.cfg` y un `access.txt` de ejemplo en `baseq3`. Igual que el cliente, necesita el `pak00.pk3` de Quake Live en esa carpeta.

```
ioqlds +set dedicated 2
```

`dedicated 1` es para red local y `dedicated 2` para internet. El servidor lee `baseq3/server.cfg` al arrancar, igual que el del original.

## Diferencias contra ioquake3

| Área | ioquake3 (Quake 3) | ioquakelive (Quake Live) |
|------|--------------------|--------------------------|
| Protocolo | Q3 1.32 / ioq3 71 | el 91 de la build 1069 |
| Carpeta base | `baseq3` | `baseq3` en disco, `baseqz` como nombre de protocolo |
| Modos de juego | FFA, Tourney, TDM, CTF | además CA, FT, Race, DOM, A&D, Harvester, 1FCTF, RR y los de Team Arena |
| Físicas | control de aire de Q3 | además control de aire de CPM, doble salto, deslizamiento agachado y auto-hop |
| Texto | fuentes de mapa de bits, por escala | por índice de fuente, con tipografías TrueType dibujadas con fontstash |
| Panorámico | sólo estirado | 4:3 con anclaje a la izquierda, al centro o a la derecha |
| Llaves y puertas | no existen | ítems `IT_KEY` y `func_door` con llave |
| Estadísticas | marcador básico | precisión por arma y los marcadores de Quake Live |
| Menús | archivos `.menu` de Q3 | los menús web de Quake Live rehechos dentro del juego |
| Cuenta y servidores | master server propio | Steam |

## Avance por área

**Hecho** quiere decir portado y revisado contra el binario. **Parcial**, que funciona con un faltante que se nombra. **No aplica**, que el original tampoco lo tiene.

### Protocolo de red

| Área | Estado | Notas |
|------|--------|-------|
| Formato del netchan | Hecho | sin GENCHECKSUM, como la cabecera de paquete de QL |
| Mensaje del cliente | Hecho | el byte extra después de los tres enteros de cabecera |
| entityStateFields | Hecho | 58 campos reordenados, gravedad como int32 |
| playerStateFields | Hecho | 58 campos, `pm_flags` de 24 bits, `weaponPrimary` y 9 campos nuevos |
| Deltas de usercmd_t | Hecho | offsets 21 a 25 reordenados y dos campos byte nuevos |
| Configstrings | Hecho | al conectar sólo se reenvían las que cambiaron |
| Cifrado XOR del netchan | Hecho | la clave sale de `challenge ^ serverId ^ messageAcknowledge` |
| Huffman fuera de banda | Hecho | los paquetes de conexión se comprimen desde el byte 12 |

### Motor: cliente

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
| Descargas | Hecho | por el Workshop de Steam: el cliente baja lo que le falta antes de entrar |
| Carteles del servidor | Hecho | los packs del Workshop que los cambian valen sólo en ese servidor |
| Voz | Hecho | por la red entre pares de Steam; el módulo de juego decide quién oye a quién |
| Autocompletado de consola | Hecho | `callvote`, mapas, factories, nombres de jugadores y comandos del cgame |
| Capturas | Hecho | la ruta y el nombre del original, levelshot incluido |
| Modo seguro | Hecho | tras un cierre anormal ofrece arrancar con los ajustes de sistema por defecto |
| Cliente de 64 bits | Hecho | `Release\|x64` es la compilación principal |

### Motor: servidor

| Área | Estado | Notas |
|------|--------|-------|
| Instantáneas | Hecho | la serialización de entidades y jugadores coincide con QL |
| Bots | Hecho | `sv_bot.c` |
| Autenticación de Steam | Hecho | valida el ticket con `BeginAuthSession`; sin Steam en el servidor se entra igual, para red local |
| Carga del módulo de juego | Hecho | DLL nativa con la tabla `gameImport_t` |
| Baneos | Hecho | por Steam ID desde `access.txt`; ban, tempban y unban escriben ahí |
| Estadísticas y rcon por ZeroMQ | Hecho | las cvars `zmq_*` del original; un rcon sin contraseña sólo acepta conexiones locales |
| Lista de servidores | Hecho | la da Steam, como en el original |
| Latido al master | Hecho | el game server de Steam; `sv_master 0` lo saca de la lista |
| Ítems del Workshop | Parcial | anuncia los que tiene montados; no lee `workshop.txt` |
| Limitación de tasa | Hecho | la de `sv_floodProtect` y el contador de userinfo del módulo de juego |
| Linux ARM | Hecho | compilado y corriendo en aarch64 |

### cgame

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
| Mensajes de muerte | Hecho | atacante, víctima e icono del arma |
| Predicción y pmove | Hecho | el pmove reescrito desde el binario |
| Clan tags | Hecho | la clave `cn` con sus 26 colores |
| Chispas de impacto, estilos de arma, zoom y viñeta | Hecho | con sus cvars |
| Congelación | Hecho | los tres shaders de hielo según avanza el descongelado, y los pedazos con su estela |
| Infección | Hecho | el punto de interés y los nombres de Red Rover, y el aspecto del infectado |
| Avisos del chat | Hecho | `cg_chatbeep` y `cg_teamChatBeep` |
| Cámara del intermedio | Hecho | incluido el caso de un mapa sin `info_player_intermission` |
| Cuerpo del jugador seguido | Hecho | al morir sin desintegrarse, el espectador ve el cuerpo |

### Menús (módulo ui)

| Área | Estado | Notas |
|------|--------|-------|
| Exports nuevos | Hecho | `UI_REGISTER_CVARS`, `UI_CHECK_ACTIVE_MENU`, `UI_WALK_MENUS`, `UI_DRAW_ADVERTISEMENT` |
| Fuentes por índice | Hecho | todos los puntos de dibujo pasan `item->fontIndex` |
| Archivos de menú | Hecho | se pueden reemplazar desde `pak01.pk3` |
| Portada | Hecho | menú en columna, arte de fondo, créditos y la lista de amigos |
| Start Match | Hecho | factories, selector de mapa y bots |
| Lista de servidores | Hecho | desde Steam, con columnas que ordenan, filtros y la imagen del mapa |
| Settings | Hecho | 220 opciones en el orden del original |
| Training | Hecho | Crash Course, Accelerate y Elevate |
| Replays | Hecho | cada demo grabada en una partida aparece como esa partida |
| Career Stats | Hecho | Statistics y Medals, leídas de las estadísticas de Steam |
| Recent Matches | Hecho | el servidor manda el reporte de la partida por Steam y el cliente lo guarda |
| Avisos legales | Hecho | el texto se lee del `web.pak` del jugador |
| Marcadores (`.menu`) | Hecho | los de equipo y los individuales |
| Acciones de menú | Hecho | las 13: `clientViewProfile`, `modPlayer`, `putspec`, `banPlayer` y el resto |
| Perfil del jugador | Hecho | Career Stats, avatar de Steam y el perfil en la superposición de Steam |
| Amigos | Hecho | la lista de Steam con los colores y el menú del original: Join Match, Send Message, Send Invite y demás |
| Voto de mapa al terminar | Hecho | necesita un `mappool.txt`, igual que en el original |

### Módulo de juego (qagame)

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

### Renderer

| Área | Estado | Notas |
|------|--------|-------|
| OpenGL clásico (`opengl1`) | Hecho | el de Quake3e con lo de QL portado; viene por defecto |
| Vulkan | Hecho | el de Quake3e, con el bloom y la corrección de color de QL |
| OpenGL 2 (`opengl2`) | Hecho | el de ioquake3, de respaldo |
| Reflex y Anti-Lag 2 | Parcial | en Vulkan, con `r_lowLatency`; falta medirlos en hardware |
| Shaders de QL | Hecho | las 134 palabras del parser del original |
| Shaders repetidos | Hecho | gana la primera definición, como en el original |
| Carteles | Hecho | el renderer agrega la superficie de cada cartel del mapa |
| Cadena del gancho | Hecho | `RT_RAIL_CORE` |
| Hielo | Hecho | los tres shaders sobre el modelo congelado |
| Bloom | Hecho | con los valores del original |
| Texturas en hilos | Hecho | JPEG, TGA y PNG |
| Cuadros en cola | Hecho | `r_maxQueuedFrames`, 0 por defecto |

## Compilar

Salen dos programas: `ioquakelive`, el cliente, e `ioqlds`, el servidor dedicado. Antes de compilar hay que traer los submódulos:

```
git submodule update --init --recursive
```

### Windows

Con Visual Studio 2026 o sus Build Tools (toolset v145):

```
cd misc\msvc
MSBuild ioquakelive.sln -p:Configuration=Release -p:Platform=x64
MSBuild ioqlds.vcxproj  -p:Configuration=Release -p:Platform=x64
```

El servidor dedicado es un proyecto aparte de la solución, por eso lleva su propia línea. Todo queda en `build\Release\x64\`.

Las plataformas son `x64`, `Win32` y `ARM64`. Para ARM64 hay que correr antes `code\tools\Build-SDL3.ps1`. Después de cambiar un struct en un header conviene compilar con `-t:Rebuild`, porque la compilación incremental a veces deja objetos viejos.

### Linux y macOS

```
make release
```

Es el Makefile de ioquake3. Necesita SDL3 instalado y visible para `pkg-config`. Los binarios son `ioquakelive`, `ioquakelive-vulkan` e `ioqlds`.

## Cómo se publica una versión

Las versiones las compila y las publica el CI de GitHub según el mensaje del commit:

```
git commit -m "release: 1.0.0"
git push origin main
```

- `release: X` compila todas las plataformas, crea el tag `vX` y publica la release.
- `prerelease: X` compila sólo Windows x86 y x64 y lo publica como pre-release. Es para parches chicos.

En los dos casos `PRODUCT_VERSION`, en `code/qcommon/q_shared.h`, tiene que decir la misma versión.

El cliente y el servidor son dos descargas:

```
ioquakelive-windows-x64.zip     el cliente: motor, renderers, módulos de juego y baseq3/
ioquakelive-windows-x86.zip
ioquakelive-windows-arm64.zip
ioquakelive-linux-x86_64.zip
ioquakelive-linux-arm64.zip
ioquakelive-macos-arm64.zip     ioquakelive.app, con ioqlds adentro

ioqlds-windows-x64.zip          el servidor: ioqlds, baseq3/ y los archivos de ejemplo
ioqlds-windows-x86.zip
ioqlds-windows-arm64.zip
ioqlds-linux-x86_64.zip
ioqlds-linux-arm64.zip
```

Ninguno trae `pak00.pk3`. GitHub adjunta a cada release el código fuente del tag, que es lo que pide la GPL.

El paquete de macOS se firma y se notariza sólo si los secretos de Apple están cargados en el repositorio. Sin ellos se publica sin firmar.

`pak01.pk3` lleva además arte que este repositorio no puede incluir: la tipografía Handel Gothic Ex, los cuadros de las medallas y del globo de chat de CPMA, y el fondo y los logos del menú. El CI lo saca de un repositorio privado. Una compilación sin eso funciona, con las medallas quietas y otra tipografía en algunos textos.

## Carpetas

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
content/pak01/     lo que agrega ioquakelive a los archivos del juego → pak01.pk3
content/server/    server.cfg y access.txt de ejemplo, para el paquete de ioqlds
```

## Créditos y licencia

- [Thomas Jones](https://github.com/tjone270/ioquakelive) empezó este fork.
- [ioquake3](https://github.com/ioquake/ioq3) es el motor de base.
- [Quake3e](https://github.com/ec-/Quake3e) aporta los renderers de OpenGL clásico y de Vulkan.
- De CPMA vienen los cuadros de las medallas animadas y del globo de chat.
- Los emojis son las imágenes de Noto Emoji.
- También se usan SDL3, MoltenVK en macOS, y fontstash con stb_truetype para el texto.

Quake Live y sus archivos son propiedad de id Software. Este repositorio no los incluye ni los redistribuye.

El código se distribuye bajo la GPL v2, igual que ioquake3. El texto está en `COPYING.txt`. Abajo se conserva el README original de ioquake3.

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
