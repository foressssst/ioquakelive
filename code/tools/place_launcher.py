import argparse
import io
import json
import os
import shutil
import subprocess
import sys
import tarfile
import urllib.request
import zipfile

SOURCES = [
    'code/launcher',
    'code/zlib',
    'code/libjpeg-turbo',
    'code/renderercommon/tr_stbtt.c',
    'code/renderercommon/tr_stbtt.h',
    'code/sys/win_resource.rc',
    'code/sys/win_resource.h',
    'misc/quakelive.ico',
    'code/SDL3',
]

ASSETS = {
    'windows-x64':  'ioquakelive-launcher.exe',
    'windows-arm64': 'ioquakelive-launcher-arm64.exe',
    'linux-x86_64': 'ioquakelive-launcher-linux-x86_64.tar.gz',
    'linux-arm64':  'ioquakelive-launcher-linux-arm64.tar.gz',
    'macos-arm64':  'ioquakelive-launcher-macos.zip',
}

LICENSE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'launcher', 'OFL.txt')
LICENSE_NAME = 'ioquakelive-launcher-OFL.txt'
BACKGROUND = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'content', 'launcher', 'launcher.jpg')

ENGINES = {
    'windows-x64':  'engine-windows-x64',
    'windows-arm64': 'engine-windows-arm64',
    'linux-x86_64': 'engine-linux-x86_64',
    'linux-arm64':  'engine-linux-arm64',
    'macos-arm64':  'engine-macos-arm64',
}


def api(repo, path):
    req = urllib.request.Request(f'https://api.github.com/repos/{repo}/{path}', headers={'User-Agent': 'ioquakelive-ci'})
    token = os.environ.get('GH_TOKEN')
    if token:
        req.add_header('Authorization', f'Bearer {token}')
    with urllib.request.urlopen(req, timeout=60) as r:
        return json.load(r)


def fetch(url, path):
    req = urllib.request.Request(url, headers={'User-Agent': 'ioquakelive-ci'})
    with urllib.request.urlopen(req, timeout=300) as r, open(path, 'wb') as f:
        shutil.copyfileobj(r, f)


def unchanged_since(tag):
    r = subprocess.run(['git', 'diff', '--quiet', tag, 'HEAD', '--'] + SOURCES)
    return r.returncode == 0


def previous_assets(repo, stage):
    try:
        latest = api(repo, 'releases/latest')
    except Exception as e:
        print(f'no previous release to compare with: {e}')
        return None
    tag = latest['tag_name']
    urls = {a['name']: a['browser_download_url'] for a in latest.get('assets', [])}
    if not all(name in urls for name in ASSETS.values()):
        print(f'{tag} does not carry all the launcher files, using the new build')
        return None
    if not unchanged_since(tag):
        print(f'launcher sources changed since {tag}, using the new build')
        return None
    print(f'launcher sources unchanged since {tag}, reusing its files')
    os.makedirs(stage, exist_ok=True)
    for name in ASSETS.values():
        fetch(urls[name], os.path.join(stage, name))
    return stage


def fresh_assets(launchers, stage):
    os.makedirs(stage, exist_ok=True)
    for key, name in ASSETS.items():
        src = os.path.join(launchers, f'launcher-{key}')
        out = os.path.join(stage, name)
        if key.startswith('windows'):
            shutil.copyfile(os.path.join(src, 'ioquakelive-launcher.exe'), out)
            continue
        with tarfile.open(os.path.join(src, 'launcher.tar')) as t:
            members = sorted((m for m in t.getmembers() if m.isfile()), key=lambda m: m.name)
            if key.startswith('linux'):
                member = t.getmember('ioquakelive-launcher')
                with tarfile.open(out, 'w:gz') as gz:
                    info = tarfile.TarInfo('ioquakelive-launcher')
                    info.size = member.size
                    info.mode = 0o755
                    info.mtime = member.mtime
                    gz.addfile(info, t.extractfile(member))
                    with open(LICENSE, 'rb') as f:
                        text = f.read()
                    info = tarfile.TarInfo(LICENSE_NAME)
                    info.size = len(text)
                    info.mode = 0o644
                    info.mtime = member.mtime
                    gz.addfile(info, io.BytesIO(text))
                continue
            with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
                for m in members:
                    arc = m.name[2:] if m.name.startswith('./') else m.name
                    leaf = arc.rsplit('/', 1)[-1]
                    if leaf.startswith('._') or leaf == '.DS_Store':
                        continue
                    zi = zipfile.ZipInfo(arc)
                    zi.compress_type = zipfile.ZIP_DEFLATED
                    zi.external_attr = (0o100000 | (m.mode & 0o777)) << 16
                    zi.create_system = 3
                    zf.writestr(zi, t.extractfile(m).read())
    return stage


def place(stage, engines):
    for key, name in ASSETS.items():
        dest = os.path.join(engines, ENGINES[key])
        if not os.path.isdir(dest):
            print(f'  no {ENGINES[key]}, skipping')
            continue
        src = os.path.join(stage, name)
        if key.startswith('windows'):
            shutil.copyfile(src, os.path.join(dest, 'ioquakelive-launcher.exe'))
            shutil.copyfile(LICENSE, os.path.join(dest, LICENSE_NAME))
        elif key.startswith('linux'):
            with tarfile.open(src) as t:
                t.extractall(dest, filter='data')
            os.chmod(os.path.join(dest, 'ioquakelive-launcher'), 0o755)
        else:
            with zipfile.ZipFile(src) as zf:
                for zi in zf.infolist():
                    path = os.path.join(dest, *zi.filename.split('/'))
                    if zi.is_dir():
                        os.makedirs(path, exist_ok=True)
                        continue
                    os.makedirs(os.path.dirname(path), exist_ok=True)
                    with open(path, 'wb') as f:
                        f.write(zf.read(zi))
                    mode = (zi.external_attr >> 16) & 0o777
                    if mode:
                        os.chmod(path, mode)
            shutil.copyfile(LICENSE, os.path.join(dest, LICENSE_NAME))
        os.makedirs(os.path.join(dest, 'baseq3'), exist_ok=True)
        shutil.copyfile(BACKGROUND, os.path.join(dest, 'baseq3', 'launcher.jpg'))
        print(f'  {name} -> {ENGINES[key]}')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--launchers', required=True)
    ap.add_argument('--engines', required=True)
    ap.add_argument('--output', required=True)
    ap.add_argument('--repo', required=True)
    args = ap.parse_args()

    os.makedirs(args.output, exist_ok=True)
    stage = os.path.join(args.output, '.launcher')
    if os.path.isdir(stage):
        shutil.rmtree(stage)
    if not previous_assets(args.repo, stage):
        fresh_assets(args.launchers, stage)
    place(stage, args.engines)
    for name in ASSETS.values():
        shutil.move(os.path.join(stage, name), os.path.join(args.output, name))
        print(f'  {name} ({os.path.getsize(os.path.join(args.output, name))} bytes)')
    shutil.rmtree(stage)


if __name__ == '__main__':
    sys.exit(main())
