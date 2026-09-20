# ioquakelive

[English](#english) · [Español](#español)

## English

An implementation of the **Quake Live** game, built on the open-source [ioquake3](https://ioquake3.org/) engine. The goal is to produce a functionally equivalent open-source client/server to the final release of Quake Live (v1069).

#### Why?
It's really important to me that the Quake Live community prosper - it's such a great game; this is why I have put so much time and effort into working on this fork.

### Project Status

Work in progress, and much further along than it looks. Both programs build and run: `ioquakelive`, the client, and `ioqlds`, the dedicated server. The game modules are name-complete against retail, and the engine-side infrastructure that used to be missing -- Steam authentication, the Steam server list and heartbeat, Workshop downloads, Steam voice, ZeroMQ stats and rcon, the moderation and ban system -- is in.

The tables below were re-audited against the code and against the retail binaries. Every row says what was checked, and a row only says Done when the behaviour was read out of the binary and matched. Two measurements back them up, both reproducible with the tools in `re/tools/`:

| Module | Retail cvars found in this fork | Retail assets found | Missing |
|---|---|---|---|
| `qagame` | 405 / 406 (99.8%) | 108 / 115 | `fs_cdpath`; a few Q3-era assets |
| `cgame` | 363 / 365 (99.5%) | 374 / 427 | mostly `.tga` names we register as `.png` |
| `ui` | 201 / 211 (95.3%) | 114 / 120 | Q3 leftovers (`ui_cdkeyvalid`, `r_colorBits`) |

`python re/tools/extract_strings.py` regenerates that comparison; the per-item lists land in `notes/strings_gap.md`.

Symbol-level comparison is **not** possible for the game module: `qagamex64.so` is stripped down to 19 named C symbols (all in the stats/reporting area), so `gap_analysis.py` cannot measure coverage. Behaviour is verified by reading the binaries in Ghidra instead, and verified addresses are quoted in the source next to each port.

#### What works

- Quake Live clients can connect to the ioquakelive server, and this client can connect to Quake Live servers.
- QL network protocol (custom netchan, entity/player state field tables, usercmd layout).
- QL game module (`qagame`): all game types, the full cvar table, physics, weapons, items, warmup, voting, unlagged.
- QL cgame module: HUD, scoreboards, announcer, medals, damage plums, obituaries, effects, prediction.
- QL UI module: the whole front end rebuilt on the retail web UI's own structure, with 220 settings in retail's order.
- The Crash Course, ported whole: Crash spawns, the cinematic, his voice lines and the route.
- Font rendering via FreeType, widescreen coordinate system, `pak01.pk3` asset overrides.

#### What's required to run the game
You must have an original `pak00.pk3` file acquired from a legitimate purchased copy of [Quake Live](https://steampowered.com/app/282440) from Steam. ioquakelive will not run without a legitimate `pak00.pk3` file. Quake Live and its assets remain the property of *id Software*.

I do not have any rights to use any Quake Live game assets and cannot/**will not** redistribute these from my own legitimate copy.

#### Key differences from upstream ioquake3

| Area | ioquake3 (Q3) | ioquakelive (QL) |
|------|---------------|-------------------|
| Protocol | Q3 1.32 / ioq3 71 | QL build 1069's protocol 91 |
| Base game dir | `baseq3` | `baseq3` filesystem, `baseqz` protocol name |
| Game types | FFA, Tourney, TDM, CTF | + CA, FT, Race, DOM, A&D, Harvester, 1FCTF, RR (and all TA modes) |
| Physics | Q3 air control | + CPM air control, double jump, crouch slide, auto-hop |
| Font system | Scale-based (textFont/bigFont) | fontIndex-based (extraFonts[3]), plus TrueType faces from pak01 |
| Widescreen | Stretch only | 4:3 constrained with left/center/right modes for anchoring elements |
| Key/door system | N/A | IT_KEY items, key-locked func_door |
| Stats | Basic scoreboard | Detailed per-weapon accuracy, 'premium' scoreboards from QL |
| Front end | Q3 menu files | Quake Live's web UI rebuilt natively: home, Start Match, Training, Replays, 220 settings |

### Implementation progress

Status is one of **Done** (ported and verified against the binary), **Partial** (works, with a named gap) or **Pending** (not started or stubbed).

#### Network protocol

| Area | Status | Notes |
|------|--------|-------|
| Netchan format | Done | GENCHECKSUM removed to match QL packet header |
| Client message parsing | Done | Extra byte after 3 header longs in `SV_ExecuteClientMessage` |
| entityStateFields | Done | 58 entries, reordered, gravity as int32 |
| playerStateFields | Done | 58 entries, `pm_flags` 24-bit, `weaponPrimary`, 9 new QL fields |
| usercmd_t deltas | Done | Offsets 21-25 reordered, 2 generic byte fields added |
| Configstring flood fix | Done | Track `csUpdated[]` per client during `CS_PRIMED`, only resend changed |
| XOR netchan encoding | Done | `challenge ^ serverId ^ messageAcknowledge` key derivation |
| Huffman on OOB data | Done | `NET_OutOfBandData` compresses connect packets at byte 12 |

#### Client engine

| Area | Status | Notes |
|------|--------|-------|
| CG_REGISTER_CVARS call | Done | `VM_Call` before `CG_INIT` in `cl_cgame.c` |
| Connection protocol | Done | Steam auth in `getchallenge`, `NET_OutOfBandRaw` for binary payloads, `cl_steamId` cvar |
| DLL pak system | Done | Architecture-specific `iobin` pk3, pure server compatible |
| Sound system | Done | Registration, channels and the QL sound set ported from the binary |
| Frame timing | Done | `Sys_Microseconds` deadline, `com_preciseFrameTiming`; jitter 0.34 → 0.01 ms at 125 fps |
| Threaded loading | Done | Job system, parallel image/sound decode, shader-text index; campgrounds loads in ~1 s |
| Borderless fullscreen | Done | `r_fullscreen 2`, no video restart on alt-tab |
| Demo recording/playback | Partial | A demo recorded by retail plays back here, verified. The other direction -- a demo recorded here played by retail -- is untested |
| Download system | Done | QL's path is the Steam Workshop: the server publishes its items in `CS_STEAM_WORKSHOP_IDS` and the client fetches what it lacks (`CL_WorkshopDownloads`) before entering |
| VOIP / Steam voice | Done | Steam P2P voice, with the server as the bridge and the game module deciding who hears whom (`G_VoiceFilter`) |
| Console auto-complete | Done | `callvote`, the map and factory lists, player names and the cgame's own commands |
| Screenshot system | Done | Retail's path and naming, levelshot included (which writes a TGA, whatever its name said) |
| x86_64 client | Partial | Release\|x64 builds; only Win32 is played and tested here |

#### Server engine

| Area | Status | Notes |
|------|--------|-------|
| Snapshot system | Done | Entity/player state serialisation matches QL |
| Bot management | Done | `sv_bot.c` functional |
| Steam auth bypass | Done | `com_build 1` skips Steam GS init and auth validation |
| Game module loading | Done | Native DLL loading with `gameImport_t` function pointer table |
| Linux/ARM dedicated server | Done | Built and run on aarch64; a test server ran a multi-mode map pool |
| Ban system | Done | Retail bans by Steam ID from `access.txt` (`g_accessFile`, `/reload_access`), not through the engine's `SV_Ban_f`. The list is read at connect time, and ban, tempban and unban all write to it |
| ZMQ stats/rcon | Done | `zmq.c`: the rcon socket and the stats publisher, with retail's `zmq_*` cvars. A passwordless rcon only accepts local connections here, which is a deliberate divergence |
| Server browser protocol | Done | The list comes from Steam, as in retail, not from a master of our own |
| Master server heartbeat | Done | The Steam game server with `EnableHeartbeats`; `sv_master 0` takes the server out of the browser |
| Map download redirect | Done | Through the Workshop, which is how QL does it |
| Rate limiting | Done | Retail's leaky bucket (`sv_floodProtect` as a float, score up one per command and down one per second) plus the userinfo flood counter in the game module |

#### cgame (client-side game module)

| Area | Status | Notes |
|------|--------|-------|
| Event system | Done | 100 `EV_` handlers including race, infection, awards |
| Widescreen rendering | Done | `CG_AdjustFrom640` with `STRETCH`/`LEFT`/`CENTER`/`RIGHT` |
| Owner-draw text | Done | All ~70 owner-draw functions route through `CG_DrawText` |
| Serverinfo parsing | Done | 29 fields matching binary `CG_ParseServerinfo` |
| Scoreboards | Done | FFA, Duel and TDM boards ported from the binary, plus the end-of-match board with the winner's model |
| HUD | Done | Retail's layout, including the item timers and the TAB card |
| Announcer, medals, rounds | Done | `50f920da`; the six animated medals use CPMA's 24-frame sets |
| Damage plums | Done | `CG_DrawDamagePlums` ported whole: the pool, not just plums |
| Crosshair | Done | `0x1000c2a0`, including the "no crosshair on multiples of 30" rule |
| Obituary feed | Done | Attacker/victim names with weapon icons |
| Prediction/pmove | Done | Whole pmove rewritten from the binary; 9 verified fixes on top |
| Clan tags | Done | `0x1003e640`, the `cn` key with its 26 colours |
| Impact sparks, weapon styles, zoom, vignette | Done | Ported with cvars |
| Advertisements | Done | `CG_DrawAdvertisements` |
| Freeze/thaw effects | Done | The three ice shaders on the frozen player, picked by `generic1` as he thaws, and the shards with their ice trail (`CG_IceTrail`) |
| Infection visuals | Done | The Red Rover POI and names, and the infected: slime over the model, the hum, and no weapon |
| Chat beep sounds | Done | `cg_chatbeep` and `cg_teamChatBeep` with `sound/player/talk.ogg`. The cvar is lower-case `b`, which is why it read as missing |
| Intermission camera | Done | `FindIntermissionPoint` / `MoveClientToIntermission` against the binary (0x10054f16), including the fallback when a map has no `info_player_intermission` |

#### UI (user interface module)

| Area | Status | Notes |
|------|--------|-------|
| New exports | Done | `UI_REGISTER_CVARS`, `UI_CHECK_ACTIVE_MENU`, `UI_WALK_MENUS`, `UI_DRAW_ADVERTISEMENT` |
| fontIndex support | Done | All 26+ DC call sites pass `item->fontIndex` |
| Cvars | Done | ~78 new cvars registered in `ui_main.c` |
| Menu file loading | Done | `pak01.pk3` override system |
| Front page | Done | Rebuilt natively: column menu, background art, credits, friends strip |
| Start Match | Done | Retail's own page: factories, map picker, bots |
| Settings | Done | 220 options in retail's order, taken from `web.pak`'s CSS and scripts |
| Training Center | Done | Crash Course, Accelerate, Elevate, with retail's commands |
| Replays | Done | Demo list and playback from the front end |
| Scoreboards (menu files) | Done | Non-team and team boards draw correctly |
| Server browser | Done | `ui_browser.c`: the list from Steam, sortable columns, filters and the map shot per row |
| UI script actions | Done | All 13 run their commands for real: `clientViewProfile`, `modPlayer`, `putspec`, `banPlayer` and the rest |
| Player profile display | Done | Through the Steam overlay, which is where retail sent it |
| Friend/social features | Done | The Steam friends list in the menu's strip and in `/friends`, with join, invite and view profile |
| Map voting UI | Done | The whole chain: the server builds `nextmaps`, the cgame loads `ui/endgamevote.menu` and the button votes. It needs a `mappool.txt`, as retail does |

#### Game module (server-side game logic)

| Area | Status | Notes |
|------|--------|-------|
| Game types | Done | FFA, Duel, TDM, CA, CTF, 1FCTF, Harvester, FT, DOM, A&D, Race, RR |
| Physics (bg_pmove) | Done | Rewritten from the binary: air control, double jump, crouch slide, auto-hop, ladders |
| Grapple hook | Done | Wall/enemy pull, close-range slowdown, water damping |
| Weapon definitions | Done | All QL weapons with correct reload times |
| Damage and splash | Done | Ported from the binary (`8e39202a`) |
| Key/door system | Done | `IT_KEY`, silver/gold/master keys, `func_door` spawnflags |
| Warmup state machine | Done | `g_gameState`, ready percentage, forfeit logic |
| Voting system | Done | `VF_ENDMAP_VOTING`, `IntermissionVote`, `nextmaps` parsing |
| CVAR table | Done | ~390 cvars with 56 `OnChanged` callbacks; 405 of retail's 406 present |
| Factories | Done | The engine parses and applies them; `map` without a factory prints the usage, as retail does |
| Race | Done | Checkpoints, timing and the race HUD |
| Unlagged (`lagHax`) | Done | Position history recording/rewinding |
| Tiered armor, shotgun rings, falloff | Done | `CheckArmor` rewrite and the weapon systems |
| Training mode | Done | `g_training.c`: `info_tour_point`, `G_TrainingFrame`, the AI nodes, `Cmd_SkipTraining` |
| Loadout system | Done | End to end: the server publishes the mode, the cgame builds the chosen weapon, pmove copies it and the spawn hands it over with its ammo |
| JSON stats reporting | Done | The documents are the ones retail builds, with the keys read out of the binary, and they go out over the ZeroMQ socket |
| Premium/subscription | N/A | Build 1069 has no subscription gate: `premium`, `subscription`, `paid` and `pro account` appear nowhere in the four binaries. id took it out when QL moved to Steam |
| Access control lists | Done | `g_access`: the `g_accessFile` file, with ban, mod and admin by Steam ID, rewritten on map change |
| Admin commands | Done | The privilege system and its commands rewritten against the binary: 16 vote gates let an admin through, `kick` and `clientkick` are separate branches, numeric arguments are validated |
| Map entities override | Done | `sv_altEntDir` reads `<dir>/<map>.ent` instead of the BSP, and `sv_dumpEntities` writes them out |

#### Renderer

| Area | Status | Notes |
|------|--------|-------|
| RT_RAIL_CORE | Done | Grapple hook chain |
| Threaded texture decode | Done | JPEG/TGA/PNG decoded on worker threads |
| GPU frame fence | Done | `r_maxQueuedFrames`, default 0 to match retail's latency |
| QL-specific shaders | Done | The parser's whole vocabulary pulled out of the engine binary -- 134 words -- and checked against `tr_shader.c`. One was missing: `nobotclip`, which Quake 3 calls `botclip`. `nodefault` and `depthTest disable` are in |
| Freeze shader | Done | Three ice shaders over the model, chosen by how far the thaw has got |
| Post-processing (bloom) | Done | `tr_qlpost.c`, with retail's own values |

### What is still open

The tables above are per area. These are the specific things known to be missing or
unverified, and they are the whole list:

- **Playing against retail, both ways.** A retail client connects here and this client
  connects to a retail server, but neither has been played through a full match.
- **Demos across the two.** A retail demo plays back here. A demo recorded here has not
  been opened by retail.
- **The smart third-person camera** (`cg_thirdPersonSmooth`). Measured in the binary at
  0x1004c6f0, not ported: the tail of that function does not decompile cleanly.
- **`vmCvar_t` has one field fewer here than in retail.** The binary reads the string at
  +0x14 and this tree has it at +0x10. Nothing breaks today, because the engine and the
  modules share this definition; it matters the day a retail module is loaded by this
  engine. Recorded deliberately, not changed.
- **33 engine cvars from retail are not registered here.** All of them Quake 3 leftovers
  that QL kept and nothing reads.
- **Only Win32 is played.** `Release|x64` and the Linux and macOS makefiles build, and the
  dedicated server has been run on aarch64, but Win32 is what gets tested.

### Building

This builds two programs: **`ioquakelive`**, the client, and **`ioqlds`**, the dedicated
server. They are the same engine, with the client half compiled out of the second one.

#### Windows (Visual Studio)

```
cd misc\msvc
MSBuild ioquakelive.sln -p:Configuration=Release -p:Platform=Win32
MSBuild ioqlds.vcxproj  -p:Configuration=Release -p:Platform=Win32
```

The dedicated server is a project of its own and is not in the solution, so it takes a
second call. Everything lands in `build\<config>\<platform>\`.

Individual projects: `cgame.vcxproj`, `ui.vcxproj`, `qagame.vcxproj`, `ioquakelive.vcxproj`,
`opengl2.vcxproj`, `ioqlds.vcxproj`

**Toolset:** v145 (VS 2022+), **Platform:** Win32/x64. After changing a struct in a header,
build with `-t:Rebuild`: the incremental build leaves stale objects.

#### Linux / macOS

Standard ioquake3 Makefile build; the binaries are `ioquakelive` and `ioqlds`. The dedicated
server has been built and run on Linux aarch64.

### Releases

A release is cut from `main` by the commit message, not by hand:

```
git commit --allow-empty -m "release: 1.0.0"
git push origin main
```

CI builds every platform, tags the commit `v1.0.0` and publishes a GitHub Release.
Client and dedicated server come out of one build but are two things to install, so
they are two downloads:

```
ioquakelive-windows-x64.zip     the client: engine, renderer, game modules, baseq3/
ioquakelive-linux-x86_64.zip
ioquakelive-macos-arm64.zip     an ioquakelive.app bundle, with ioqlds inside it

ioqlds-windows-x64.zip          the dedicated server: ioqlds, baseq3/, and a
ioqlds-linux-x86_64.zip         sample server.cfg and access.txt to edit

ioquakelive-<version>-src.zip   the source that produced them, as GPL v2 requires
```

None of them contains `pak00.pk3`. Unzip, drop your own `pak00.pk3` into `baseq3/`
and run the executable. On Windows the client does that step for you: if `pak00.pk3`
is missing it finds Quake Live in your Steam library and offers to copy it.

There is no updater and nothing phones home. A new version is a new download.

The macOS bundle is signed and notarised only when the Apple secrets
(`MACOS_CERTIFICATE_P12` and friends) are set on the repository; without them the
job still runs and ships an unsigned bundle.

### Directory Layout

```
code/
  cgame/          Client-side game module (HUD, scoreboard, effects, prediction)
  game/           Server-side game module (game logic, entities, physics)
  ui/             User interface module (menus, server browser)
  client/         Engine client code
  server/         Engine server code
  qcommon/        Shared engine code (networking, filesystem, commands)
  renderercommon/ Shared renderer code
  renderergl2/    OpenGL 2 renderer
misc/
  msvc/           Visual Studio solution and project files
content/pak01/    Asset overrides (menus, fonts, shaders) → pak01.pk3
```

## Español

Una implementación del juego **Quake Live**, construida sobre el motor de código abierto [ioquake3](https://ioquake3.org/). El objetivo es producir un cliente y un servidor de código abierto funcionalmente equivalentes a la versión final de Quake Live (v1069).

#### ¿Por qué?
Me importa de verdad que la comunidad de Quake Live prospere — es un juegazo; por eso le puse tanto tiempo y esfuerzo a este fork.

### Estado del proyecto

En obra, y mucho más avanzado de lo que parece. Los dos programas compilan y corren: `ioquakelive`, el cliente, e `ioqlds`, el servidor dedicado. Los módulos de juego están completos contra retail, y la infraestructura del lado del motor que faltaba —autenticación por Steam, la lista de servidores y el latido al master de Steam, descargas del Workshop, voz por Steam, estadísticas y rcon por ZeroMQ, el sistema de moderación y baneos— ya está.

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
- El módulo de interfaz de QL: el front end entero rehecho sobre la estructura de la web UI de retail, con 220 opciones en el orden de retail.
- El Crash Course, portado entero: Crash aparece, la cinemática, sus líneas con voz y el recorrido.
- Tipografía por FreeType, sistema de coordenadas panorámico, sobreescritura de assets por `pak01.pk3`.

#### Qué hace falta para correrlo
Necesitás un `pak00.pk3` original, sacado de una copia **comprada** de [Quake Live](https://steampowered.com/app/282440) en Steam. ioquakelive no arranca sin un `pak00.pk3` legítimo. Quake Live y sus assets siguen siendo propiedad de *id Software*.

No tengo ningún derecho sobre los assets de Quake Live y no puedo —ni **voy a**— redistribuirlos desde mi propia copia legítima.

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
| Front end | archivos `.menu` de Q3 | la web UI de Quake Live rehecha de forma nativa: portada, Start Match, Training, Replays, 220 opciones |

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
| Grabación y reproducción de demos | Parcial | Una demo grabada por retail se reproduce acá, verificado. Al revés —una demo grabada acá abierta por retail— sin probar |
| Sistema de descargas | Hecho | la vía de QL es el Workshop de Steam: el servidor publica sus items en `CS_STEAM_WORKSHOP_IDS` y el cliente baja lo que le falta (`CL_WorkshopDownloads`) antes de entrar |
| VOIP / voz de Steam | Hecho | voz por la red entre pares de Steam, con el servidor de puente y el módulo de juego decidiendo quién oye a quién (`G_VoiceFilter`) |
| Autocompletado de consola | Hecho | `callvote`, las listas de mapas y factories, nombres de jugadores y las órdenes del propio cgame |
| Sistema de capturas | Hecho | la ruta y el nombre de retail, levelshot incluido (que escribe un TGA, dijera lo que dijera su nombre) |
| Cliente x86_64 | Parcial | `Release\|x64` compila; sólo Win32 se juega y se prueba acá |

#### Motor: servidor

| Área | Estado | Notas |
|------|--------|-------|
| Sistema de instantáneas | Hecho | la serialización de entity/player state coincide con QL |
| Gestión de bots | Hecho | `sv_bot.c` funcional |
| Autenticación de Steam | Hecho | el servidor le pasa el ticket a Steam con `BeginAuthSession` y echa al que no pasa; sin Steam del lado del servidor se entra igual, que es lo único que deja jugar en red local |
| Carga del módulo de juego | Hecho | carga de DLL nativa con la tabla de punteros `gameImport_t` |
| Sistema de baneos | Hecho | retail banea por Steam ID desde `access.txt` (`g_accessFile`, `/reload_access`), no por el `SV_Ban_f` del motor. La lista se lee al conectar, y ban, tempban y unban escriben en ella |
| Estadísticas y rcon por ZMQ | Hecho | `zmq.c`: el socket de rcon y el publicador de estadísticas, con las cvars `zmq_*` de retail. Un rcon sin contraseña acá sólo acepta conexiones locales, que es una divergencia deliberada |
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
| Replays | Hecho | lista de demos y reproducción desde el front end |
| Marcadores (archivos .menu) | Hecho | los de equipo y los de no-equipo dibujan correctamente |
| Navegador de servidores | Hecho | `ui_browser.c`: la lista desde Steam, columnas que ordenan, filtros y el cuadrito del mapa por fila |
| Acciones de scripts de UI | Hecho | los 13 ejecutan sus órdenes de verdad: `clientViewProfile`, `modPlayer`, `putspec`, `banPlayer` y el resto |
| Perfil del jugador | Hecho | por la superposición de Steam, que es adonde mandaba retail |
| Amigos y funciones sociales | Hecho | la lista de amigos de Steam en la franja del menú y en `/friends`, con entrar, invitar y ver perfil |
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
| Shaders específicos de QL | Hecho | el vocabulario entero del parser sacado del binario del motor —134 palabras— y contrastado contra `tr_shader.c`. Faltaba una: `nobotclip`, que Quake 3 llama `botclip`. `nodefault` y `depthTest disable` están |
| Shader de congelado | Hecho | tres shaders de hielo sobre el modelo, elegidos según cuánto avanzó el descongelado |
| Post-procesado (bloom) | Hecho | `tr_qlpost.c`, con los valores de retail |

### Lo que sigue abierto

Las tablas de arriba van por área. Esto es lo concreto que se sabe que falta o que no está verificado, y es la lista entera:

- **Jugar contra retail, en los dos sentidos.** Un cliente de retail entra acá y este cliente entra a un servidor de retail, pero ninguno de los dos se jugó una partida completa.
- **Las demos cruzadas.** Una demo de retail se reproduce acá. Una demo grabada acá no la abrió retail todavía.
- **La cámara suave en tercera persona** (`cg_thirdPersonSmooth`). Medida en el binario en 0x1004c6f0, sin portar: la cola de esa función no decompila limpio.
- **`vmCvar_t` tiene un campo menos acá que en retail.** El binario lee la cadena en +0x14 y este árbol la tiene en +0x10. Hoy no rompe nada, porque el motor y los módulos comparten esta definición; importa el día que un módulo de retail cargue en este motor. Anotado a propósito, sin cambiar.
- **33 cvars de motor de retail no están registradas acá.** Todas restos de Quake 3 que QL conservó y que nadie lee.
- **Sólo se juega Win32.** `Release|x64` y los makefiles de Linux y macOS compilan, y el servidor dedicado corrió en aarch64, pero Win32 es lo que se prueba.

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

El CI compila todas las plataformas, etiqueta el commit `v1.0.0` y publica un Release en GitHub. El cliente y el servidor dedicado salen de una sola compilación pero son dos cosas de instalar, así que son dos descargas:

```
ioquakelive-windows-x64.zip     el cliente: motor, renderer, módulos de juego, baseq3/
ioquakelive-linux-x86_64.zip
ioquakelive-macos-arm64.zip     un paquete ioquakelive.app, con ioqlds adentro

ioqlds-windows-x64.zip          el servidor dedicado: ioqlds, baseq3/, y un
ioqlds-linux-x86_64.zip         server.cfg y un access.txt de ejemplo para editar

ioquakelive-<versión>-src.zip   el código que los produjo, como exige la GPL v2
```

Ninguno trae `pak00.pk3`. Se descomprime, se pone el `pak00.pk3` propio en `baseq3/` y se ejecuta. En Windows el cliente hace ese paso solo: si falta `pak00.pk3`, busca Quake Live en tu biblioteca de Steam y ofrece copiarlo.

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
