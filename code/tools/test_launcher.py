import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time


def run(binary, shot, extra, timeout):
    env = dict(os.environ)
    env.update(extra)
    started = time.time()
    r = subprocess.run([binary, shot], env=env, capture_output=True, text=True, errors='replace', timeout=timeout)
    print(f'  {os.path.basename(binary)} exited {r.returncode} after {time.time() - started:.0f}s')
    for line in (r.stdout + r.stderr).splitlines():
        print('   ', line)
    return r


def check(ok, what, failures):
    print(('  ok    ' if ok else '  FAIL  ') + what)
    if not ok:
        failures.append(what)


def placed(root, name):
    inside = os.path.join(root, 'bin', name)
    return inside if os.path.exists(inside) else os.path.join(root, name)


def runnable(path):
    return os.path.isfile(path) and (os.name == 'nt' or os.access(path, os.X_OK))


def config_version(root):
    path = os.path.join(root, 'baseq3', 'launcher.cfg')
    if not os.path.isfile(path):
        return None
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f.read().splitlines():
            if line.startswith('version '):
                return line.split(' ', 1)[1].strip()
    return None


def bundle(path):
    head, sep, _ = path.replace(os.sep, '/').partition('.app/')
    return head + '.app' if sep else path


def replace_with(src, dst):
    src, dst = bundle(src), bundle(dst)
    if os.path.isdir(src):
        shutil.rmtree(dst)
        shutil.copytree(src, dst, symlinks=True)
    else:
        shutil.copy2(src, dst)


def make_old(path):
    with open(path, 'rb') as f:
        data = bytearray(f.read())
    m = re.search(rb'ioquakelive ([0-9][0-9A-Za-z.-]*)', data)
    if not m:
        return None
    old = re.sub(rb'[0-9]', b'0', m.group(1))
    data[m.start(1):m.end(1)] = old
    with open(path, 'wb') as f:
        f.write(data)
    return old.decode()


def signature(app):
    r = subprocess.run(['codesign', '--verify', '--strict', app], capture_output=True, text=True)
    return r.returncode == 0, (r.stdout + r.stderr).strip() or 'valid'


def leftovers(root, allowed=()):
    allowed = {os.path.normcase(os.path.abspath(p)) for p in allowed}
    found = []
    for dirpath, _, filenames in os.walk(root):
        for name in filenames:
            path = os.path.join(dirpath, name)
            if name.endswith(('.new', '.part', '.old')) and os.path.normcase(os.path.abspath(path)) not in allowed:
                found.append(path)
    return found


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--launcher', required=True)
    ap.add_argument('--json', required=True)
    ap.add_argument('--work', required=True)
    ap.add_argument('--game', required=True)
    ap.add_argument('--self', required=True, dest='self_name')
    ap.add_argument('--link', required=True)
    ap.add_argument('--version', required=True)
    ap.add_argument('--timeout', type=int, default=900)
    args = ap.parse_args()

    with open(args.json, encoding='utf-8') as f:
        latest = json.load(f)['tag_name'].lstrip('v')
    work = os.path.abspath(args.work)
    if os.path.isdir(work):
        shutil.rmtree(work)
    root = os.path.join(work, 'install')
    desktop = os.path.join(work, 'desktop')
    os.makedirs(desktop)
    failures = []

    print(f'fresh install of {latest} into {root}')
    run(os.path.abspath(args.launcher), os.path.join(work, 'install.png'), {
        'LAUNCHER_JSON': os.path.abspath(args.json),
        'LAUNCHER_STEPS': '1',
        'LAUNCHER_DIR': root,
        'LAUNCHER_DESKTOP': desktop + os.sep,
        'LAUNCHER_NOSTEAM': '1',
    }, args.timeout)
    game = placed(root, args.game)
    launcher = os.path.join(root, args.self_name)
    check(runnable(game), f'game is in place and runnable: {args.game}', failures)
    check(runnable(launcher), f'launcher copied itself and is runnable: {args.self_name}', failures)
    check(config_version(root) == latest, f'launcher.cfg records {latest} (found {config_version(root)})', failures)
    check(not os.path.lexists(os.path.join(desktop, args.link)), f'no desktop shortcut {args.link} before the last step', failures)
    check(not leftovers(root), 'no .new, .part or .old files left', failures)
    check(os.path.isfile(os.path.join(work, 'install.png')), 'install screenshot saved', failures)
    if sys.platform == 'darwin':
        app = game.split('/Contents/')[0]
        signed, why = signature(app)
        print(f'  info  game bundle signature after install: {why}')

    if runnable(launcher):
        stale = make_old(game)
        check(stale is not None, f'game marked as an old version ({stale})', failures)
        print(f'update from {stale} to {latest}')
        with open(os.path.join(root, 'baseq3', 'launcher.cfg'), 'w', encoding='ascii', newline='\n') as f:
            f.write(f'version {stale}\nby {args.version}')
        old = time.time() - 86400 * 365
        os.utime(game, (old, old))
        replace_with(os.path.abspath(args.launcher), launcher)
        run(launcher, os.path.join(work, 'update.png'), {
            'LAUNCHER_JSON': os.path.abspath(args.json),
            'LAUNCHER_UPDATE': '1',
            'LAUNCHER_NOSTEAM': '1',
        }, args.timeout)
        check(config_version(root) == latest, f'launcher.cfg records {latest} after the update (found {config_version(root)})', failures)
        check(os.path.getmtime(game) > old + 3600, 'game files were replaced by the update', failures)
        check(runnable(game), 'game is still runnable after the update', failures)
        check(runnable(launcher), 'launcher is still runnable after the update', failures)
        check(not leftovers(root, [launcher + '.old']), 'no .new, .part or .old files left after the update', failures)
        check(os.path.isfile(os.path.join(work, 'update.png')), 'update screenshot saved', failures)

    if sys.platform == 'darwin' and signed:
        ok, why = signature(app)
        check(ok, f'game bundle signature still valid ({why})', failures)

    print('result:', 'FAILED ' + str(len(failures)) if failures else 'passed')
    sys.exit(1 if failures else 0)


if __name__ == '__main__':
    main()
