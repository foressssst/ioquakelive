# ioquakelive

Una implementación de código abierto de Quake Live, desarrollada por el equipo de [Quake Club](https://quakeclub.com). Reproduce la última versión original del juego (la build 1069) utilizando [ioquake3](https://ioquake3.org/) como motor, incorporando mejoras propias sin perder la compatibilidad con el Juego original.

El proyecto parte del trabajo de [Thomas Jones](https://github.com/tjone270/ioquakelive), cuya implementación fue el punto de partida y la inspiración para continuar su desarrollo.

## Cómo jugar

Necesitaras tener una copia legitima de Quake Live (https://store.steampowered.com/app/282440/Quake_Live/). ioquakelive no incluye los archivos originales del juego, utiliza los de tu propia instalación, luego:

1. Descarga la última versión desde [Releases](https://github.com/foressssst/ioquakelive/releases).
2. Extrae el juego en cualquier carpeta, en donde sea.
3. Ejecuta `ioquakelive.x86_64.exe` o `ioquakelive-vulkan.x86_64.exe` si quieres utilizar Vulkan.

En Sistemas operativos Windows, el juego detecta automáticamente tu instalación de Quake Live y copia el `pak00.pk3`, caso contrario puedes copiarlo manualmente a la carpeta `baseq3`.


## Que es lo novedoso?

- Compatibilidad con OpenGL, OpenGL 2 y Vulkan, soporte para NVIDIA Reflex y AMD Anti-Lag 2 en Vulkan.
- Carga de recursos en varios hilos y desbloqueo de FPS.
- Un demo player renovado que incluye`cg_xray`(wallhack) durante la reproducción de demos y al espectear (no funciona al entrar a jugar).
- Integración con Quake Club.
- Nuevos comandos de consola: `help <comando o cvar>` y `searchconsole`.

## Para compilar

Primero que nada, inicializa los submódulos:

```bash
git submodule update --init --recursive
```

- **Windows:** Visual Studio 2026 (toolset v145), se compila con:
  `MSBuild misc\msvc\ioquakelive.sln -p:Configuration=Release -p:Platform=x64`
- **Linux y macOS:** ejecuta `make release`, este requiere tener SDL3 instalado.

## Créditos y licencia

Este proyecto utiliza y toma como referencia el y los trabajo de:

- [Thomas Jones — ioquakelive](https://github.com/tjone270/ioquakelive)
- [ioquake3](https://github.com/ioquake/ioq3)
- [Quake3e](https://github.com/ec-/Quake3e)
- [CPMA](https://playmorepromode.com/)

Quake Live y sus recursos pertenecen a id Software, el código fuente se distribuye bajo la licencia GPL v2.
