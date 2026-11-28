<p align="center">
  <img src="misc/ioquakelive-banner.gif" width="100%" alt="ioquakelive">
</p>

<p align="center">
  <a href="README.md">Español</a> · <b>English</b>
</p>

<p align="center">
  <a href="https://github.com/foressssst/ioquakelive/releases/latest"><img src="https://img.shields.io/github/v/release/foressssst/ioquakelive?label=version&color=4073FF" alt="Latest version"></a>
  <a href="https://github.com/foressssst/ioquakelive/releases"><img src="https://img.shields.io/github/downloads/foressssst/ioquakelive/total?label=downloads&color=4073FF" alt="Downloads"></a>
  <a href="https://github.com/foressssst/ioquakelive/actions/workflows/build.yml"><img src="https://img.shields.io/github/actions/workflow/status/foressssst/ioquakelive/build.yml?branch=main&label=build" alt="Build"></a>
  <a href="COPYING.txt"><img src="https://img.shields.io/badge/license-GPL--2.0-4073FF" alt="GPL-2.0 license"></a>
</p>

<p align="center">
  <a href="https://github.com/foressssst/ioquakelive/releases/latest"><b>Download</b></a>
  &nbsp;·&nbsp;
  <a href="https://discord.gg/JKDWykm2Jy"><b>Discord</b></a>
  &nbsp;·&nbsp;
  <a href="https://quakeclub.com"><b>Quake Club</b></a>
</p>

ioquakelive is an open-source implementation of Quake Live, developed by the [Quake Club](https://quakeclub.com) team. It reproduces the last original version of the game (build 1069) on the [ioquake3](https://ioquake3.org/) engine, adding its own improvements without losing compatibility with the original game.

The project builds on the work of [Thomas Jones](https://github.com/tjone270/ioquakelive), whose implementation served as the foundation and inspiration for its continued development.

## How to play

You need a legitimate copy of [Quake Live](https://store.steampowered.com/app/282440/Quake_Live/) on Steam: ioquakelive does not include the original game files, it uses the ones from your own installation.

1. Download the installer for your operating system. All links point directly to the latest stable version.

   | System | Installer |
   | --- | --- |
   | Windows (x64) | [`ioquakelive-setup-windows-x64.exe`](https://github.com/foressssst/ioquakelive/releases/latest/download/ioquakelive-setup-windows-x64.exe) |
   | Windows (ARM64) | [`ioquakelive-setup-windows-arm64.exe`](https://github.com/foressssst/ioquakelive/releases/latest/download/ioquakelive-setup-windows-arm64.exe) |
   | Linux (x86_64) | [`ioquakelive-setup-linux-x86_64.tar.gz`](https://github.com/foressssst/ioquakelive/releases/latest/download/ioquakelive-setup-linux-x86_64.tar.gz) |
   | Linux (ARM64) | [`ioquakelive-setup-linux-arm64.tar.gz`](https://github.com/foressssst/ioquakelive/releases/latest/download/ioquakelive-setup-linux-arm64.tar.gz) |
   | macOS (Apple Silicon) | [`ioquakelive-setup-macos.zip`](https://github.com/foressssst/ioquakelive/releases/latest/download/ioquakelive-setup-macos.zip) |
   | macOS (Intel) | [`ioquakelive-setup-macos-x86_64.zip`](https://github.com/foressssst/ioquakelive/releases/latest/download/ioquakelive-setup-macos-x86_64.zip) |

2. Open the installer, choose the folder where you want to install the game and press Install.
3. Follow the setup steps. The launcher will automatically look for your Quake Live installation and offer to import your config, HUD and demos. You can also create a desktop shortcut.

Once installed, always start the game from the launcher, since it keeps the game up to date.

On Windows and Linux, the launcher starts the game with Vulkan if your computer supports it, and with OpenGL otherwise. On macOS it uses OpenGL.

If the launcher can't find your Quake Live installation, you can select the `pak00.pk3` file manually.

If you have questions or run into a problem, the [Quake Club Discord](https://discord.gg/JKDWykm2Jy) has a step-by-step installation guide and a support channel for ioquakelive.

## If your antivirus blocks it

ioquakelive is a fairly new project and its executables are not digitally signed yet, so your browser or Microsoft Defender may flag some versions as suspicious.

The source code is public and every version is built on GitHub straight from this repository. That's why we recommend downloading ioquakelive only from our [Releases](https://github.com/foressssst/ioquakelive/releases) page.

Before going past a security warning, check where the installer comes from by following the steps in "How each version is published".

If Microsoft Defender flags a file as a threat, check the details of the detection in the Windows protection history. If in doubt, ask us in our support channel on Discord.

## Features

- OpenGL, OpenGL 2 and Vulkan support.
- NVIDIA Reflex and AMD Anti-Lag 2 support on Vulkan.
- Multithreaded asset loading and unlocked FPS.
- Demo player improvements.
- Its own launcher that makes installing easy and keeps the game up to date on Windows, Linux and macOS.
- Quake Club integration.

## How each version is published

- Every version of ioquakelive is built with [GitHub Actions](https://github.com/foressssst/ioquakelive/actions), straight from the source code in this repository and in clean build environments.
- The installers come with a GitHub provenance attestation that lets you verify which build they come from. You can check it with [GitHub CLI](https://cli.github.com/):

  ```bash
  gh attestation verify ioquakelive-setup-windows-x64.exe -R foressssst/ioquakelive
  ```

- The launcher verifies every game file it downloads with SHA-256 before installing it.
- ioquakelive does not distribute the original Quake Live files. It uses the `pak00.pk3` file from your Steam installation, without modifying it.
- The game and the launcher connect to GitHub and Cloudflare to check for and download new versions, and to [quakeclub.com](https://quakeclub.com) to show servers, matches, profiles, demos and news.

## Building from source

The repository uses Git submodules for SDL3, curl, libogg, libvorbis, opus and opusfile. To clone the repository along with its dependencies, run:

```bash
git clone --recursive https://github.com/foressssst/ioquakelive.git
```

If you have already cloned the repository, you can download or update the submodules with:

```bash
git submodule update --init --recursive
```

To run the game once it's built you need the `pak00.pk3` file from your Quake Live installation. If the game doesn't find it in Steam automatically, copy it to the `baseq3` folder next to the executable.

### Windows

You need Visual Studio 2026 or its Build Tools, with the "Desktop development with C++" workload.

Installing [NASM](https://www.nasm.us/) is optional. If it's installed in its default folder, JPEG image loading is optimized.

From the Visual Studio Developer PowerShell, run:

```powershell
msbuild misc\msvc\ioquakelive.sln /p:Configuration=Release /p:Platform=x64 /m
```

The binaries are placed in `build\Release\x64\`.

To build for ARM64 you also need the MSVC ARM64 component, available from the Visual Studio installer. In this case, build SDL3 first and then the game solution:

```powershell
powershell -ExecutionPolicy Bypass -File code\tools\Build-SDL3.ps1
msbuild misc\msvc\ioquakelive.sln /p:Configuration=Release /p:Platform=ARM64 /m
```

The binaries are placed in `build\Release\arm64\`.

### Linux

Install the dependencies we use in our Ubuntu 22.04 build environment:

```bash
sudo apt-get install -y build-essential git zip pkg-config cmake ninja-build nasm \
  libfreetype-dev libogg-dev libvorbis-dev \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev \
  libxss-dev libxtst-dev libxkbcommon-dev libwayland-dev libdecor-0-dev \
  libegl1-mesa-dev libgl1-mesa-dev libgles2-mesa-dev libdrm-dev libgbm-dev \
  libasound2-dev libpulse-dev libpipewire-0.3-dev libdbus-1-dev libudev-dev libibus-1.0-dev
```

Build SDL3 from its submodule first, then the game:

```bash
cmake -S code/SDL3 -B build/SDL3 -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PWD/sdl3-local" -DCMAKE_INSTALL_LIBDIR=lib \
  -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST_LIBRARY=OFF
cmake --build build/SDL3
cmake --install build/SDL3

PKG_CONFIG_PATH="$PWD/sdl3-local/lib/pkgconfig" make release -j"$(nproc)"
cp -L sdl3-local/lib/libSDL3.so.0 build/release-linux-x86_64/
```

The binaries are placed in `build/release-linux-x86_64/`.

On ARM64, the output folder is `build/release-linux-arm64/`. Remember to use that same path in the `cp` command.

### macOS

You need the Xcode command line tools and [Homebrew](https://brew.sh/).

For Apple Silicon computers:

```bash
brew install cmake pkgconf freetype libogg libvorbis

cmake -S code/SDL3 -B build/SDL3 -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DCMAKE_INSTALL_PREFIX="$PWD/sdl3-local" \
  -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST_LIBRARY=OFF
cmake --build build/SDL3 -j"$(sysctl -n hw.logicalcpu)"
cmake --install build/SDL3

PKG_CONFIG_PATH="$PWD/sdl3-local/lib/pkgconfig" make release USE_LOCAL_HEADERS=0 ARCH=arm64 -j"$(sysctl -n hw.logicalcpu)"
```

For Macs with an Intel processor, replace `arm64` with `x86_64` in both commands and add `MACOSX_VERSION_MIN=11.0` to the `make` command.

The binaries are placed in `build/release-darwin-arm64/` or `build/release-darwin-x86_64/`, depending on the architecture.

The `.app` bundle, including MoltenVK for Vulkan, is built following the steps in our build workflow.

### Launcher

To build the launcher you only need the SDL3 submodule and, on Linux, the same X11 and Wayland dependencies listed above.

```bash
cmake -S code/launcher -B build/launcher -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build/launcher --config MinSizeRel --target ioquakelive-launcher
```

The executable is placed in `build/launcher/bin`.

You can find the full build and packaging commands for each platform in [`build.yml`](.github/workflows/build.yml) and [`launcher.yml`](.github/workflows/launcher.yml).

### Repository layout

| Folder | Contents |
| --- | --- |
| `code/client`, `code/server`, `code/qcommon` | Engine code |
| `code/cgame`, `code/game`, `code/ui` | Game modules |
| `code/renderergl1`, `code/renderergl2`, `code/renderervk` | OpenGL, OpenGL 2 and Vulkan renderers |
| `code/launcher` | Launcher source code |
| `content/pak01` | ioquakelive's own files, packed into `pak01.pk3` |
| `misc/msvc` | Visual Studio solution |
| `.github/workflows` | Build and release workflows |

## Reporting bugs

If you find a bug, you can report it in the [Issues](https://github.com/foressssst/ioquakelive/issues) section of this repository or in our support channel on the [Quake Club Discord](https://discord.gg/JKDWykm2Jy).

If possible, include the ioquakelive version you're using, your operating system, the renderer and the steps to reproduce the bug.

## Acknowledgements

- [scryd1ke](https://github.com/scryd1ke), for their code contributions.
- [mmranni](https://github.com/mmranni), for testing and bug reports.

## Credits and license

This project also builds on the work of other developers and open-source projects:

- [ioquakelive by Thomas Jones](https://github.com/tjone270/ioquakelive)
- [ioquake3](https://github.com/ioquake/ioq3)
- [Quake3e](https://github.com/ec-/Quake3e)
- [CPMA](https://playmorepromode.com/)

Quake Live and its assets belong to id Software. The ioquakelive source code is distributed under the GPL v2 license.
