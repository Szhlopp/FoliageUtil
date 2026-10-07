#!/usr/bin/env python3
"""Print a conservative local cleanup plan; --apply removes only listed generated artifacts."""
import argparse
import json
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def file_bytes(path):
    return path.stat().st_size if path.is_file() else sum(p.stat().st_size for p in path.rglob('*') if p.is_file() and not p.is_symlink())


def plan(root, intermediates=False):
    root = root.resolve()
    output = root / 'out'
    selected = {}
    def add(path, reason):
        if path.exists() and not path.is_symlink() and root in path.resolve().parents:
            selected[path] = reason
    for build in root.glob('build*'):
        if not build.is_dir() or build.is_symlink() or not (build / 'CMakeCache.txt').is_file():
            continue
        for name in ['examples', 'test-output', 'cli-test', 'card-bake-test', 'development-test']:
            add(build / name, 'reproducible test exports')
    if output.exists():
        referenced_models = set()
        for source in output.rglob('*.json'):
            if source.is_symlink() or source.stat().st_size > 8 * 1024 * 1024:
                continue
            try:
                doc = json.loads(source.read_text())
            except (ValueError, OSError):
                continue
            def visit(value):
                if isinstance(value, dict):
                    model = value.get('model', value.get('path') if value.get('op') == 'mesh' else None)
                    if isinstance(model, str):
                        referenced_models.add((source.parent / model).resolve())
                    for child in value.values():
                        visit(child)
                elif isinstance(value, list):
                    for child in value:
                        visit(child)
            visit(doc)
        for file in output.rglob('*'):
            if file.is_symlink() or not file.is_file():
                continue
            if file.suffix.startswith('.blend') and file.suffix[6:].isdigit():
                add(file, 'Blender backup; current scene retained')
            if file.suffix == '.obj' and file.with_suffix('.glb').is_file() and file.resolve() not in referenced_models:
                add(file, 'duplicate OBJ; matching self-contained GLB retained')
                add(file.with_suffix('.mtl'), 'MTL for duplicate OBJ')
                textures = file.parent / (file.stem + '-textures')
                if not any(textures.resolve() == p or textures.resolve() in p.parents for p in referenced_models):
                    add(textures, 'texture copies for duplicate OBJ')
        if intermediates:
            # These exact development folders have newer retained final counterparts.
            obsolete = ['rooted-tree-repeat', 'rooted-tree-seed43', 'rooted-tree-filament', 'willow-filament']
            for parent in output.iterdir():
                if parent.is_dir() and not parent.is_symlink():
                    for name in ['repeat', 'seed-43', 'filament']:
                        if (parent / name).is_dir():
                            obsolete.append((parent / name).relative_to(output).as_posix())
            if (output / 'willow-card-bake-final/willow-tree-card-baked.glb').is_file():
                obsolete.append('willow-card-bake')
            for name in obsolete:
                folder = output / name
                if folder.exists():
                    for file in folder.rglob('*'):
                        if file.is_file() and file.suffix in {'.glb', '.obj', '.mtl', '.blend'} and file.resolve() not in referenced_models:
                            add(file, 'obsolete intermediate; recipes and previews retained')
                    if name == 'willow-card-bake':
                        for package in folder.glob('*.cardbake'):
                            for file in package.glob('*.png'):
                                add(file, 'superseded high-resolution card bake; final package retained')
            final_banner = output / 'social/foliageutil-social-preview.blend'
            if final_banner.is_file():
                for file in final_banner.parent.glob('*.blend'):
                    if file != final_banner:
                        add(file, 'obsolete banner iteration; final scene and all preview PNGs retained')
    # Avoid counting/removing a file twice when its whole test directory is selected.
    return [(p, why) for p, why in sorted(selected.items()) if not any(parent in selected for parent in p.parents)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apply', action='store_true')
    parser.add_argument('--intermediates', action='store_true', help='Include explicitly identified obsolete bake/banner studies')
    args = parser.parse_args()
    paths = plan(ROOT, args.intermediates)
    total = 0
    records = []
    for path, reason in paths:
        size = file_bytes(path)
        total += size
        records.append({'path': path.relative_to(ROOT).as_posix(), 'bytes': size, 'reason': reason})
        print(f'{size / 1024**2:9.1f} MiB  {path.relative_to(ROOT)}  ({reason})')
    print(f'{"Removing" if args.apply else "Dry run:"} {total / 1024**3:.2f} GiB in {len(paths)} generated paths')
    if args.apply:
        log = ROOT / 'out/cleanup-report.json'
        log.parent.mkdir(parents=True, exist_ok=True)
        log.write_text(json.dumps({'bytes': total, 'paths': records}, indent=2) + '\n')
        for path, _ in paths:
            if path.is_dir():
                shutil.rmtree(path)
            else:
                path.unlink()
        print(f'Cleanup report: {log}')


if __name__ == '__main__':
    main()
