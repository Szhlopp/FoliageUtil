#!/usr/bin/env python3
"""Portable recipe packaging keeps every asset reachable without the source tree."""
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from package_unity_recipe import package


class UnityTools(unittest.TestCase):
    def test_portable_assets_atlas_lod_and_deduplication(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'source'
            source.mkdir()
            (source / 'leaf.png').write_bytes(b'fixture image bytes')
            (source / 'duplicate.png').write_bytes(b'fixture image bytes')
            (source / 'mesh.obj').write_text('v 0 0 0\n')
            atlas = {'format': 'texutil-spritesheet', 'grid': [2, 1], 'images': [{'file': 'leaf.png'}], 'cells': [{'index': 0}]}
            (source / 'atlas.json').write_text(json.dumps(atlas))
            graph = {'materials': {'leaf': {'base_color_texture': 'leaf.png', 'normal_texture': 'duplicate.png'}}, 'nodes': {'mesh': {'op': 'mesh', 'path': 'mesh.obj'}, 'instances': {'op': 'instance', 'atlas': 'atlas.json'}}, 'lods': {'Low': {'overrides': {'mesh': {'path': 'mesh.obj'}, 'instances': {'atlas': 'atlas.json'}}}}}
            (source / 'graph.json').write_text(json.dumps(graph))
            result = package(source / 'graph.json', root / 'bundle')
            shutil.rmtree(source)
            moved = root / 'relocated'
            result.rename(moved)
            saved = json.loads((moved / 'graph.json').read_text())
            self.assertEqual(saved['materials']['leaf']['base_color_texture'], saved['materials']['leaf']['normal_texture'])
            for field in saved['materials']['leaf'].values():
                self.assertTrue((moved / field).is_file())
            self.assertTrue((moved / saved['nodes']['mesh']['path']).is_file())
            for node in [saved['nodes']['instances'], saved['lods']['Low']['overrides']['instances']]:
                manifest = moved / node['atlas']
                contents = json.loads(manifest.read_text())
                self.assertEqual(contents['grid'], [2, 1])
                self.assertTrue((manifest.parent / contents['images'][0]['file']).is_file())
            self.assertEqual(saved['nodes']['mesh']['path'], saved['lods']['Low']['overrides']['mesh']['path'])
            self.assertEqual(len(list((moved / 'assets').glob('*.png'))), 1)

    def test_failure_preserves_inputs_and_existing_outputs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            graph = root / 'graph.json'
            graph.write_text(json.dumps({'nodes': {'mesh': {'op': 'mesh', 'path': 'missing.obj'}}}))
            with self.assertRaises(FileNotFoundError):
                package(graph, root / 'bundle')
            self.assertFalse((root / 'bundle').exists())
            (root / 'bundle').mkdir()
            marker = root / 'bundle/personal.txt'
            marker.write_text('keep')
            with self.assertRaisesRegex(ValueError, 'empty output'):
                package(graph, root / 'bundle')
            self.assertEqual(marker.read_text(), 'keep')
            with self.assertRaisesRegex(ValueError, 'separate'):
                package(graph, root)

    def test_budget_failure_does_not_publish_partial_bundle(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'large.png').write_bytes(b'x' * (1024 * 1024))
            graph = root / 'graph.json'
            graph.write_text(json.dumps({'materials': {'leaf': {'base_color_texture': 'large.png'}}, 'nodes': {}}))
            with self.assertRaisesRegex(ValueError, 'size limit'):
                package(graph, root / 'bundle', 1)
            self.assertFalse((root / 'bundle').exists())


if __name__ == '__main__':
    unittest.main()
