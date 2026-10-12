import argparse
import os
import sys
import zipfile

SERVER_PREFIX = 'ioqlds'

PLATFORM_OUT = {
    'engine-linux-x86_64':  ('ioquakelive-linux-x86_64.zip',  'ioqlds-linux-x86_64.zip'),
    'engine-linux-arm64':   ('ioquakelive-linux-arm64.zip',   'ioqlds-linux-arm64.zip'),
    'engine-windows-x86':   ('ioquakelive-windows-x86.zip',   'ioqlds-windows-x86.zip'),
    'engine-windows-x64':   ('ioquakelive-windows-x64.zip',   'ioqlds-windows-x64.zip'),
    'engine-windows-arm64': ('ioquakelive-windows-arm64.zip', 'ioqlds-windows-arm64.zip'),
    'engine-macos-arm64':   ('ioquakelive-macos-arm64.zip',   None),
}

LAYER_SHARED = {
    'engine-linux-x86_64':  True,
    'engine-linux-arm64':   True,
    'engine-windows-x86':   True,
    'engine-windows-x64':   True,
    'engine-windows-arm64': True,
    'engine-macos-arm64':   False,
}

def add_tree(zf, root, dest_prefix, keep=None):

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
            zi.external_attr = (st.st_mode & 0xFFFF) << 16
            zi.create_system = 3
            zf.writestr(zi, data)

def is_server_file(rel):
    return os.path.basename(rel).startswith(SERVER_PREFIX)

def build_one(engine_dir, shared_dir, server_dir, out_path, layer_shared, want_server):
    print(f'building {out_path}')
    keep = is_server_file if want_server else (lambda rel: not is_server_file(rel))
    with zipfile.ZipFile(out_path, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        add_tree(zf, engine_dir, '', keep)
        if layer_shared:
            add_tree(zf, shared_dir, '')
        else:
            print(f'  skipping shared/ layering for {os.path.basename(engine_dir)} '
                  '(pk3s already inside bundle)')
        if want_server and server_dir:
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
