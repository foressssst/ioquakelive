# ioquakelive

Una implementación de código abierto de Quake Live, desarrollada por el equipo de [Quake Club](https://quakeclub.com). Reproduce la última versión original del juego (1069) sobre el motor [ioquake3](https://ioquake3.org/), incorporando mejoras propias y manteniendo la compatibilidad con el juego original.

El proyecto toma como punto de partida el trabajo de [Thomas Jones](https://github.com/tjone270/ioquakelive), cuya implementación sirvió de base e inspiración para continuar su desarrollo.

## Cómo jugar

Necesitarás una copia legítima de [Quake Live en Steam](https://store.steampowered.com/app/282440/Quake_Live/), ya que ioquakelive no incluye los archivos originales del juego, por lo que utiliza los de tu propia instalación.

1. Descarga la última versión desde [Releases](https://github.com/foressssst/ioquakelive/releases).
2. Extrae los archivos en la carpeta que prefieras.
3. Ejecuta `ioquakelive.x86_64.exe` para utilizar la version OpenGL o `ioquakelive-vulkan.x86_64.exe` si prefieres Vulkan.

En Windows, el juego detecta automáticamente tu instalación de Quake Live y copia el archivo `pak00.pk3`. Si la detección automática no funciona, puedes copiarlo manualmente a la carpeta `baseq3`.

## Características

- Compatibilidad con OpenGL, OpenGL 2 y Vulkan, incluyendo soporte para NVIDIA Reflex y AMD Anti-Lag 2 en Vulkan.
- Carga de recursos en varios hilos y FPS desbloqueados.
- Reproductor de demos mejorado, con soporte para `cg_xray` (wallhack) durante la reproducción de demos y al espectear. Esta función no está disponible durante las partidas.
- Integración con Quake Club.
- Nuevos comandos de consola: `help <comando o cvar>` y `searchconsole`.

## Compilación

Antes de compilar, inicializa los submódulos del proyecto:

```bash
git submodule update --init --recursive
```

- **Windows:** requiere Visual Studio 2026 con el toolset v145. Para compilar, ejecuta:

  ```bash
  MSBuild misc\msvc\ioquakelive.sln -p:Configuration=Release -p:Platform=x64
  ```

- **Linux y macOS:** ejecuta `make release`. Se requiere tener SDL3 instalado.

## Firma de código

Los binarios de Windows se firman con la firma de código gratuita para proyectos de código abierto de [SignPath Foundation](https://signpath.org), provista a través de [SignPath.io](https://signpath.io).

## Agradecimientos

- [scryd1ke](https://github.com/scryd1ke) — aportes de código.
- [mmranni](https://github.com/mmranni) — pruebas y reportes.

## Créditos y licencia

Este proyecto también se basa en el trabajo de otros desarrolladores y proyectos de código abierto:

- [Thomas Jones — ioquakelive](https://github.com/tjone270/ioquakelive)
- [ioquake3](https://github.com/ioquake/ioq3)
- [Quake3e](https://github.com/ec-/Quake3e)
- [CPMA](https://playmorepromode.com/)

Quake Live y sus recursos pertenecen a id Software. El código fuente de este proyecto se distribuye bajo la licencia GPL v2.
