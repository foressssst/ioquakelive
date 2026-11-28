<p align="center">
  <img src="misc/ioquakelive-banner.gif" width="100%" alt="ioquakelive">
</p>

<p align="center">
  <b>Español</b> · <a href="README.en.md">English</a>
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

ioquakelive es una implementación de código abierto de Quake Live, desarrollada por el equipo de [Quake Club](https://quakeclub.com). Reproduce la última versión original del juego (build 1069) sobre el motor [ioquake3](https://ioquake3.org/), incorporando mejoras propias sin perder compatibilidad con el juego original.

El proyecto toma como punto de partida el trabajo de [Thomas Jones](https://github.com/tjone270/ioquakelive), cuya implementación sirvió de base e inspiración para continuar su desarrollo.

## Cómo jugar

Necesitas tener una copia legítima de [Quake Live](https://store.steampowered.com/app/282440/Quake_Live/) en Steam, ya que ioquakelive no incluye los archivos originales del juego, sino que utiliza los de tu propia instalación.

1. Descarga el instalador correspondiente a tu sistema operativo. Todos los enlaces apuntan directamente a la última versión estable.

   | Sistema | Instalador |
   | --- | --- |
   | Windows (x64) | [`ioquakelive-setup-windows-x64.exe`](https://github.com/foressssst/ioquakelive/releases/latest/download/ioquakelive-setup-windows-x64.exe) |
   | Windows (ARM64) | [`ioquakelive-setup-windows-arm64.exe`](https://github.com/foressssst/ioquakelive/releases/latest/download/ioquakelive-setup-windows-arm64.exe) |
   | Linux (x86_64) | [`ioquakelive-setup-linux-x86_64.tar.gz`](https://github.com/foressssst/ioquakelive/releases/latest/download/ioquakelive-setup-linux-x86_64.tar.gz) |
   | Linux (ARM64) | [`ioquakelive-setup-linux-arm64.tar.gz`](https://github.com/foressssst/ioquakelive/releases/latest/download/ioquakelive-setup-linux-arm64.tar.gz) |
   | macOS (Apple Silicon) | [`ioquakelive-setup-macos.zip`](https://github.com/foressssst/ioquakelive/releases/latest/download/ioquakelive-setup-macos.zip) |
   | macOS (Intel) | [`ioquakelive-setup-macos-x86_64.zip`](https://github.com/foressssst/ioquakelive/releases/latest/download/ioquakelive-setup-macos-x86_64.zip) |

2. Abre el instalador, selecciona la carpeta donde quieres instalar el juego y pulsa Instalar.
3. Sigue los pasos de instalación. El launcher buscará automáticamente tu instalación de Quake Live y te dará la opción de importar tu configuración, HUD y demos. También podrás crear un acceso directo en el escritorio.

Una vez instalado, abre siempre el juego desde el launcher, ya que este se encarga de mantenerlo actualizado.

En Windows y Linux, el launcher iniciará el juego con Vulkan si tu equipo es compatible. De lo contrario, utilizará OpenGL. En macOS se utilizará OpenGL.

Si el launcher no encuentra tu instalación de Quake Live, puedes seleccionar manualmente el archivo `pak00.pk3`.

Si tienes dudas o algún problema, en el [Discord de Quake Club](https://discord.gg/JKDWykm2Jy) encontrarás una guía de instalación paso a paso y un canal dedicado al soporte de ioquakelive.

## Si el antivirus lo bloquea

ioquakelive es un proyecto relativamente nuevo y sus ejecutables aún no cuentan con una firma digital, por lo que el navegador o Microsoft Defender pueden marcar algunas versiones como sospechosas.

El código fuente es público y cada versión se compila directamente en GitHub desde este repositorio. Por ello, recomendamos descargar ioquakelive únicamente desde nuestra sección de [Releases](https://github.com/foressssst/ioquakelive/releases).

Antes de continuar ante una advertencia de seguridad, comprueba la procedencia del instalador siguiendo las instrucciones de la sección «Cómo se publica cada versión».

Si Microsoft Defender detecta algún archivo como una amenaza, revisa el detalle de la detección en el historial de protección de Windows. Ante cualquier duda, puedes consultarnos en nuestro canal de soporte de Discord.

## Características

- Compatibilidad con OpenGL, OpenGL 2 y Vulkan.
- Soporte para NVIDIA Reflex y AMD Anti-Lag 2 en Vulkan.
- Carga de recursos en varios hilos y FPS desbloqueados.
- Mejoras en el reproductor de demos.
- Launcher propio que facilita la instalación y mantiene el juego actualizado en Windows, Linux y macOS.
- Integración con Quake Club.

## Cómo se publica cada versión

- Todas las versiones de ioquakelive se compilan mediante [GitHub Actions](https://github.com/foressssst/ioquakelive/actions), directamente desde el código fuente de este repositorio y en entornos de compilación limpios.
- Los instaladores cuentan con una certificación de procedencia de GitHub que permite verificar de qué compilación provienen. Puedes comprobarlo utilizando [GitHub CLI](https://cli.github.com/):

  ```bash
  gh attestation verify ioquakelive-setup-windows-x64.exe -R foressssst/ioquakelive
  ```

- El launcher verifica la integridad de cada archivo del juego que descarga mediante SHA-256 antes de instalarlo.
- ioquakelive no distribuye los archivos originales de Quake Live. Utiliza el archivo `pak00.pk3` de tu instalación de Steam, sin modificarla.
- El juego y el launcher se conectan a GitHub y Cloudflare para consultar y descargar nuevas versiones, y a [quakeclub.com](https://quakeclub.com) para mostrar servidores, partidas, perfiles, demos y noticias.

## Compilar desde el código fuente

El repositorio utiliza submódulos de Git para SDL3, curl, libogg, libvorbis, opus, opusfile y FFmpeg. Para clonar el repositorio junto con sus dependencias, ejecuta:

```bash
git clone --recursive https://github.com/foressssst/ioquakelive.git
```

Si ya tienes el repositorio clonado, puedes descargar o actualizar los submódulos con:

```bash
git submodule update --init --recursive
```

Para ejecutar el juego una vez compilado necesitarás el archivo `pak00.pk3` de tu instalación de Quake Live. Si el juego no lo encuentra automáticamente en Steam, cópialo a la carpeta `baseq3` junto al ejecutable.

### Windows

Necesitas Visual Studio 2026 o sus Build Tools, con la carga de trabajo «Desarrollo para el escritorio con C++».

La instalación de [NASM](https://www.nasm.us/) es opcional. Si se encuentra instalado en su carpeta por defecto, se optimizará la carga de imágenes JPEG.

Los videos del Training Center se reproducen mediante FFmpeg. Para compilarlo necesitas MSYS2 con los paquetes `make` y `diffutils` instalados. Antes de compilar el juego, ejecuta `code\tools\Build-FFmpeg.ps1` (agrega `-Platform ARM64` si estás compilando para ARM64).

Este paso es opcional. Si decides omitirlo, podrás compilar y ejecutar el juego con normalidad, pero los videos del Training Center no estarán disponibles.

Desde la Developer PowerShell de Visual Studio, ejecuta:

```powershell
msbuild misc\msvc\ioquakelive.sln /p:Configuration=Release /p:Platform=x64 /m
```

Los binarios se generan en `build\Release\x64\`.

Para compilar en ARM64 también necesitas el componente de MSVC para ARM64, disponible desde el instalador de Visual Studio. En este caso, primero debes compilar SDL3 y luego la solución del juego:

```powershell
powershell -ExecutionPolicy Bypass -File code\tools\Build-SDL3.ps1
msbuild misc\msvc\ioquakelive.sln /p:Configuration=Release /p:Platform=ARM64 /m
```

Los binarios se generan en `build\Release\arm64\`.

### Linux

Instala las dependencias que utilizamos en nuestro entorno de compilación de Ubuntu 22.04:

```bash
sudo apt-get install -y build-essential git zip pkg-config cmake ninja-build nasm \
  libfreetype-dev libogg-dev libvorbis-dev \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev \
  libxss-dev libxtst-dev libxkbcommon-dev libwayland-dev libdecor-0-dev \
  libegl1-mesa-dev libgl1-mesa-dev libgles2-mesa-dev libdrm-dev libgbm-dev \
  libasound2-dev libpulse-dev libpipewire-0.3-dev libdbus-1-dev libudev-dev libibus-1.0-dev
```

Antes de compilar el juego, compila FFmpeg para habilitar la reproducción de los videos del Training Center:

```bash
sh code/tools/build-ffmpeg.sh code/libs/ffmpeg/linux-x86_64 --enable-pic
```

Si estás compilando para ARM64, reemplaza `linux-x86_64` por `linux-arm64`.

Primero compila SDL3 desde su submódulo y luego el juego:

```bash
cmake -S code/SDL3 -B build/SDL3 -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PWD/sdl3-local" -DCMAKE_INSTALL_LIBDIR=lib \
  -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST_LIBRARY=OFF
cmake --build build/SDL3
cmake --install build/SDL3

PKG_CONFIG_PATH="$PWD/sdl3-local/lib/pkgconfig" make release -j"$(nproc)"
cp -L sdl3-local/lib/libSDL3.so.0 build/release-linux-x86_64/
```

Los binarios se generan en `build/release-linux-x86_64/`.

En ARM64, la carpeta de salida es `build/release-linux-arm64/`. Recuerda utilizar esta misma ruta en el comando `cp`.

### macOS

Necesitas las herramientas de línea de comandos de Xcode y [Homebrew](https://brew.sh/).

Antes de compilar el juego, compila FFmpeg para habilitar la reproducción de los videos del Training Center:

```bash
sh code/tools/build-ffmpeg.sh code/libs/ffmpeg/darwin-arm64 --extra-cflags=-mmacosx-version-min=11.0 --extra-ldflags=-mmacosx-version-min=11.0
```

Para equipos con Apple Silicon:

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

Para equipos Mac con procesador Intel, reemplaza `arm64` por `x86_64` en todos los comandos y agrega `MACOSX_VERSION_MIN=11.0` al comando `make`.

Los binarios se generan en `build/release-darwin-arm64/` o `build/release-darwin-x86_64/`, dependiendo de la arquitectura.

El paquete `.app`, incluyendo MoltenVK para Vulkan, se genera siguiendo los pasos de nuestro workflow de compilación.

### Launcher

Para compilar el launcher solo necesitas el submódulo de SDL3 y, en Linux, las mismas dependencias de X11 y Wayland mencionadas anteriormente.

```bash
cmake -S code/launcher -B build/launcher -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build/launcher --config MinSizeRel --target ioquakelive-launcher
```

El ejecutable se genera en `build/launcher/bin`.

Puedes consultar los comandos completos de compilación y empaquetado de cada plataforma en [`build.yml`](.github/workflows/build.yml) y [`launcher.yml`](.github/workflows/launcher.yml).

### Estructura del repositorio

| Carpeta | Contenido |
| --- | --- |
| `code/client`, `code/server`, `code/qcommon` | Código del motor |
| `code/cgame`, `code/game`, `code/ui` | Módulos del juego |
| `code/renderergl1`, `code/renderergl2`, `code/renderervk` | Renderers OpenGL, OpenGL 2 y Vulkan |
| `code/launcher` | Código fuente del launcher |
| `content/pak01` | Archivos propios de ioquakelive incluidos en `pak01.pk3` |
| `misc/msvc` | Solución de Visual Studio |
| `.github/workflows` | Procesos de compilación y publicación de nuevas versiones |

## Reportar errores

Si encuentras algún error, puedes reportarlo desde la sección de [Issues](https://github.com/foressssst/ioquakelive/issues) de este repositorio o en nuestro canal de soporte en el [Discord de Quake Club](https://discord.gg/JKDWykm2Jy).

En lo posible, indica qué versión de ioquakelive estás utilizando, tu sistema operativo, el renderer y los pasos para reproducir el error.

## Agradecimientos

- [scryd1ke](https://github.com/scryd1ke), por sus aportes al código.
- [mmranni](https://github.com/mmranni), por sus pruebas y reportes de errores.

## Créditos y licencia

Este proyecto también se basa en el trabajo de otros desarrolladores y proyectos de código abierto:

- [ioquakelive de Thomas Jones](https://github.com/tjone270/ioquakelive)
- [ioquake3](https://github.com/ioquake/ioq3)
- [Quake3e](https://github.com/ec-/Quake3e)
- [CPMA](https://playmorepromode.com/)

Quake Live y sus recursos pertenecen a id Software. El código fuente de ioquakelive se distribuye bajo la licencia GPL v2.
