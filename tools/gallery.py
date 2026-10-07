#!/usr/bin/env python3
"""Render the example meshes with TexUtil and assemble its native labeled sheet."""
import argparse
import json
import subprocess
import sys
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--texutil', required=True, type=Path)
    parser.add_argument('--foliage', type=Path, default=Path('build/foliageutil'))
    parser.add_argument('--samples', type=Path, default=Path('out/samples'), help='Prepared portable sample bundle')
    parser.add_argument('--out', type=Path, default=Path('out/gallery'))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = args.out.resolve()
    names = ['tree', 'bush', 'log', 'reeds', 'wheat', 'bamboo', 'flower', 'rose', 'plant', 'cards']
    nodes = {}
    items = []
    for name in names:
        cmd = [sys.executable, str(root / 'tools/preview.py'), str(args.samples.resolve() / 'graphs' / (name + '.json')), '--texutil', str(args.texutil.resolve()), '--foliage', str(args.foliage.resolve()), '--out', str(output / name), '--size', '384', '--elevation', '30' if name in ['rose', 'flower', 'plant'] else '12']
        subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)
        nodes[name] = {'op': 'image', 'path': str(output / name / 'preview.png')}
        items.append({'node': name, 'label': 'Crossed cards (opaque)' if name == 'cards' else name.capitalize()})
        print('Rendered ' + name, flush=True)
    recipe = {'size': 384, 'nodes': nodes, 'outputs': {'foliage-gallery.png': {'type': 'sheet', 'title': 'FoliageUtil | Seeded JSON graphs', 'columns': 5, 'cell': 320, 'items': items}}}
    path = output / 'gallery.json'
    path.write_text(json.dumps(recipe, indent=2) + '\n')
    subprocess.run([str(args.texutil.resolve()), str(path), '--out', str(output)], check=True)


if __name__ == '__main__':
    main()
