"""
Assemble per-platform release zips for ioquakelive.

Two downloads per platform, because they are two programs to install even though
they come out of one build: the client (ioquakelive) and the dedicated server
(ioqlds). An engine file belongs to the server zip when its name starts with
"ioqlds"; everything else is the client's.

Inputs:
  --shared <dir>    Directory to drop into Linux/Windows release zips as-is.
                    Typically contains baseq3/iobin.pk3 and baseq3/pak01.pk3
                    (the deterministic universal paks built once upstream).
                    pak00.pk3 is NOT included - end users supply it themselves
                    from their existing Quake Live install. The macOS zip
                    does NOT receive the shared layer (see LAYER_SHARED).
  --server <dir>    Files to drop into baseq3/ of the dedicated server zip only:
                    the sample server.cfg and access.txt. Optional.
  --engines <dir>   Parent directory containing one subdirectory per platform
                    artifact, e.g. engines/engine-linux-x86_64/ioquakelive.x86_64
                    (matches actions/download-artifact layout for the
                    "engine-*" artifacts).
  --output <dir>    Where to write ioquakelive-<platform>.zip and
                    ioqlds-<platform>.zip files.

Linux/Windows zips end up with engine binaries flattened at the root plus a
baseq3/ subdirectory sourced from --shared/baseq3. The macOS zip contains
only ioquakelive.app/... - the bundled pk3s live inside Contents/Resources/
and no sibling baseq3/ is added, and it has no separate server zip because
make-macosx-app.sh puts ioqlds inside the bundle.

Byte-determinism across platforms: the iobin.pk3 and pak01.pk3 at each
platform's canonical load path (baseq3/ for linux/windows,
ioquakelive.app/Contents/Resources/baseq3/ for macOS) are byte-identical,
because they all trace back to a single iobin-pk3 / pak01-pk3 artifact
built once upstream.
"""
import argparse
import os
import sys
import zipfile


# An engine file whose name starts with this belongs to the dedicated server.
SERVER_PREFIX = 'ioqlds'

# Maps engine artifact directory name -> (client zip, server zip or None).
PLATFORM_OUT = {
    'engine-linux-x86_64': ('ioquakelive-linux-x86_64.zip', 'ioqlds-linux-x86_64.zip'),
    'engine-windows-x64':  ('ioquakelive-windows-x64.zip',  'ioqlds-windows-x64.zip'),
    'engine-macos-arm64':  ('ioquakelive-macos-arm64.zip',  None),
}

# Whether to layer shared/baseq3/ on top of the engine artifact for each
# platform. macOS is excluded: the signed + notarised ioquakelive.app already
# bundles the pk3s inside Contents/Resources/baseq3/, and shipping a sibling
# baseq3/ next to the .app would be dead weight (the engine reads from
# fs_apppath = Contents/Resources via Sys_DefaultAppPath). The bundle copy
# is byte-identical to Linux/Windows' top-level baseq3/iobin.pk3 because
# they all come from the same iobin-pk3 artifact, so determinism is
# preserved at a different path rather than lost.
LAYER_SHARED = {
    'engine-linux-x86_64': True,
    'engine-windows-x64':  True,
    'engine-macos-arm64':  False,
}


def add_tree(zf, root, dest_prefix, keep=None):
    """Add every file under root into zf, with paths prefixed by dest_prefix.

    keep, when given, decides by relative path which files go in.

    File mode bits are copied from the source file into the zip entry's
    external_attr so that +x survives extraction on Linux/macOS. This matters
    for engine binaries (ioquakelive.x86_64, ioqlds.x86_64) and
    for every Mach-O inside ioquakelive.app/Contents/MacOS/.
    """
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames.sort()
        for name in sorted(filenames):
            abs_path = os.path.join(dirpath, name)
            rel = os.path.relpath(abs_path, root).replace(os.sep, '/')
            if keep and not keep(rel):
                continue
            arc = f'{dest_prefix}/{rel}' if dest_prefix else rel
            st = os.stat(abs_path)
            with open(abs_path, 'rb') as f:
                data = f.read()
            zi = zipfile.ZipInfo(arc)
            zi.compress_type = zipfile.ZIP_DEFLATED
            # Preserve unix mode bits (low 16 of external_attr is MS-DOS,
            # high 16 is unix mode). Advertise as Unix so extractors honour it.
            zi.external_attr = (st.st_mode & 0xFFFF) << 16
            zi.create_system = 3  # Unix
            zf.writestr(zi, data)


def is_server_file(rel):
    """Whether this engine file belongs to the dedicated server."""
    return os.path.basename(rel).startswith(SERVER_PREFIX)


def build_one(engine_dir, shared_dir, server_dir, out_path, layer_shared, want_server):
    print(f'building {out_path}')
    keep = is_server_file if want_server else (lambda rel: not is_server_file(rel))
    with zipfile.ZipFile(out_path, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        # Engine binaries flatten to the root of the zip
        add_tree(zf, engine_dir, '', keep)
        if layer_shared:
            # Shared content (baseq3/...) layered on top. The dedicated server
            # needs it too: qagame lives in iobin.pk3.
            add_tree(zf, shared_dir, '')
        else:
            print(f'  skipping shared/ layering for {os.path.basename(engine_dir)} '
                  '(pk3s already inside bundle)')
        if want_server and server_dir:
            # The samples a server operator edits, where the server looks for them.
            add_tree(zf, server_dir, 'baseq3')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--shared', required=True, help='dir to inline into every zip (contains baseq3/)')
    ap.add_argument('--server', help='dir to inline into baseq3/ of the dedicated server zip')
    ap.add_argument('--engines', required=True, help='parent dir containing engine-<platform>/ subdirs')
    ap.add_argument('--output', required=True, help='dir to write release zips into')
    args = ap.parse_args()

    if not os.path.isdir(args.shared):
        sys.exit(f'shared dir not found: {args.shared}')
    if not os.path.isdir(args.engines):
        sys.exit(f'engines dir not found: {args.engines}')
    if args.server and not os.path.isdir(args.server):
        sys.exit(f'server dir not found: {args.server}')
    os.makedirs(args.output, exist_ok=True)

    found_any = False
    for entry in sorted(os.listdir(args.engines)):
        engine_dir = os.path.join(args.engines, entry)
        if not os.path.isdir(engine_dir):
            continue
        names = PLATFORM_OUT.get(entry)
        if not names:
            print(f'  skipping unknown engine artifact dir: {entry}')
            continue
        client_name, server_name = names
        for out_basename, want_server in ((client_name, False), (server_name, True)):
            if not out_basename:
                continue
            out_path = os.path.join(args.output, out_basename)
            build_one(engine_dir, args.shared, args.server, out_path,
                      LAYER_SHARED[entry], want_server)
            size = os.path.getsize(out_path)
            print(f'  wrote {out_path} ({size} bytes)')
        found_any = True

    if not found_any:
        sys.exit('no recognised engine-<platform> artifact dirs were found')


if __name__ == '__main__':
    main()
