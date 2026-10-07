#!/usr/bin/env python3
"""Bundle one prepared graph and its actual texture/atlas/mesh dependencies for Unity StreamingAssets."""
import argparse
import hashlib
import json
import tempfile
from pathlib import Path


def package(recipe, output, max_mb=512):
    recipe, output = Path(recipe).resolve(), Path(output).resolve()
    if output == recipe.parent or output in recipe.parents:
        raise ValueError('Choose a separate output directory')
    if output.exists() and any(output.iterdir()):
        raise ValueError('Choose an empty output directory; existing bundles are preserved')
    if not 1 <= max_mb <= 4096:
        raise ValueError('Runtime package limit must be within 1..4096 MiB')
    if recipe.stat().st_size > 8 * 1024 * 1024:
        raise ValueError('Recipe exceeds 8 MiB')
    document = json.loads(recipe.read_text(encoding='utf-8'))
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.foliage-recipe-', dir=output.parent) as temp:
        stage = Path(temp)
        (stage / 'assets').mkdir()
        copied = {}
        total = 0

        def asset(source):
            nonlocal total
            source = source.resolve(strict=True)
            if source in copied:
                return copied[source]
            size = source.stat().st_size
            if size > 64 * 1024 * 1024:
                raise ValueError('Runtime asset/package size limit exceeded')
            data = source.read_bytes()
            name = hashlib.sha256(data).hexdigest()[:24] + source.suffix.lower()
            destination = stage / 'assets' / name
            if not destination.exists():
                if total + size > max_mb * 1024 * 1024:
                    raise ValueError('Runtime package size limit exceeded')
                destination.write_bytes(data)
                total += size
            copied[source] = 'assets/' + name
            return copied[source]

        for material in document.get('materials', {}).values():
            for key in ['base_color_texture', 'normal_texture', 'metallic_roughness_texture']:
                if key in material:
                    material[key] = asset(recipe.parent / material[key])
        def node_assets(node, op):
            if op == 'mesh' and 'path' in node:
                node['path'] = asset(recipe.parent / node['path'])
            if 'atlas' in node:
                source = (recipe.parent / node['atlas']).resolve(strict=True)
                if source.stat().st_size > 1024 * 1024:
                    raise ValueError('Atlas manifest exceeds 1 MiB')
                atlas = json.loads(source.read_text(encoding='utf-8'))
                if atlas.get('format') != 'texutil-spritesheet':
                    raise ValueError('Unsupported atlas format')
                for image in atlas['images']:
                    image['file'] = Path(asset(source.parent / image['file'])).name
                text = json.dumps(atlas, indent=2) + '\n'
                name = hashlib.sha256(text.encode()).hexdigest()[:24] + '.atlas.json'
                (stage / 'assets' / name).write_text(text, encoding='utf-8')
                node['atlas'] = 'assets/' + name
        for profile in document.get('lods', {}).values():
            for name, patch in profile.get('overrides', {}).items():
                node_assets(patch, document['nodes'][name]['op'])
        for node in document['nodes'].values():
            node_assets(node, node['op'])
        (stage / 'graph.json').write_text(json.dumps(document, indent=2) + '\n', encoding='utf-8')
        files = sorted(str(p.relative_to(stage)) for p in stage.rglob('*') if p.is_file())
        (stage / 'bundle.json').write_text(json.dumps({'type': 'foliage_runtime_recipe', 'version': 1, 'graph': 'graph.json', 'files': files}, indent=2) + '\n', encoding='utf-8')
        if sum(p.stat().st_size for p in stage.rglob('*') if p.is_file()) > max_mb * 1024 * 1024:
            raise ValueError('Runtime package size limit exceeded')
        if output.exists():
            output.rmdir()
        # Same-filesystem rename leaves an incomplete package unpublished on failure.
        stage.rename(output)
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('recipe', type=Path)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--max-mb', default=512, type=int)
    args = parser.parse_args()
    if not 1 <= args.max_mb <= 4096:
        parser.error('--max-mb must be within 1..4096')
    print(package(args.recipe, args.out, args.max_mb))


if __name__ == '__main__':
    main()
