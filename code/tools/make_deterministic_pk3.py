"""
Build a deterministic, byte-identical .pk3 from a directory tree.

Used by CI to package iobin.pk3 (flat dir of game DLLs) and pak01.pk3
(recursive content tree of textures/models/sounds/scripts/ui), so each
release ships the same bytes on every platform regardless of the host
OS that the packaging job runs on.

Determinism notes:
  - File order: sorted by posix arcname.
  - Timestamps: forced to 1980-01-01 00:00:00 (earliest legal zip stamp).
  - External attributes: forced to 0o644 << 16 (Unix file mode).
  - create_system: forced to 3 (Unix), so the zip header doesn't include
    OS-specific extra fields like Windows file attributes.
  - Compression: zlib DEFLATE level 9.
"""
import argparse
import os
import sys
import zipfile

EPOCH = (1980, 1, 1, 0, 0, 0)  # earliest legal zip timestamp


def collect(root):
    """Walk root and return [(arcname, abs_path)] sorted by posix arcname.

    Anything whose name starts with a dot is left out: an input can be a git
    working copy, and its .git must not end up in the pk3.
    """
    out = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = sorted(d for d in dirnames if not d.startswith('.'))
        for name in filenames:
            if name.startswith('.'):
                continue
            abs_path = os.path.join(dirpath, name)
            rel = os.path.relpath(abs_path, root)
            arcname = rel.replace(os.sep, '/')  # always posix inside the zip
            out.append((arcname, abs_path))
    out.sort(key=lambda p: p[0])
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--input', required=True, action='append',
                    help='source directory to pack; repeat it to merge several trees')
    ap.add_argument('--output', required=True, help='output .pk3 path')
    args = ap.parse_args()

    # the game looks paths up without regard to case, so two inputs may not
    # both carry a path even when its case differs
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
