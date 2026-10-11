# ioquakelive

ioquakelive es el **Quake Live original** hecho de nuevo sobre [ioquake3](https://ioquake3.org/), un motor de código abierto. La meta es tener un cliente y un servidor abiertos que funcionen igual que la última versión de Quake Live (build 1069), y después sumarle mejoras y calidad de vida. También puede ser el primer paso para un mod grande, para otro fork o para versiones mejores a futuro.

## ¿Por qué?

El proyecto lo empezó [Thomas Jones](https://github.com/tjone270/ioquakelive). Él descifró el protocolo y la estructura de los módulos, y logró que un cliente y un servidor de Quake Live compilaran sobre ioquake3. Este repositorio sigue desde ahí.

## Cómo probarlo

**Solo funciona si tienes Quake Live comprado en Steam.** ioquakelive no trae los archivos del juego, los saca de tu instalación.

1. Abre Steam con la cuenta que tiene Quake Live. La lista de servidores, los amigos y la entrada a los servidores pasan por Steam.
2. Baja la última versión desde [Releases](https://github.com/foressssst/ioquakelive/releases). Para Windows es `ioquakelive-windows-x64.zip`.
3. Descomprímela en cualquier carpeta.
4. Abre `ioquakelive.x86_64.exe`. Para jugar con Vulkan abre `ioquakelive-vulkan.x86_64.exe`, que es el mismo juego.

Para tener en cuenta:

- La primera vez, en Windows, el juego busca Quake Live en tu biblioteca de Steam y copia `pak00.pk3` a su carpeta `baseq3`. Si no lo encuentra, cópialo tú desde `steamapps\common\Quake Live\baseq3`.
- Los ejecutables no van firmados, así que Windows Defender a veces bloquea la descarga o avisa de algo terminado en `!ml`. Es un falso positivo. Se permite en Seguridad de Windows, en Historial de protección.
- Tu configuración, tus demos y tus capturas quedan en la carpeta `baseq3` del juego, no en tu perfil de Windows.
- No se actualiza solo. Para pasar a otra versión, baja el zip nuevo y descomprímelo encima. Tu configuración no se toca.
- Las pre-releases traen solo Windows. Linux y macOS salen en las releases completas.

Si algo falla, abre un issue acá. Cuenta qué versión tienes y qué estabas haciendo. La versión sale en la consola, que se abre con la tecla que está a la izquierda del 1.

## Lo nuevo

Lo más reciente, de la 0.3.4 a la 0.3.13. El detalle de cada versión está en [Releases](https://github.com/foressssst/ioquakelive/releases).

- **Menús iguales a los del original.** La portada, la lista de servidores, Settings y el resto se comparan contra la web de Quake Live y se ajustan al píxel. En 4:3 y 5:4 la interfaz ya escala como en el original.
- **Settings de retail**, con las opciones nuevas de ioquakelive puestas en su lugar. Ahora también se abre bien dentro de una partida.
- **Workshop como en Quake Live**, en el cliente y en el servidor. `ioqlds` baja lo que pide `workshop.txt` antes de arrancar.
- **Sala de tracking en el Training Center.** Un bot que esquiva, se acerca, se aleja y cambia de ritmo según cuánto le pegas, para practicar la LG.
- **Vulkan más estable.** Se arregló el menú en negro y el corte al pasar por un teleport.
- **Servidor lleno.** Si el servidor tiene otros configurados, a los jugadores de ioquakelive los manda solos al que tenga lugar (`sv_fullRedirect` en el servidor, `cl_followRedirect` en el cliente).
- **El servidor aguanta más gente** sin caerse, y los hitsounds ya no se cortan con el servidor lleno.
- Arreglos varios: la escopeta en servidores de retail, `cl_timeNudge`, las sombras de los jugadores, el locutor, el alt-tab, el voto de mapa desde el menú y el Main Menu dentro de una partida.

## Compatibilidad

### Sistemas

| Sistema | Estado |
|---|---|
| Windows de 64 bits | La versión principal y la más probada |
| Windows de 32 bits | Funciona |
| Linux x86_64 | Compila y sale en las releases completas |
| Linux ARM64 | Compila; el servidor está probado en una máquina ARM |
| Windows ARM64 y macOS con Apple Silicon | Compilan y se empaquetan, pero no se han probado en un equipo real |

### Con el Quake Live original

- Juegas en los mismos servidores de Quake Live, incluidos los que usan minqlx.
- Las demos sirven para los dos lados: las del original se ven en ioquakelive y las grabadas acá se ven en el original.
- El Workshop es el mismo. Antes de entrar a un servidor, el cliente baja los mapas que le faltan.
- Las cvars y los binds son los de Quake Live, así que tu config sirve si la copias a `baseq3`.
- Falta probar que el cliente del Quake Live original entre a un servidor `ioqlds`.

### Video

- Tres renderers: OpenGL, que viene por defecto, Vulkan y OpenGL 2 de respaldo.
- FSR en Settings > Video, solo con OpenGL.
- Reflex en tarjetas NVIDIA y Anti-Lag 2 en AMD, con Vulkan. Reflex está medido; Anti-Lag 2 todavía no se probó en una tarjeta AMD.
- Para G-SYNC y FreeSync, `com_maxfpsVRR 1` deja los fps justo bajo la frecuencia del monitor.
- Pantallas anchas, 4:3 y 5:4.

## Lo que agrega sobre el original

**Rendimiento y video**

- Los mapas cargan más rápido, porque las imágenes y los sonidos se leen en varios hilos.
- Pantalla completa sin bordes con `r_fullscreen 2`. La ventana cambia de tamaño sin reiniciar el video.
- `r_picmipExplosions 1` deja las explosiones de cohete y granada con picmip, como en Quake 3.
- Versiones para ARM64 en Windows y Linux.

**HUD y juego**

- Medallas y chat animados, con los cuadros de CPMA. El chat animado se apaga con `ch_animateScroll 0`.
- `ch_rewardY`, `ch_fragMessageY` y `ch_fragMessageScale` mueven la medalla y el mensaje de frag.
- `s_playSoundVolume`, de 0 a 2, ajusta el volumen de los sonidos que mandan los servidores.
- Emojis a color en el chat, la consola y los nombres.
- Los carteles de cada servidor se quedan en ese servidor. En el original quedan pegados al pasar a otro.
- Una sala de tracking en el Training Center.

**Demos y espectador**

- `rewind`, `fastforward` y `seekdemo` para moverte por una demo, y `fragforward` para saltar al siguiente frag.
- `cg_xray` muestra a los jugadores a través de las paredes, en demos y como espectador.

**Consola**

- `help <comando o cvar>` explica qué hace, y `searchconsole` busca texto en la consola.
- `frametimes` mide el tiempo real entre cuadros, e `inputrate` cuántos datos manda tu mouse por segundo.
- La consola se anima al abrirse. `con_animate 0` la deja quieta.
- En los menús, la consola y las demos se usa la flecha del sistema.

## Lo que falta

- Parte de la IA de los bots no está revisada contra el original: los roles de equipo en CTF y A&D, y las órdenes por chat de voz.
- La página escondida "???" del menú principal.
- Las capturas de pantalla no quedan asociadas a su partida. Las demos sí.
- Algunos mensajes de error y de sistema todavía son los de la primera versión del fork, no los del original.

Reportados y en revisión:

- Cortes cortos después de un shuffle en algunos servidores.
- Una demo del mapa 13night que cierra el juego.

## Servidor dedicado

`ioqlds` viene en su propio zip, con un `server.cfg` y un `access.txt` de ejemplo en `baseq3`. Igual que el cliente, necesita el `pak00.pk3` de Quake Live en esa carpeta. Si actualizas descomprimiendo encima, guarda antes tu `server.cfg` y tu `access.txt`, porque el zip trae los de ejemplo.

```
ioqlds +set dedicated 2
```

`dedicated 1` es para red local y `dedicated 2` para internet. Al arrancar lee `baseq3/server.cfg`, igual que el del original.

Los mapas del Workshop se piden en `baseq3/workshop.txt`, un número de ítem por línea. Las líneas que empiezan con `#` son comentarios. El servidor los baja antes de cargar el primer mapa y los anuncia a los clientes, que los bajan solos al entrar. `sv_workshopFile` cambia el nombre de ese archivo, y `steam_downloadugc <id>` baja un ítem con el servidor andando.

## Para desarrolladores

### Estado

Los tres módulos del juego (`qagame`, `cgame` y `ui`) están portados desde los binarios de la build 1069. Cada función portada lleva anotadas las direcciones del binario contra las que se revisó en Ghidra.

Para tener una idea del avance, esta tabla cuenta cuántos nombres de cvars y de archivos del módulo original aparecen también acá. No dice si se comportan igual; eso se revisa en el binario.

| Módulo | Cvars | Archivos |
|---|---|---|
| `qagame` | 405 de 406 | 114 de 115 |
| `cgame` | 364 de 365 | 389 de 427 |
| `ui` | 202 de 211 | 119 de 120 |

Lo que falta son casi todos archivos `.tga` que acá se cargan con otra extensión, y cvars que quedaron de Quake 3 o que solo cambian en mayúsculas. Hay además un detalle interno: `vmCvar_t` tiene un campo menos que en el original. Solo importaría para cargar un módulo original y no afecta al juego.

### Diferencias con ioquake3

| Área | ioquake3 (Quake 3) | ioquakelive (Quake Live) |
|------|--------------------|--------------------------|
| Protocolo | Q3 1.32 / ioq3 71 | el 91 de la build 1069 |
| Carpeta base | `baseq3` | `baseq3` en disco, `baseqz` como nombre de protocolo |
| Modos de juego | FFA, Tourney, TDM, CTF | además CA, FT, Race, DOM, A&D, Harvester, 1FCTF, RR y los de Team Arena |
| Físicas | control de aire de Q3 | además control de aire de CPM, doble salto, deslizamiento agachado y auto-hop |
| Texto | fuentes de mapa de bits, por escala | por índice de fuente, con tipografías TrueType dibujadas con fontstash |
| Panorámico | solo estirado | 4:3 anclado a la izquierda, al centro o a la derecha |
| Llaves y puertas | no existen | ítems `IT_KEY` y `func_door` con llave |
| Estadísticas | marcador básico | precisión por arma y los marcadores de Quake Live |
| Menús | archivos `.menu` de Q3 | los menús web de Quake Live hechos de nuevo dentro del juego |
| Cuenta y servidores | master server propio | Steam |

### Avance por área

**Hecho**: portado y revisado contra el binario. **Parcial**: funciona, y lo que falta está anotado. **No aplica**: el original tampoco lo tiene.

<details>
<summary>Ver las tablas</summary>

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

### Cómo se publica una versión

El CI de GitHub compila y publica según el mensaje del commit:

```
git commit -m "release: 1.0.0"
git push origin main
```

- `release: X` compila todas las plataformas, crea el tag `vX` y publica la release.
- `prerelease: X` compila solo Windows x86 y x64 y lo publica como pre-release. Es para parches chicos.

En los dos casos, `PRODUCT_VERSION` en `code/qcommon/q_shared.h` tiene que decir la misma versión.

El cliente y el servidor se bajan por separado:

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

El paquete de macOS se firma y se notariza solo si los secretos de Apple están cargados en el repositorio. Si no, se publica sin firmar.

`pak01.pk3` lleva además arte que este repositorio no puede incluir: la tipografía Handel Gothic Ex, los cuadros de las medallas y del globo de chat de CPMA, y el fondo y los logos del menú. El CI lo saca de un repositorio privado. Sin eso el juego compila y funciona igual, con las medallas quietas y otra tipografía en algunos textos.

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

- [Thomas Jones](https://github.com/tjone270/ioquakelive) empezó este fork.
- [ioquake3](https://github.com/ioquake/ioq3) es el motor de base.
- [Quake3e](https://github.com/ec-/Quake3e) aporta los renderers de OpenGL clásico y de Vulkan.
- De CPMA vienen los cuadros de las medallas animadas y del globo de chat.
- Los emojis son los de Noto Emoji, y el texto en `sans-serif` de los menús usa Liberation Sans.
- También se usan SDL3, MoltenVK en macOS, y fontstash con stb_truetype para el texto.

Quake Live y sus archivos son propiedad de id Software. Este repositorio no los incluye ni los redistribuye.

El código se distribuye bajo la GPL v2, igual que ioquake3. El texto está en `COPYING.txt`. Abajo queda el README original de ioquake3.

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
