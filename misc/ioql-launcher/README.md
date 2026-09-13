# ioquakelive launcher (privado)

Launcher y autoactualizador para las builds privadas de ioquakelive.

- `install.ps1`: instalación de un solo comando. Deja `launcher.ps1`,
  `ioquakelive.cmd` y `launcher.json` en `%LOCALAPPDATA%\ioquakelive` (o en
  `$IoqlDir`), crea un acceso directo en el escritorio y ejecuta el launcher.
- `launcher.ps1`: al abrirse baja el manifiesto del canal, verifica su firma
  RSA-SHA256 con la llave pública incrustada, baja solo los archivos que
  cambiaron (cada uno comprobado por SHA-256), los instala y abre el juego. Sin
  conexión, o con una firma inválida, abre la versión ya instalada.
- `ioquakelive.cmd`: el acceso directo apunta acá.

Nunca se publica ni se descarga `pak00.pk3` ni otro archivo de id Software: el
juego lo copia de la instalación de Quake Live en Steam de cada jugador
(`FS_CopyFromSteam`, `code/qcommon/files.c`).

Las versiones se publican con `ql\tools\publish_release.ps1`, fuera del repo,
que es donde vive la llave privada de firma.
