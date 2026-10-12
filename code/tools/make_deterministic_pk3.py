import argparse
import os
import sys
import zipfile

EPOCH = (1980, 1, 1, 0, 0, 0)

def collect(root):

    out = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = sorted(d for d in dirnames if not d.startswith('.'))
        for name in filenames:
            if name.startswith('.'):
                continue
            abs_path = os.path.join(dirpath, name)
            rel = os.path.relpath(abs_path, root)
            arcname = rel.replace(os.sep, '/')
            out.append((arcname, abs_path))
    out.sort(key=lambda p: p[0])
    return out

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--input', required=True, action='append',
                    help='source directory to pack; repeat it to merge several trees')
    ap.add_argument('--output', required=True, help='output .pk3 path')
    args = ap.parse_args()

    entries = []
    seen = {}
    for root in args.input:
        if not os.path.isdir(root):
            sys.exit(f'input directory not found: {root}')
        found = collect(root)
        for arcname, _ in found:
            key = arcname.lower()
            if key in seen:
                sys.exit(f'{arcname} is both in {seen[key]} and in {root}')
            seen[key] = root
        print(f'{len(found)} files from {root}')
        entries.extend(found)
    entries.sort(key=lambda p: p[0])
    if not entries:
        sys.exit('no files found under ' + ', '.join(args.input))

    out_dir = os.path.dirname(os.path.abspath(args.output))
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)

    with zipfile.ZipFile(args.output, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        for arcname, abs_path in entries:
            with open(abs_path, 'rb') as f:
                data = f.read()
            zi = zipfile.ZipInfo(arcname, EPOCH)
            zi.compress_type = zipfile.ZIP_DEFLATED
            zi.external_attr = 0o644 << 16
            zi.create_system = 3
            zf.writestr(zi, data)

    size = os.path.getsize(args.output)
    print(f'wrote {args.output} ({size} bytes, {len(entries)} entries)')
    for arcname, _ in entries:
        print(f'  {arcname}')

if __name__ == '__main__':
    main()
