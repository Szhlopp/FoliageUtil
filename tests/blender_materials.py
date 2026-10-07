"""Optional Blender regression for saved FoliageUtil optics on petal-study.glb."""
import importlib.util
import sys
from pathlib import Path

import bpy


def main():
    root = Path(__file__).resolve().parents[1]
    spec = importlib.util.spec_from_file_location('foliage_preview', root / 'tools/render_blender.py')
    preview = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(preview)
    args = sys.argv[sys.argv.index('--') + 1:]
    if len(args) != 1:
        raise ValueError('Pass the exported petal-study.glb after --')
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    bpy.ops.import_scene.gltf(filepath=str(Path(args[0]).resolve()))
    applied = preview.apply_foliage_materials()
    assert applied and 'petal' in applied, 'Saved material extras were not applied'
    material = bpy.data.materials['petal']
    settings = applied['petal']
    nodes = material.node_tree.nodes
    shader = next(n for n in nodes if n.type == 'BSDF_PRINCIPLED')
    assert abs(shader.inputs['Subsurface Weight'].default_value - settings['subsurface']) < 1e-6
    assert abs(shader.inputs['Subsurface Scale'].default_value - settings['subsurface_scale']) < 1e-6
    for actual, expected in zip(shader.inputs['Subsurface Radius'].default_value, settings['subsurface_radius']):
        assert abs(actual - expected) < 1e-6
    tissue = nodes['Foliage translucency']
    coverage = nodes['Foliage alpha coverage']
    assert abs(tissue.inputs[0].default_value - settings['translucency']) < 1e-6
    assert coverage.inputs[0].is_linked, 'Lost the alpha texture and cutoff chain'
    assert coverage.inputs[1].links[0].from_node.type == 'BSDF_TRANSPARENT'
    assert coverage.inputs[2].links[0].from_node == tissue
    assert shader.inputs['Alpha'].default_value == 1 and not shader.inputs['Alpha'].is_linked
    output = next(n for n in nodes if n.type == 'OUTPUT_MATERIAL' and n.is_active_output)
    assert output.inputs['Surface'].links[0].from_node == coverage, 'Coverage must wrap both tissue shaders'
    pigment = nodes['Foliage transmission pigment']
    assert pigment.inputs[1].links[0].from_socket == shader.inputs['Base Color'].links[0].from_socket
    translucent = nodes['Foliage thin tissue']
    assert translucent.inputs['Normal'].links[0].from_socket == shader.inputs['Normal'].links[0].from_socket
    preview.apply_petal_subsurface(0, .0015)
    assert shader.inputs['Subsurface Weight'].default_value == 0, 'Zero override must disable saved subsurface'
    print('Blender material contract passed: saved optics, tint, normals, alpha coverage and zero override')


if __name__ == '__main__':
    main()
