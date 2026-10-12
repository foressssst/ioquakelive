import os
import subprocess
import sys

def git(args):
    try:
        out = subprocess.run(
            ['git'] + args,
            check=True,
            capture_output=True,
            text=True,
            cwd=os.path.dirname(os.path.abspath(__file__)),
        )
        return out.stdout.strip()
    except (FileNotFoundError, subprocess.CalledProcessError):
        return ''

def detect_hash():
    short = git(['rev-parse', '--short=10', 'HEAD'])
    if not short:
        return 'unknown'
    status = git(['status', '--porcelain', '--untracked-files=no'])
    if status:
        return short + '-dirty'
    return short

def main():
    if len(sys.argv) != 2:
        sys.exit(f'usage: {sys.argv[0]} <output_path>')
    out_path = sys.argv[1]

    git_hash = detect_hash()
    new_content = (
        '#ifndef GIT_VERSION_H\n'
        '#define GIT_VERSION_H\n'
        f'#define PRODUCT_GIT_HASH "{git_hash}"\n'
        '#endif\n'
    )

    existing = ''
    if os.path.exists(out_path):
        with open(out_path, 'r') as f:
            existing = f.read()

    if existing == new_content:
        return

    out_dir = os.path.dirname(out_path)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    with open(out_path, 'w') as f:
        f.write(new_content)
    print(f'wrote {out_path} (PRODUCT_GIT_HASH = "{git_hash}")')

if __name__ == '__main__':
    main()
