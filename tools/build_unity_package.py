#!/usr/bin/env python3
"""Build the host platform's native plugin and assemble a local Unity package under out/."""
import argparse
import hashlib
import json
import platform
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def verify_binary(file, system, arch):
    if system == 'macOS':
        actual = set(subprocess.check_output(['lipo', '-archs', str(file)], text=True).split())
        expected = {'arm64': {'arm64'}, 'x64': {'x86_64'}, 'universal': {'arm64', 'x86_64'}}[arch]
        if actual != expected:
            raise ValueError(f'Plugin architecture {actual} does not match requested {arch}')
        dependencies = subprocess.check_output(['otool', '-L', str(file)], text=True)
        for line in dependencies.splitlines():
            if line.startswith('\t') and not line.strip().startswith(('@rpath/libfoliage_native.dylib ', '/usr/lib/', '/System/Library/')):
                raise ValueError('Plugin has a nonportable dynamic dependency: ' + line.strip())
    else:
        data = file.read_bytes()[:4096]
        if system == 'Windows':
            offset = struct.unpack_from('<I', data, 0x3c)[0]
            valid = data[:2] == b'MZ' and data[offset:offset + 4] == b'PE\0\0' and struct.unpack_from('<H', data, offset + 4)[0] == 0x8664
        else:
            valid = data[:6] == b'\x7fELF\x02\x01' and struct.unpack_from('<H', data, 18)[0] == 62
        if not valid:
            raise ValueError('Native binary does not match desktop x64 platform')


def metadata(path, relative):
    guid = hashlib.sha256(('FoliageUtil/' + relative.as_posix()).encode()).hexdigest()[:32]
    text = f'fileFormatVersion: 2\nguid: {guid}\n'
    if path.is_dir():
        text += 'folderAsset: yes\n'
    Path(str(path) + '.meta').write_text(text)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arch', choices=['arm64', 'x64', 'universal'])
    parser.add_argument('--out', type=Path, default=ROOT / 'out/unity-package')
    parser.add_argument('--build', type=Path)
    parser.add_argument('--skip-build', action='store_true', help='Package an existing build configured for this platform and architecture')
    parser.add_argument('--jobs', type=int, default=8)
    args = parser.parse_args()
    system = {'Darwin': 'macOS', 'Windows': 'Windows', 'Linux': 'Linux'}.get(platform.system())
    if not system:
        parser.error('Only desktop macOS, Windows and Linux hosts are supported')
    arch = args.arch or ('arm64' if platform.machine().lower() in ['arm64', 'aarch64'] else 'x64')
    if system != 'macOS' and arch != 'x64':
        parser.error('This package currently builds Windows/Linux x64; macOS also supports ARM64 and universal')
    if not 1 <= args.jobs <= 64:
        parser.error('--jobs must be within 1..64')
    output = args.out.resolve()
    if ROOT / 'out' not in output.parents:
        parser.error('Keep generated Unity packages under this repository\'s out/ directory')
    build = (args.build or ROOT / f'build-unity-{system.lower()}-{arch}').resolve()
    if not args.skip_build:
        command = ['cmake', '-S', str(ROOT), '-B', str(build), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', '-DFOLIAGE_NATIVE_PLUGIN=ON', '-DFOLIAGE_BUNDLED_PNG=ON', '-DFOLIAGE_ALEMBIC=OFF']
        if system == 'macOS':
            command += ['-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0', '-DCMAKE_OSX_ARCHITECTURES=' + {'arm64': 'arm64', 'x64': 'x86_64', 'universal': 'arm64;x86_64'}[arch]]
        subprocess.run(command, check=True)
        subprocess.run(['cmake', '--build', str(build), '--config', 'Release', '--parallel', str(args.jobs)], check=True)
    manifest = output / '.foliageutil-package.json'
    if output.exists() and any(output.iterdir()):
        if not manifest.is_file():
            parser.error('Output is not an owned generated package; choose another directory')
        previous = json.loads(manifest.read_text())
        actual = sorted(str(p.relative_to(output)) for p in output.rglob('*') if p.is_file() and p.name != '.DS_Store')
        if sorted(previous['files'] + ['.foliageutil-package.json']) != actual:
            parser.error('Output has added or missing files; choose another directory to preserve them')
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.foliage-unity-', dir=output.parent) as temp:
        work = Path(temp)
        stage, installed = work / 'package', work / 'native'
        shutil.copytree(ROOT / 'unity/com.szhlopp.foliageutil', stage, ignore=shutil.ignore_patterns('*.meta'))
        documentation = stage / 'Documentation~'
        documentation.mkdir()
        for name in ['UNITY.md', 'CARD_BAKE.md', 'MINERALS.md']:
            shutil.copy2(ROOT / 'docs' / name, documentation / name)
        subprocess.run(['cmake', '--install', str(build), '--config', 'Release', '--prefix', str(installed), '--component', 'NativePlugin'], check=True)
        plugin = stage / 'Plugins' / system / arch
        plugin.mkdir(parents=True)
        for file in (installed / 'Plugins').iterdir():
            verify_binary(file, system, arch)
            shutil.copy2(file, plugin / file.name)
            if system == 'macOS' and file.suffix == '.dylib':
                subprocess.run(['codesign', '--force', '--sign', '-', '--timestamp=none', str(plugin / file.name)], check=True)
        shutil.copytree(installed / 'ThirdParty', stage / 'ThirdParty')
        for path in sorted(stage.rglob('*')):
            if path.suffix != '.meta':
                metadata(path, path.relative_to(stage))
        files = sorted(str(p.relative_to(stage)) for p in stage.rglob('*') if p.is_file())
        (stage / '.foliageutil-package.json').write_text(json.dumps({'version': 1, 'native_api': 1, 'platform': system, 'arch': arch, 'files': files}, indent=2) + '\n')
        backup = work / 'previous'
        if output.exists():
            output.rename(backup)
        try:
            stage.rename(output)
        except Exception:
            if backup.exists():
                backup.rename(output)
            raise
    print(output / 'package.json')


if __name__ == '__main__':
    main()
