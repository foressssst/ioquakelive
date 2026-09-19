# ioquakelive

An implementation of the **Quake Live** game, built on the open-source [ioquake3](https://ioquake3.org/) engine. The goal is to produce a functionally equivalent open-source client/server to the final release of Quake Live (v1069).

#### Why?
It's really important to me that the Quake Live community prosper - it's such a great game; this is why I have put so much time and effort into working on this fork.

## Project Status

**Work in progress, and much further along than it looks.** The game modules are close to name-complete against retail; what is left is mostly engine-side infrastructure (master server, stats, moderation) and a handful of visual systems.

The tables below were **re-audited against the code and against the retail binaries on 2026-09-19**. Every row says what was checked. Two measurements back them up, both reproducible with the tools in `re/tools/`:

| Module | Retail cvars found in this fork | Retail assets found | Missing |
|---|---|---|---|
| `qagame` | 405 / 406 (99.8%) | 108 / 115 | `fs_cdpath`; a few Q3-era assets |
| `cgame` | 363 / 365 (99.5%) | 374 / 427 | mostly `.tga` names we register as `.png` |
| `ui` | 201 / 211 (95.3%) | 114 / 120 | Q3 leftovers (`ui_cdkeyvalid`, `r_colorBits`) |

`python re/tools/extract_strings.py` regenerates that comparison; the per-item lists land in `notes/strings_gap.md`.

Symbol-level comparison is **not** possible for the game module: `qagamex64.so` is stripped down to 19 named C symbols (all in the stats/reporting area), so `gap_analysis.py` cannot measure coverage. Behaviour is verified by reading the binaries in Ghidra instead, and verified addresses are quoted in the source next to each port.

### What works

- Quake Live clients can connect to the ioquakelive server, and this client can connect to Quake Live servers.
- QL network protocol (custom netchan, entity/player state field tables, usercmd layout).
- QL game module (`qagame`): all game types, the full cvar table, physics, weapons, items, warmup, voting, unlagged.
- QL cgame module: HUD, scoreboards, announcer, medals, damage plums, obituaries, effects, prediction.
- QL UI module: the whole front end rebuilt on the retail web UI's own structure, with 220 settings in retail's order.
- The Crash Course, ported whole: Crash spawns, the cinematic, his voice lines and the route.
- Font rendering via FreeType, widescreen coordinate system, `pak01.pk3` asset overrides.

### What's required to run the game
You must have an original `pak00.pk3` file acquired from a legitimate purchased copy of [Quake Live](https://steampowered.com/app/282440) from Steam. ioquakelive will not run without a legitimate `pak00.pk3` file. Quake Live and its assets remain the property of *id Software*.

I do not have any rights to use any Quake Live game assets and cannot/**will not** redistribute these from my own legitimate copy.

### Key differences from upstream ioquake3

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

## Implementation progress

Status is one of **Done** (ported and verified against the binary), **Partial** (works, with a named gap) or **Pending** (not started or stubbed).

### Network protocol

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

### Client engine

| Area | Status | Notes |
|------|--------|-------|
| CG_REGISTER_CVARS call | Done | `VM_Call` before `CG_INIT` in `cl_cgame.c` |
| Connection protocol | Done | Steam auth in `getchallenge`, `NET_OutOfBandRaw` for binary payloads, `cl_steamId` cvar |
| DLL pak system | Done | Architecture-specific `iobin` pk3, pure server compatible |
| Sound system | Done | Registration, channels and the QL sound set ported from the binary |
| Frame timing | Done | `Sys_Microseconds` deadline, `com_preciseFrameTiming`; jitter 0.34 → 0.01 ms at 125 fps |
| Threaded loading | Done | Job system, parallel image/sound decode, shader-text index; campgrounds loads in ~1 s |
| Borderless fullscreen | Done | `r_fullscreen 2`, no video restart on alt-tab |
| Demo recording/playback | Pending | Records and plays, but never verified against QL protocol demos |
| Download system | Pending | ioquake3's cURL redirect is in; QL's own download path not audited |
| VOIP / Steam voice | Pending | QL uses Steam P2P voice; `cl_voip` is still ioquake3's |
| Console auto-complete | Pending | QL-specific commands/cvars not registered for tab-complete |
| Screenshot system | Pending | QL screenshot path/naming conventions not audited |
| x86_64 client | Pending | Only Win32 is built and tested here |

### Server engine

| Area | Status | Notes |
|------|--------|-------|
| Snapshot system | Done | Entity/player state serialisation matches QL |
| Bot management | Done | `sv_bot.c` functional |
| Steam auth bypass | Done | `com_build 1` skips Steam GS init and auth validation |
| Game module loading | Done | Native DLL loading with `gameImport_t` function pointer table |
| Linux/ARM dedicated server | Done | Built and run on aarch64; a test server ran a multi-mode map pool |
| Ban system | Pending | `SV_Ban_f` / `SV_BanNum_f` still print "Not yet implemented" (`sv_ccmds.c:670,704`) |
| ZMQ stats/rcon | Pending | No ZeroMQ anywhere in the tree; QL published stats and took rcon over it |
| Server browser protocol | Pending | Only ioquake3's `getservers`; QL used Valve's query protocol |
| Master server heartbeat | Pending | No heartbeat in `sv_main.c`; QL used Steam master servers |
| Map download redirect | Pending | HTTP redirect for missing maps not implemented |
| Rate limiting | Pending | QL-specific flood protection tuning not audited |

### cgame (client-side game module)

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
| Freeze/thaw effects | Partial | `CG_ThawPlayer` spawns the shards; the frozen-player shader is not done |
| Infection visuals | Partial | The Red Rover POI and names are in; the infected look is not |
| Chat beep sounds | Pending | No `cg_chatBeep` anywhere |
| Intermission camera | Pending | QL intermission camera behaviour not verified |

### UI (user interface module)

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
| Server browser | Pending | The list plumbing is exposed (`UI_Home_ServerList`) but there is no browser page and no master server to ask |
| UI script actions | Pending | 13 QL-specific scripts stubbed (clientViewProfile, modPlayer, putspec, banPlayer, ...) |
| Player profile display | Pending | Steam profile/avatar integration removed; needs replacement |
| Friend/social features | Pending | The strip is drawn but there is no friend list behind it |
| Map voting UI | Pending | Driven by the server; the UI side is not built |

### Game module (server-side game logic)

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
| Loadout system | Partial | Starting weapons and `weaponPrimary` honour it; the loadout menu is not built |
| JSON stats reporting | Partial | Original uses C++ jsoncpp; JSON emitters stubbed, non-JSON helpers preserved |
| Premium/subscription | Pending | QL's premium/pro checks need removing or stubbing deliberately |
| Access control lists | Pending | `g_accessFile` parsing present, Steam ID ACL untested |
| Admin commands | Pending | Privilege system present, admin actions untested |
| Map entities override | Pending | `sv_altEntDir` is not in the tree |

### Renderer

| Area | Status | Notes |
|------|--------|-------|
| RT_RAIL_CORE | Done | Grapple hook chain |
| Threaded texture decode | Done | JPEG/TGA/PNG decoded on worker threads |
| GPU frame fence | Done | `r_maxQueuedFrames`, default 0 to match retail's latency |
| QL-specific shaders | Pending | Shader keywords not fully audited |
| Freeze shader | Pending | The frozen-player look |
| Post-processing (bloom) | Pending | Partially implemented in opengl2, not QL-tuned |

## Building

### Windows (Visual Studio)

```
cd misc\msvc
MSBuild ioquakelive.sln -p:Configuration=Release -p:Platform=Win32
```

Individual projects: `cgame.vcxproj`, `ui.vcxproj`, `qagame.vcxproj`, `quakelive.vcxproj`, `opengl2.vcxproj`

**Toolset:** v145 (VS 2022+), **Platform:** Win32/x64. After changing a struct in a header, build with `-t:Rebuild`: the incremental build leaves stale objects.

### Linux / macOS

Standard ioquake3 Makefile build. The dedicated server has been built and run on Linux aarch64.

## Directory Layout

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
