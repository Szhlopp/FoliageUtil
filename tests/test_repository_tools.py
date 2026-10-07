#!/usr/bin/env python3
"""Check source-bundle and cleanup boundaries without touching real project outputs."""
import json
import sys
import tempfile
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from clean_generated import plan
from prepare_samples import SAMPLES, dependencies, load_manifest, prepare_sources, generated_files


class RepositoryTools(unittest.TestCase):
    def test_all_recipe_dependencies_are_packaged(self):
        for job in load_manifest()['materials']:
            deps = dependencies(SAMPLES / 'materials' / job['recipe'], SAMPLES / 'materials')
            self.assertTrue(deps)
            self.assertTrue(all((SAMPLES / 'materials').resolve() in p.parents for p in deps))

    def test_source_output_protection(self):
        with self.assertRaisesRegex(ValueError, 'separate'):
            prepare_sources(SAMPLES / 'generated')
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            (root / 'personal.txt').write_text('preserve')
            with self.assertRaisesRegex(ValueError, 'empty directory'):
                prepare_sources(root)
            self.assertEqual((root / 'personal.txt').read_text(), 'preserve')
            bundle = root / 'bundle'
            prepare_sources(bundle, fixture=True)
            with self.assertRaisesRegex(ValueError, 'mix diagnostic'):
                prepare_sources(bundle)
            self.assertEqual({p.name for p in (bundle / 'graphs').glob('*.json')}, {p.name for p in (SAMPLES / 'graphs').glob('*.json')})
            self.assertFalse(list(bundle.rglob('*.png')))

    def test_generated_atlas_lists_missing_images(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            recipe = root / 'recipe.json'
            recipe.write_text(json.dumps({'outputs': {'atlas.json': {'type': 'spritesheet'}}}))
            (root / 'atlas.json').write_text(json.dumps({'images': [{'file': 'atlas.assets/color.png'}]}))
            self.assertIn(root / 'atlas.assets/color.png', generated_files(recipe, root))

    def test_cleanup_preserves_sources_finals_and_referenced_models(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            names = ['Archive.zip', 'samples/graphs/tree.json', 'docs/images/banner.png', 'out/tree/tree.glb', 'out/tree/tree.obj', 'out/tree/tree.mtl', 'out/tree/tree-textures/color.png', 'out/referenced/mesh.obj', 'out/referenced/mesh.glb', 'out/referenced/scene.json', 'out/final.blend', 'out/final.blend1', 'out/social/foliageutil-social-preview.blend', 'out/social/old.blend', 'out/social/old.png', 'out/willow-card-bake-final/willow-tree-card-baked.glb', 'out/willow-card-bake/old.glb', 'out/willow-card-bake/recipe.json', 'build/CMakeCache.txt', 'build/examples/test/test.glb']
            for name in names:
                p = root / name
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_text('{}' if p.suffix == '.json' else 'data')
            (root / 'out/referenced/scene.json').write_text(json.dumps({'model': 'mesh.obj'}))
            proposed = {str(p.relative_to(root)) for p, _ in plan(root, True)}
            for name in ['out/tree/tree.obj', 'out/tree/tree.mtl', 'out/tree/tree-textures', 'out/final.blend1', 'out/social/old.blend', 'out/willow-card-bake/old.glb', 'build/examples']:
                self.assertIn(name, proposed)
            for name in ['Archive.zip', 'samples/graphs/tree.json', 'docs/images/banner.png', 'out/tree/tree.glb', 'out/referenced/mesh.obj', 'out/final.blend', 'out/social/foliageutil-social-preview.blend', 'out/social/old.png', 'out/willow-card-bake-final/willow-tree-card-baked.glb', 'out/willow-card-bake/recipe.json']:
                self.assertNotIn(name, proposed)


if __name__ == '__main__':
    unittest.main()
