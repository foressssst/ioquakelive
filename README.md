<p align="center">
  <img src="misc/ioquakelive-banner.gif" width="100%" alt="ioquakelive">
</p>

<p align="center">
  <a href="https://github.com/foressssst/ioquakelive/releases/latest"><img src="https://img.shields.io/github/v/release/foressssst/ioquakelive?label=versi%C3%B3n&color=4073FF" alt="Última versión"></a>
  <a href="https://github.com/foressssst/ioquakelive/releases"><img src="https://img.shields.io/github/downloads/foressssst/ioquakelive/total?label=descargas&color=4073FF" alt="Descargas"></a>
  <a href="https://github.com/foressssst/ioquakelive/actions/workflows/build.yml"><img src="https://img.shields.io/github/actions/workflow/status/foressssst/ioquakelive/build.yml?branch=main&label=build" alt="Build"></a>
  <a href="COPYING.txt"><img src="https://img.shields.io/badge/licencia-GPL--2.0-4073FF" alt="Licencia GPL-2.0"></a>
</p>

<p align="center">
  <a href="https://github.com/foressssst/ioquakelive/releases/latest"><b>Descargar</b></a>
  &nbsp;·&nbsp;
  <a href="https://discord.gg/JKDWykm2Jy"><b>Discord</b></a>
  &nbsp;·&nbsp;
  <a href="https://quakeclub.com"><b>Quake Club</b></a>
</p>

Una implementación de código abierto de Quake Live, desarrollada por el equipo de [Quake Club](https://quakeclub.com). Reproduce la última versión original del juego (1069) sobre el motor [ioquake3](https://ioquake3.org/), incorporando mejoras propias y manteniendo la compatibilidad con el juego original.

El proyecto toma como punto de partida el trabajo de [Thomas Jones](https://github.com/tjone270/ioquakelive), cuya implementación sirvió de base e inspiración para continuar su desarrollo.

## Cómo jugar

Necesitarás una copia legítima de [Quake Live](https://store.steampowered.com/app/282440/Quake_Live/), ya que ioquakelive no incluye los archivos originales del juego, por lo que utiliza los de tu propia instalación.

1. Descarga el instalador correspondiente a tu sistema. Cada enlace baja directamente la última versión:

   | Sistema | Instalador |
   | --- | --- |
   | Windows | [`ioquakelive-setup-windows-x64.exe`](https://github.com/foressssst/ioquakelive/releases/latest/download/ioquakelive-setup-windows-x64.exe) |
   | Windows (ARM) | [`ioquakelive-setup-windows-arm64.exe`](https://github.com/foressssst/ioquakelive/releases/latest/download/ioquakelive-setup-windows-arm64.exe) |
   | Linux | [`ioquakelive-setup-linux-x86_64.tar.gz`](https://github.com/foressssst/ioquakelive/releases/latest/download/ioquakelive-setup-linux-x86_64.tar.gz) |
   | Linux (ARM) | [`ioquakelive-setup-linux-arm64.tar.gz`](https://github.com/foressssst/ioquakelive/releases/latest/download/ioquakelive-setup-linux-arm64.tar.gz) |
   | macOS (Apple Silicon) | [`ioquakelive-setup-macos.zip`](https://github.com/foressssst/ioquakelive/releases/latest/download/ioquakelive-setup-macos.zip) |
   | macOS (Intel) | [`ioquakelive-setup-macos-x86_64.zip`](https://github.com/foressssst/ioquakelive/releases/latest/download/ioquakelive-setup-macos-x86_64.zip) |

2. Abre el instalador, elige otra carpeta si lo deseas y pulsa Instalar.
3. Sigue los pasos. El launcher buscará tu instalación de Quake Live de Steam y te permitirá importar tu configuración, HUD y demos. Al finalizar, también podrás crear un acceso directo en el escritorio.

Después, abre siempre el juego desde el launcher. Este se encarga de mantenerlo actualizado. En Windows y Linux lo abre con Vulkan si tu equipo lo admite, o con OpenGL si no; en macOS, con OpenGL.

Si no encuentra tu instalación de Quake Live, puedes seleccionar manualmente el archivo `pak00.pk3`.

Si tienes dudas o algún problema, escríbenos en el [Discord de Quake Club](https://discord.gg/JKDWykm2Jy), donde también está la guía de instalación paso a paso.

## Si el antivirus lo bloquea

ioquakelive es un proyecto nuevo y todavía no tiene firma digital, por lo que su reputación en Windows es baja y el navegador o Microsoft Defender pueden marcarlo como sospechoso. Suele pasar con cada versión nueva, hasta que más personas la descargan. No es un virus, el juego es de código abierto y cada versión se compila en GitHub desde este repositorio, así que descárgalo siempre desde [Releases](https://github.com/foressssst/ioquakelive/releases).

1. Si el navegador bloquea la descarga, elige "Conservar".
2. Si al abrirlo aparece "Windows protegió su PC", pulsa "Más información" y luego "Ejecutar de todas formas".
3. Si Microsoft Defender lo elimina, abre Seguridad de Windows, entra en "Historial de protección", selecciona el aviso y elige "Restaurar". Para que no se repita, agrega la carpeta del juego como exclusión en "Protección antivirus y contra amenazas".

## Características

- Compatibilidad con OpenGL, OpenGL 2 y Vulkan, con soporte para NVIDIA Reflex y AMD Anti-Lag 2 en Vulkan.
- Carga de recursos en varios hilos y FPS desbloqueados.
- Reproductor de demos mejorado.
- Launcher propio que instala el juego y lo mantiene actualizado en Windows, Linux y macOS.
- Integración con Quake Club.

## Cómo se publica cada versión

- Cada versión se compila en [GitHub Actions](https://github.com/foressssst/ioquakelive/actions) desde el código de este repositorio, en máquinas limpias, y el registro de cada compilación es público.
- Los instaladores llevan una certificación de GitHub que prueba de qué compilación salieron. Puedes comprobarlo con [GitHub CLI](https://cli.github.com/):

  ```bash
  gh attestation verify ioquakelive-setup-windows-x64.exe -R foressssst/ioquakelive
  ```

- El launcher comprueba el SHA-256 de cada archivo del juego que descarga antes de instalarlo.
- El juego no viene incluido: ioquakelive usa el `pak00.pk3` de tu instalación de Steam, sin modificarla.
- Para sus funciones propias, el juego y el launcher se conectan a GitHub y a Cloudflare, para buscar y bajar las versiones nuevas, y a [quakeclub.com](https://quakeclub.com), para mostrar servidores, partidas, perfiles, demos y noticias.

## Compilar desde el código

El repositorio usa submódulos de Git (SDL3, curl, libogg, libvorbis, opus y opusfile), así que clónalo con ellos:

```bash
git clone --recursive https://github.com/foressssst/ioquakelive.git
```

Si ya lo tenías clonado, descárgalos con `git submodule update --init --recursive`.

Para probar lo que compilaste, el juego necesita el `pak00.pk3` de tu Quake Live. Si no lo encuentra solo en tu instalación de Steam, cópialo a la carpeta `baseq3` que queda junto al ejecutable.

### Windows

Necesitas Visual Studio 2026 o sus Build Tools, con la carga de trabajo "Desarrollo para el escritorio con C++". [NASM](https://www.nasm.us/) es opcional: si está instalado en su carpeta por defecto, la carga de imágenes JPEG queda optimizada. Desde la Developer PowerShell de Visual Studio:

```powershell
msbuild misc\msvc\ioquakelive.sln /p:Configuration=Release /p:Platform=x64 /m
```

Los binarios quedan en `build\Release\x64\`.

Para ARM64 hace falta además el componente de MSVC para ARM64, que se agrega desde el instalador de Visual Studio. Primero se compila SDL3 y después la solución:

```powershell
powershell -ExecutionPolicy Bypass -File code\tools\Build-SDL3.ps1
msbuild misc\msvc\ioquakelive.sln /p:Configuration=Release /p:Platform=ARM64 /m
```

Los binarios quedan en `build\Release\arm64\`.

### Linux

En Ubuntu, con los paquetes que usa la CI (Ubuntu 22.04):

```bash
sudo apt-get install -y build-essential git zip pkg-config cmake ninja-build nasm \
  libfreetype-dev libogg-dev libvorbis-dev \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev \
  libxss-dev libxtst-dev libxkbcommon-dev libwayland-dev libdecor-0-dev \
  libegl1-mesa-dev libgl1-mesa-dev libgles2-mesa-dev libdrm-dev libgbm-dev \
  libasound2-dev libpulse-dev libpipewire-0.3-dev libdbus-1-dev libudev-dev libibus-1.0-dev
```

Primero SDL3, desde su submódulo, y después el juego:

```bash
cmake -S code/SDL3 -B build/SDL3 -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PWD/sdl3-local" -DCMAKE_INSTALL_LIBDIR=lib \
  -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST_LIBRARY=OFF
cmake --build build/SDL3
cmake --install build/SDL3

PKG_CONFIG_PATH="$PWD/sdl3-local/lib/pkgconfig" make release -j"$(nproc)"
cp -L sdl3-local/lib/libSDL3.so.0 build/release-linux-x86_64/
```

Los binarios quedan en `build/release-linux-x86_64/`. En ARM la carpeta es `build/release-linux-arm64/`, también en el `cp`.

### macOS

Con las herramientas de línea de comandos de Xcode y [Homebrew](https://brew.sh/), en Apple Silicon:

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

En un Mac Intel, cambia `arm64` por `x86_64` en los dos comandos y agrega `MACOSX_VERSION_MIN=11.0` al `make`.

Los binarios quedan en `build/release-darwin-arm64/` o en `build/release-darwin-x86_64/`. El `.app`, con MoltenVK para Vulkan, se arma como en la CI.

### Launcher

Solo necesita el submódulo de SDL3 y, en Linux, las mismas bibliotecas de X11 y Wayland:

```bash
cmake -S code/launcher -B build/launcher -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build/launcher --config MinSizeRel --target ioquakelive-launcher
```

Queda en `build/launcher/bin`.

Los comandos exactos de cada plataforma, con el empaquetado, están en [`build.yml`](.github/workflows/build.yml) y [`launcher.yml`](.github/workflows/launcher.yml).

### Dónde está cada cosa

| Carpeta | Contenido |
| --- | --- |
| `code/client`, `code/server`, `code/qcommon` | El motor |
| `code/cgame`, `code/game`, `code/ui` | Los módulos del juego |
| `code/renderergl1`, `code/renderergl2`, `code/renderervk` | Los renderers OpenGL, OpenGL 2 y Vulkan |
| `code/launcher` | El launcher |
| `content/pak01` | Lo que va dentro de `pak01.pk3`, los archivos que agrega ioquakelive |
| `misc/msvc` | La solución de Visual Studio |
| `.github/workflows` | La compilación y la publicación de cada versión |

## Reportar errores

Abre un [issue](https://github.com/foressssst/ioquakelive/issues) o escríbenos en el [Discord de Quake Club](https://discord.gg/JKDWykm2Jy). Si puedes, cuenta qué sistema usas, qué renderer y cómo se reproduce.

## Agradecimientos

- [scryd1ke](https://github.com/scryd1ke), por sus aportes de código.
- [mmranni](https://github.com/mmranni), por sus pruebas y reportes.

## Créditos y licencia

Este proyecto también se basa en el trabajo de otros desarrolladores y proyectos de código abierto:

- [ioquakelive de Thomas Jones](https://github.com/tjone270/ioquakelive)
- [ioquake3](https://github.com/ioquake/ioq3)
- [Quake3e](https://github.com/ec-/Quake3e)
- [CPMA](https://playmorepromode.com/)

Quake Live y sus recursos pertenecen a id Software. El código fuente de este proyecto se distribuye bajo la licencia GPL v2.
