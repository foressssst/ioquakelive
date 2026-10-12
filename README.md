# ioquakelive

`ioquakelive` es una implementación de **Quake Live** basada en el motor de código abierto [ioquake3](https://ioquake3.org/). El objetivo es reproducir el comportamiento del cliente y del servidor de la última versión original, **build 1069**, y desarrollar mejoras propias sin perder compatibilidad con el juego.

El proyecto parte del trabajo de [Thomas Jones](https://github.com/tjone270/ioquakelive), quien reconstruyó partes del protocolo de red y de los módulos del juego, y consiguió compilar el cliente y el servidor sobre ioquake3. Este repositorio continúa ese trabajo.

<sub>Desarrollado con apoyo de [Claude Code](https://claude.com/claude-code).</sub>

## Cómo probarlo

Necesitas una copia legítima de **Quake Live en Steam**. Este proyecto no incluye los archivos originales del juego: utiliza los recursos de tu propia instalación.

1. Descarga la última versión desde [Releases](https://github.com/foressssst/ioquakelive/releases).
2. Extrae el archivo en una carpeta.
3. Ejecuta `ioquakelive.x86_64.exe`. Para usar Vulkan, ejecuta `ioquakelive-vulkan.x86_64.exe`.

### Antes de empezar

- En Windows, el juego intenta localizar Quake Live en tu biblioteca de Steam y copiar `pak00.pk3` a la carpeta `baseq3`. Si no lo encuentra, tendrás que copiar ese archivo manualmente desde tu instalación original.
- Los ejecutables no están firmados digitalmente. Windows Defender podría mostrar una advertencia o bloquear su ejecución.
- Las actualizaciones se instalan manualmente: descarga la nueva versión y extrae su contenido sobre la carpeta existente.
- Las pre-releases están disponibles solo para Windows. Las versiones completas incluyen Linux y macOS.
- Las versiones beta anteriores a la 1.0, desde la 0.1.0 hasta la 0.3.13, están disponibles en [ioquakelive-betas](https://github.com/foressssst/ioquakelive-betas/releases).

Si encuentras un problema, puedes reportarlo en [Issues](https://github.com/foressssst/ioquakelive/issues).

## Novedades

- **Tres renderers:** OpenGL clásico, OpenGL 2 y Vulkan.
- **AMD FSR:** escalado de imagen mediante `r_fsr`.
- **NVIDIA Reflex y AMD Anti-Lag 2:** opciones de baja latencia en Vulkan, con el estado de compatibilidad indicado en la sección de renderizado.
- **Carga en paralelo:** imágenes y sonidos se procesan en varios hilos para reducir los tiempos de carga.
- **Demo Player renovado:** interfaz y reproducción de demos actualizadas.
- **Visión a través de paredes:** `cg_xray` permite ver jugadores durante la reproducción de demos y al observar una partida.
- **Búsqueda y ayuda en consola:** `help <comando o cvar>` muestra información sobre un comando o una cvar, mientras que `searchconsole` permite buscar texto en la consola.
- **Herramientas de diagnóstico:** `frametimes` mide el tiempo entre cuadros e `inputrate` muestra la frecuencia de los datos enviados por el mouse.
- **Consola animada:** `con_animate 0` desactiva la animación al abrirla.
- **Flecha del sistema:** utilizada en los menús, la consola y el reproductor de demos.

## Estado del proyecto

Los tres módulos principales del juego —`qagame`, `cgame` y `ui`— se han reconstruido a partir de los binarios de Quake Live build 1069. Las direcciones de las funciones examinadas con Ghidra están documentadas en los mensajes de commit correspondientes.

La siguiente tabla compara los nombres de las cvars y los archivos presentes en el código con los del juego original. **La coincidencia de nombres no garantiza que el comportamiento sea idéntico**; esa equivalencia se comprueba por separado.

| Módulo | Cvars | Archivos |
|---|---:|---:|
| `qagame` | 406/406 | 115/115 |
| `cgame` | 365/365 | 427/427 |
| `ui` | 210/211 | 120/120 |

La única cvar del módulo `ui` que no se ha incorporado es `ui_cdkeyvalid`, heredada del sistema de clave de CD de Quake 3 y no utilizada por Quake Live.

En el recuento también se consideran presentes los archivos que se cargan con otra extensión y las cvars cuyo nombre cambia, pero que el motor trata de forma equivalente. Existe además una diferencia interna en `vmCvar_t`: la estructura tiene un campo menos que en el original. Esto solo sería relevante para cargar un módulo original y no afecta al juego en su estado actual.

### Detalle de implementación

Las siguientes tablas documentan las áreas portadas y su estado actual. Los nombres de funciones, estructuras, cvars y componentes se mantienen tal como aparecen en el código.

#### Protocolo de red

| Área | Estado | Notas |
|---|---|---|
| Formato de `netchan` | Hecho | Sin `GENCHECKSUM`, como en la cabecera de paquetes de Quake Live. |
| Mensaje del cliente | Hecho | Incluye el byte adicional después de los tres enteros de cabecera. |
| `entityStateFields` | Hecho | 58 campos reordenados; gravedad como `int32`. |
| `playerStateFields` | Hecho | 58 campos, `pm_flags` de 24 bits, `weaponPrimary` y nueve campos nuevos. |
| Deltas de `usercmd_t` | Hecho | Offsets 21 a 25 reordenados y dos campos adicionales de tipo byte. |
| Configstrings | Hecho | Al conectar, solo se reenvían las que han cambiado. |
| Cifrado XOR de `netchan` | Hecho | La clave se obtiene de `challenge ^ serverId ^ messageAcknowledge`. |
| Huffman fuera de banda | Hecho | Los paquetes de conexión se comprimen a partir del byte 12. |

#### Motor: cliente

| Área | Estado | Notas |
|---|---|---|
| Cvars de `cgame` | Hecho | Se registran al iniciar, antes de leer la configuración. |
| Conexión | Hecho | Ticket de Steam en `getchallenge` y cvar `cl_steamId`. |
| Módulos del juego | Hecho | `iobin.pk3` contiene los módulos para cada arquitectura y funciona con servidores puros. |
| Sonido | Hecho | Registro, canales y conjunto de sonidos de Quake Live. |
| Temporización de cuadros | Hecho | Con `com_preciseFrameTiming`, la variación a 125 FPS baja de 0,34 a 0,01 ms. |
| Carga en hilos | Hecho | Imágenes y sonidos se cargan en paralelo; `campgrounds` tarda cerca de un segundo. |
| Pantalla completa sin bordes | Hecho | Disponible mediante `r_fullscreen 2`. |
| Demos | Hecho | Compatibilidad bidireccional con el cliente original. |
| Workshop | Hecho | Descarga los recursos que faltan antes de entrar a una partida. |
| Carteles del servidor | Hecho | Los cambios procedentes del Workshop se aplican únicamente en el servidor correspondiente. |
| Voz | Hecho | Comunicación entre pares mediante Steam; el módulo de juego determina quién puede escuchar a cada jugador. |
| Autocompletado de consola | Hecho | Incluye `callvote`, mapas, factories, nombres de jugadores y comandos de `cgame`. |
| Capturas | Hecho | Mantiene las rutas y los nombres del original, incluidos los `levelshots`. |
| Modo seguro | Hecho | Tras un cierre anormal, ofrece iniciar con los ajustes de sistema predeterminados. |
| Cliente de 64 bits | Hecho | `Release\\x64` es la compilación principal. |

#### Motor: servidor

| Área | Estado | Notas |
|---|---|---|
| Snapshots | Hecho | La serialización de entidades y jugadores coincide con Quake Live. |
| Bots | Hecho | Implementación en `sv_bot.c`. |
| Autenticación de Steam | Hecho | Valida el ticket mediante `BeginAuthSession`. Si el servidor no tiene Steam, permite conexiones de red local. |
| Carga del módulo de juego | Hecho | DLL nativa mediante la tabla `gameImport_t`. |
| Baneos | Hecho | Gestión por Steam ID mediante `access.txt`; `ban`, `tempban` y `unban` actualizan el archivo. |
| Estadísticas y RCON por ZeroMQ | Hecho | Incluye las cvars `zmq_*` del original. El RCON sin contraseña solo acepta conexiones locales. |
| Lista de servidores | Hecho | Gestionada por Steam, como en el original. |
| Heartbeat al master | Hecho | Utiliza el game server de Steam. `sv_master 0` lo retira de la lista. |
| Workshop | Hecho | Descarga los recursos indicados en `workshop.txt` antes de iniciar y los anuncia a los clientes. |
| `sv_fps` | Hecho | Fijado en 40, como en el original. |
| Protección contra spam | Hecho | Incluye `sv_floodProtect` y el contador de `userinfo` del módulo de juego. |
| Redirección por servidor lleno | Hecho | `sv_fullRedirect` deriva a los jugadores de ioquakelive a otro servidor con espacio. |
| Linux ARM | Hecho | Compilado y ejecutándose en `aarch64`. |

#### `cgame`

| Área | Estado | Notas |
|---|---|---|
| Eventos | Hecho | 100 manejadores `EV_`, incluidos Race, infección y premios. |
| Pantalla panorámica | Hecho | `CG_AdjustFrom640` admite los modos estirado, izquierda, centro y derecha. |
| Texto mediante `owner-draw` | Hecho | Cerca de 70 funciones pasan por `CG_DrawText`. |
| `serverinfo` | Hecho | Incluye los 29 campos de `CG_ParseServerinfo`. |
| Marcadores | Hecho | FFA, Duel, TDM y marcador final con el modelo del ganador. |
| HUD | Hecho | Disposición del original, con temporizadores de ítems y tarjeta del TAB. |
| Locutor, medallas y rondas | Hecho | Las seis medallas animadas utilizan 24 cuadros de CPMA. |
| Indicadores de daño | Hecho | Implementación completa de `CG_DrawDamagePlums`. |
| Mira | Hecho | Incluye la regla de ocultarla en múltiplos de 30. |
| Mensajes de muerte | Hecho | Mensajes del original con atacante, víctima e icono del arma. |
| Predicción y `pmove` | Hecho | `pmove` reconstruido a partir del binario. |
| `cl_timeNudge` | Hecho | Incluye la extrapolación de jugadores y el adelanto de proyectiles del original. |
| Clan tags | Hecho | Clave `cn` con sus 26 colores. |
| Efectos de armas | Hecho | Chispas de impacto, estilos de arma, zoom y viñeta mediante sus cvars. |
| Congelación | Hecho | Tres shaders de hielo según avanza el descongelamiento; incluye fragmentos con estela. |
| Infección | Hecho | Punto de interés, nombres de Red Rover y aspecto del jugador infectado. |
| Avisos de chat | Hecho | `cg_chatbeep` y `cg_teamChatBeep`. |
| Cámara de intermedio | Hecho | Incluye mapas sin `info_player_intermission`. |
| Cuerpo del jugador seguido | Hecho | Al morir sin desintegrarse, el espectador puede ver el cuerpo. |

#### Menús (`ui`)

| Área | Estado | Notas |
|---|---|---|
| Nuevas funciones exportadas | Hecho | `UI_REGISTER_CVARS`, `UI_CHECK_ACTIVE_MENU`, `UI_WALK_MENUS` y `UI_DRAW_ADVERTISEMENT`. |
| Sistema de fuentes (`fontIndex`) | Hecho | Los puntos de dibujo reciben `item->fontIndex`. |
| Carga de menús | Hecho | Los archivos de ioquakelive se incluyen en `pak01.pk3`, dentro de `ui/ioql/`, y funcionan en servidores puros. |
| Escalado de interfaz | Hecho | Se basa en la altura de pantalla y respeta las reglas del original para formatos 4:3 y 5:4. |
| Portada | Hecho | Menú en columna, fondo, avisos legales y lista de amigos; dentro de una partida, incorpora la barra verde. |
| Start Match | Hecho | Factories, selección de mapa y bots. |
| Lista de servidores | Hecho | Datos de Steam, columnas ordenables, filtros, imagen del mapa y ventana con información de la partida. |
| Settings | Hecho | 220 opciones en el orden del original, también accesibles durante una partida. |
| Training | Hecho | Crash Course, Accelerate y Elevate, además de la sala de tracking propia de ioquakelive. |
| Replays | Hecho | Cada demo grabada durante una partida aparece asociada a esa partida. |
| Career Stats | Hecho | Statistics y Medals a partir de las estadísticas de Steam. |
| Recent Matches | Hecho | El servidor envía el reporte de la partida mediante Steam y el cliente lo guarda. |
| Avisos legales | Hecho | El texto se obtiene del `web.pak` del jugador. |
| Marcadores (`.menu`) | Hecho | Marcadores individuales y por equipos. |
| Acciones de menú | Hecho | Las 13 acciones, incluidas `clientViewProfile`, `modPlayer`, `putspec` y `banPlayer`. |
| Votaciones desde el menú | Hecho | El voto de mapa utiliza el modo elegido en el menú, como en el original. |
| Perfil del jugador | Hecho | Career Stats, avatar de Steam y perfil en la superposición de Steam. |
| Amigos | Hecho | Lista de Steam, colores y opciones del original, como Join Match, Send Message y Send Invite. |
| Voto de mapa al terminar | Hecho | Requiere `mappool.txt`, como en el original. |

#### Módulo de juego (`qagame`)

| Área | Estado | Notas |
|---|---|---|
| Modos de juego | Hecho | FFA, Duel, TDM, CA, CTF, 1FCTF, Harvester, FT, DOM, A&D, Race y RR. |
| Físicas | Hecho | Control de aire, doble salto, deslizamiento agachado, auto-hop y escaleras. |
| Gancho | Hecho | Tracción hacia paredes y enemigos, frenado a corta distancia y amortiguación bajo el agua. |
| Armas | Hecho | Todas las armas de Quake Live, con sus tiempos de recarga. |
| Daño y salpicadura | Hecho | |
| Llaves y puertas | Hecho | Llaves plateada, dorada y maestra. |
| Calentamiento | Hecho | `g_gameState`, porcentaje de jugadores listos y abandono. |
| Votaciones | Hecho | Incluye el voto de mapa al terminar la partida. |
| Cvars | Hecho | Las 406 cvars del original, con sus funciones de cambio. |
| Factories | Hecho | Ejecutar `map` sin una factory muestra el uso, como en el original. |
| Race | Hecho | Checkpoints, cronometraje y HUD. |
| Compensación de lag | Hecho | Historial de posiciones y rebobinado. |
| Armadura y daño | Hecho | Armadura por niveles, perdigones en anillos y reducción del daño con la distancia. |
| Entrenamiento | Hecho | Implementado en `g_training.c`. |
| Loadouts | Hecho | Se aplican desde el menú al aparecer en la partida, incluida la munición. |
| Estadísticas JSON | Hecho | Genera los mismos documentos que el original mediante ZeroMQ. |
| Premium | No aplica | La build 1069 no tiene suscripciones. |
| Permisos | Hecho | `access.txt` gestiona los permisos de ban, mod y admin por Steam ID. |
| Comandos de administración | Hecho | Privilegios y comandos reconstruidos a partir del binario. |
| Entidades por archivo | Hecho | `sv_altEntDir` lee `<mapa>.ent` y `sv_dumpEntities` las escribe. |

#### Renderers

| Área | Estado | Notas |
|---|---|---|
| OpenGL clásico (`opengl1`) | Hecho | Basado en Quake3e, con las modificaciones de Quake Live. Es el renderer predeterminado. |
| Vulkan | Hecho | Basado en Quake3e, con el bloom y la corrección de color de Quake Live. |
| OpenGL 2 (`opengl2`) | Hecho | Renderer de respaldo basado en ioquake3. |
| Reflex y Anti-Lag 2 | Parcial | Disponibles en Vulkan mediante `r_lowLatency`. Reflex se ha medido en NVIDIA; Anti-Lag 2 aún no se ha probado en AMD. |
| FSR | Hecho | Disponible mediante `r_fsr`, únicamente en OpenGL clásico. |
| Shaders de Quake Live | Hecho | Las 134 palabras del parser original. |
| Shaders duplicados | Hecho | Prevalece la primera definición, como en el original. |
| Carteles | Hecho | El renderer añade la superficie correspondiente a cada cartel del mapa. |
| Cadena del gancho | Hecho | Utiliza `RT_RAIL_CORE`. |
| Hielo | Hecho | Los tres shaders se aplican al modelo congelado. |
| Bloom | Hecho | Utiliza los valores del original. |
| Texturas en hilos | Hecho | Carga de imágenes JPEG, TGA y PNG. |
| Cuadros en cola | Hecho | Control mediante `r_maxQueuedFrames`, con valor predeterminado de 0. |

## Compilar

El proyecto genera dos programas principales: `ioquakelive`, el cliente, e `ioqlds`, el servidor dedicado.

Antes de compilar, inicializa los submódulos:

```bash
git submodule update --init --recursive
```

### Windows

Se requiere Visual Studio 2026 o sus Build Tools, con el toolset v145.

```bat
cd misc\msvc
MSBuild ioquakelive.sln -p:Configuration=Release -p:Platform=x64
MSBuild ioqlds.vcxproj -p:Configuration=Release -p:Platform=x64
```

El servidor dedicado tiene un proyecto independiente de la solución, por lo que se compila por separado. Los binarios se generan en `build\Release\x64\`.

Las plataformas disponibles son `x64`, `Win32` y `ARM64`. Para compilar en ARM64, ejecuta antes `code\tools\Build-SDL3.ps1`.

Si modificas una estructura (`struct`) en un archivo de cabecera, utiliza `-t:Rebuild`: una compilación incremental puede conservar objetos desactualizados.

### Linux y macOS

```bash
make release
```

Se utiliza el Makefile de ioquake3. SDL3 debe estar instalado y disponible mediante `pkg-config`.

Los binarios generados son `ioquakelive`, `ioquakelive-vulkan` e `ioqlds`.

## Estructura del repositorio

```text
code/
  cgame/           Módulo de juego del cliente: HUD, marcadores, efectos y predicción
  game/             Módulo de juego del servidor: reglas, entidades y físicas
  ui/               Menús
  client/           Cliente del motor
  server/           Servidor del motor
  qcommon/          Código compartido: red, sistema de archivos, comandos y Steam
  renderercommon/   Código compartido entre renderers
  renderergl1/      OpenGL clásico
  renderergl2/      OpenGL 2
  renderervk/       Vulkan
  sdl/, sys/        Ventana, entrada y sistema operativo
  SDL3/             Submódulo
  tools/            Scripts de compilación y empaquetado

misc/msvc/          Solución y proyectos de Visual Studio
content/pak01/      Recursos propios de ioquakelive para generar pak01.pk3
content/server/     server.cfg y access.txt de ejemplo para ioqlds
```

## Créditos y licencia

Este proyecto se apoya en el trabajo de:

- [Thomas Jones — ioquakelive](https://github.com/tjone270/ioquakelive)
- [ioquake3](https://github.com/ioquake/ioq3)
- [Quake3e](https://github.com/ec-/Quake3e)
- [CPMA](https://playmorepromode.com/)

Otras dependencias y recursos:

- Emojis: Noto Emoji.
- Fuentes: `sans-serif` y Liberation Sans.
- SDL3, MoltenVK en macOS y fontstash con stb_truetype para la representación de texto.

Quake Live y sus recursos pertenecen a id Software. Este repositorio no incluye ni redistribuye los archivos originales del juego.

El código de este proyecto se distribuye bajo la licencia GPL v2.
