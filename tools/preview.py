#!/usr/bin/env python3
"""Generate a mesh and a solid-material Filament preview through an existing TexUtil build."""
import argparse
import json
import subprocess
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('recipe', type=Path)
    parser.add_argument('--foliage', type=Path, default=Path('build/foliageutil'))
    parser.add_argument('--texutil', type=Path, required=True)
    parser.add_argument('--out', type=Path, default=Path('out/preview'))
    parser.add_argument('--size', type=int, default=640)
    parser.add_argument('--elevation', type=float, default=15)
    parser.add_argument('--azimuth', type=float, default=25)
    args = parser.parse_args()
    recipe = args.recipe.resolve()
    output = args.out.resolve()
    report = json.loads(subprocess.check_output([str(args.foliage.resolve()), str(recipe), '--out', str(output), '--no-lods', '--no-growth', '--json'], text=True))
    meshes = [Path(item['path']) for item in report['outputs'] if Path(item['path']).suffix == '.glb']
    mesh = next((path for path in meshes if path.stem == recipe.stem), meshes[0])
    doc = json.loads(recipe.read_text())
    defaults = {'leaf': {'base_color': [.12, .32, .035, 1]}, 'bark': {'base_color': [.18, .09, .035, 1]}, 'cut': {'base_color': [.58, .37, .17, 1]}}
    defaults.update(doc.get('materials', {}))
    slots = next(item['materials'] for item in report['outputs'] if item['path'] == str(mesh))
    preview = {'size': 64, 'nodes': {}, 'outputs': {}}
    bindings = {}
    for slot in slots:
        material = defaults[slot]
        name = slot + '.mtlx'
        preview['outputs'][name] = {'type': 'materialx', 'name': slot, 'inputs': {'base_color': material.get('base_color', [.35, .5, .16, 1])[:3], 'specular_roughness': material.get('roughness', .8), 'metalness': material.get('metallic', 0)}}
        bindings[slot] = name
    # This helper renders scalar materials only. Avoid requiring nondegenerate
    # UV triangles at tiny boolean intersections when no texture is sampled.
    preview['outputs']['preview.png'] = {'type': 'preview', 'model': str(mesh), 'materials': bindings, 'projection': 'triplanar', 'environment': 'outdoor', 'background': '#202831', 'size': args.size, 'rotation': [args.elevation, args.azimuth, 0]}
    destination = output / 'texutil-preview.json'
    destination.write_text(json.dumps(preview, indent=2) + '\n')
    subprocess.run([str(args.texutil.resolve()), str(destination), '--out', str(output), '--json'], check=True)
    print(output / 'preview.png')


if __name__ == '__main__':
    main()
