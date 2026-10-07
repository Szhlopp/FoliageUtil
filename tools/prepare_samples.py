#!/usr/bin/env python3
"""Prepare a portable sample bundle from checked-in graphs and TexUtil recipes."""
import argparse
import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SAMPLES = ROOT / 'samples'
MARKER = '.foliage-samples.json'


def inside(path, root):
    return path == root or root in path.parents


def load_manifest(source=SAMPLES):
    manifest = json.loads((source / 'manifest.json').read_text())
    if manifest.get('version') != 1:
        raise ValueError('Unsupported sample manifest version')
    for job in manifest['materials']:
        recipe = (source / 'materials' / job['recipe']).resolve()
        if not inside(recipe, (source / 'materials').resolve()) or not recipe.is_file():
            raise ValueError(f'Missing or escaping material source: {job}')
    return manifest


def dependencies(path, root, found=None):
    found = set() if found is None else found
    path = path.resolve()
    if path in found:
        return found
    if not inside(path, root.resolve()) or not path.is_file():
        raise ValueError(f'Sample dependency must be packaged under materials/: {path}')
    found.add(path)
    doc = json.loads(path.read_text())
    for value in doc.get('imports', {}).values():
        if isinstance(value, dict):
            value = value['path']
        dependencies(path.parent / value, root, found)
    for output in doc.get('outputs', {}).values():
        if isinstance(output, dict) and output.get('type') == 'spritesheet':
            dependencies(path.parent / output['source'], root, found)
    for node in doc.get('nodes', {}).values():
        if node.get('op') == 'image':
            image = (path.parent / node['path']).resolve()
            if not inside(image, root.resolve()) or not image.is_file():
                raise ValueError(f'Missing packaged image input: {image}')
            found.add(image)
    return found


def prepare_sources(output, source=SAMPLES, fixture=False):
    output, source = output.resolve(), source.resolve()
    if inside(output, source) or inside(source, output):
        raise ValueError('Output must be separate from the checked-in sample sources')
    marker = output / MARKER
    if output.exists() and any(output.iterdir()) and not marker.is_file():
        raise ValueError(f'Output is not an existing sample bundle or empty directory: {output}')
    old = json.loads(marker.read_text()) if marker.is_file() else {}
    if old and old.get('fixture', False) != fixture:
        raise ValueError('Do not mix diagnostic test fixtures with rendered sample textures')
    manifest = load_manifest(source)
    for job in manifest['materials']:
        dependencies(source / 'materials' / job['recipe'], source / 'materials')
    output.mkdir(parents=True, exist_ok=True)
    marker.write_text(json.dumps({'version': 1, 'fixture': fixture, 'jobs': old.get('jobs', {})}, indent=2) + '\n')
    for srcdir, destdir in [('graphs', 'graphs'), ('materials', 'assets')]:
        for src in (source / srcdir).rglob('*'):
            if src.is_file() and src.suffix in {'.json', '.md'}:
                dest = output / destdir / src.relative_to(source / srcdir)
                if not inside(dest.resolve(), output):
                    raise ValueError(f'Output symlink escapes sample bundle: {dest}')
                dest.parent.mkdir(parents=True, exist_ok=True)
                if not dest.exists() or dest.read_bytes() != src.read_bytes():
                    shutil.copyfile(src, dest)
    shutil.copyfile(source / 'manifest.json', output / 'source-manifest.json')
    return old


def generated_files(recipe, output):
    doc = json.loads(recipe.read_text())
    result = []
    for name, spec in doc['outputs'].items():
        file = output / name
        if not inside(file.resolve(), output.resolve()):
            raise ValueError(f'Escaping generated file: {file}')
        result.append(file)
        if isinstance(spec, dict) and spec.get('type') == 'spritesheet' and file.exists():
            atlas = json.loads(file.read_text())
            for image in atlas['images']:
                image_path = file.parent / image['file']
                if not inside(image_path.resolve(), output.resolve()):
                    raise ValueError(f'Escaping atlas image: {image_path}')
                result.append(image_path)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--texutil', required=True, type=Path, help='Existing TexUtil executable; a source checkout is not required')
    parser.add_argument('--out', type=Path, default=ROOT / 'out/samples')
    parser.add_argument('--threads', type=int, default=8)
    parser.add_argument('--force', action='store_true', help='Re-render even when source hashes and generated files match')
    args = parser.parse_args()
    try:
        if not 1 <= args.threads <= 256:
            raise ValueError('Threads must be within 1..256')
        executable = args.texutil.resolve()
        if not executable.is_file():
            raise ValueError(f'TexUtil executable does not exist: {executable}')
        old = prepare_sources(args.out)
        output = args.out.resolve()
        binary_hash = hashlib.sha256(executable.read_bytes()).hexdigest()
        jobs = {}
        for job in load_manifest()['materials']:
            relative = job['recipe']
            source = SAMPLES / 'materials' / relative
            recipe = output / 'assets' / relative
            digest = hashlib.sha256(binary_hash.encode())
            for path in sorted(dependencies(source, SAMPLES / 'materials')):
                digest.update(path.relative_to(SAMPLES).as_posix().encode())
                digest.update(path.read_bytes())
            fingerprint = digest.hexdigest()
            previous = old.get('jobs', {}).get(relative, {})
            files = generated_files(recipe, recipe.parent)
            valid = not args.force and previous.get('source_hash') == fingerprint
            valid = valid and all(f.is_file() and previous.get('files', {}).get(f.relative_to(output).as_posix()) == hashlib.sha256(f.read_bytes()).hexdigest() for f in files)
            if not valid:
                print(f'Rendering {relative}', flush=True)
                result = subprocess.run([str(executable), str(recipe), '--out', str(recipe.parent), '--threads', str(args.threads), '--json'], capture_output=True, text=True)
                if result.returncode:
                    raise RuntimeError(f'TexUtil failed for {relative}:\n{result.stderr}\n{result.stdout}')
                files = generated_files(recipe, recipe.parent)
            else:
                print(f'Cached {relative}', flush=True)
            jobs[relative] = {'source_hash': fingerprint, 'files': {f.relative_to(output).as_posix(): hashlib.sha256(f.read_bytes()).hexdigest() for f in files}}
            (output / MARKER).write_text(json.dumps({'version': 1, 'fixture': False, 'jobs': jobs}, indent=2) + '\n')
        print(f'Prepared {len(list((output / "graphs").glob("*.json")))} graphs and {len(jobs)} material packages in {output}')
    except (ValueError, OSError, RuntimeError, KeyError) as error:
        parser.exit(1, f'prepare_samples: {error}\n')


if __name__ == '__main__':
    main()
