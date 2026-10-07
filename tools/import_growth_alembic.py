#!/usr/bin/env python3
"""Import a FoliageUtil Alembic package, its PBR maps and empty-frame visibility into Blender."""
import argparse
import json
import sys
from pathlib import Path

import bpy

sys.path.insert(0, str(Path(__file__).resolve().parent))
from render_blender import apply_foliage_materials


def linear_vertex_color(nodes, links, attribute):
    # Blender's Alembic reader stores incoming linear C4f as raw byte colors;
    # its shader then decodes those bytes as sRGB. Undo that extra decode.
    group = bpy.data.node_groups.new('Foliage Alembic linear colors', 'ShaderNodeTree')
    group.interface.new_socket(name='Color', in_out='INPUT', socket_type='NodeSocketColor')
    group.interface.new_socket(name='Color', in_out='OUTPUT', socket_type='NodeSocketColor')
    source, output = group.nodes.new('NodeGroupInput'), group.nodes.new('NodeGroupOutput')
    separate, combine = group.nodes.new('ShaderNodeSeparateColor'), group.nodes.new('ShaderNodeCombineColor')
    group.links.new(source.outputs['Color'], separate.inputs['Color'])
    group.links.new(combine.outputs['Color'], output.inputs['Color'])

    def math_node(operation, *values):
        node = group.nodes.new('ShaderNodeMath')
        node.operation = operation
        for socket, value in zip(node.inputs, values):
            if isinstance(value, (float, int)):
                socket.default_value = value
            else:
                group.links.new(value, socket)
        return node.outputs[0]

    for index in range(3):
        channel = separate.outputs[index]
        high = math_node('MULTIPLY_ADD', math_node('POWER', channel, 1 / 2.4), 1.055, -.055)
        low = math_node('MULTIPLY', channel, 12.92)
        result = math_node('MULTIPLY_ADD', math_node('LESS_THAN', channel, .0031308), math_node('SUBTRACT', low, high), high)
        group.links.new(result, combine.inputs[index])
    instance = nodes.new('ShaderNodeGroup')
    instance.node_tree = group
    links.new(attribute.outputs['Color'], instance.inputs['Color'])
    return instance.outputs['Color']


def material_from_recipe(name, recipe, directory):
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    nodes, links = material.node_tree.nodes, material.node_tree.links
    shader = next(n for n in nodes if n.type == 'BSDF_PRINCIPLED')
    shader.inputs['Base Color'].default_value = recipe['base_color']
    shader.inputs['Metallic'].default_value = recipe['metallic']
    shader.inputs['Roughness'].default_value = recipe['roughness']
    material.use_backface_culling = not recipe['double_sided']
    attribute = nodes.new('ShaderNodeVertexColor')
    attribute.layer_name = 'Color'

    def texture(key, color):
        if key not in recipe:
            return None
        path = (directory / recipe[key]).resolve()
        if directory not in path.parents:
            raise ValueError('Texture path escapes the export package')
        node = nodes.new('ShaderNodeTexImage')
        # A source image can be used in both color and data roles.
        node.image = bpy.data.images.load(str(path), check_existing=False)
        node.image.colorspace_settings.name = 'sRGB' if color else 'Non-Color'
        return node

    def multiply(a, b):
        node = nodes.new('ShaderNodeMath')
        node.operation = 'MULTIPLY'
        for socket, value in zip(node.inputs, [a, b]):
            if isinstance(value, (int, float)):
                socket.default_value = value
            else:
                links.new(value, socket)
        return node.outputs[0]

    def pigment(a, b):
        node = nodes.new('ShaderNodeMixRGB')
        node.blend_type = 'MULTIPLY'
        node.inputs[0].default_value = 1
        for socket, value in zip(list(node.inputs)[1:], [a, b]):
            if isinstance(value, list):
                socket.default_value = value
            else:
                links.new(value, socket)
        return node.outputs[0]

    color = texture('base_color_texture', True)
    base = pigment(linear_vertex_color(nodes, links, attribute), recipe['base_color'])
    if color:
        base = pigment(base, color.outputs['Color'])
    links.new(base, shader.inputs['Base Color'])
    if recipe['alpha_mode'] != 'OPAQUE':
        alpha = multiply(attribute.outputs['Alpha'], recipe['base_color'][3])
        if color:
            alpha = multiply(alpha, color.outputs['Alpha'])
        if recipe['alpha_mode'] == 'MASK':
            cutoff = nodes.new('ShaderNodeMath')
            cutoff.operation = 'LESS_THAN'
            links.new(alpha, cutoff.inputs[0])
            cutoff.inputs[1].default_value = recipe['alpha_cutoff']
            invert = nodes.new('ShaderNodeMath')
            invert.operation = 'SUBTRACT'
            invert.inputs[0].default_value = 1
            links.new(cutoff.outputs[0], invert.inputs[1])
            alpha = invert.outputs[0]
        links.new(alpha, shader.inputs['Alpha'])
    packed = texture('metallic_roughness_texture', False)
    if packed:
        channels = nodes.new('ShaderNodeSeparateColor')
        links.new(packed.outputs['Color'], channels.inputs['Color'])
        links.new(multiply(channels.outputs['Green'], recipe['roughness']), shader.inputs['Roughness'])
        links.new(multiply(channels.outputs['Blue'], recipe['metallic']), shader.inputs['Metallic'])
    normal = texture('normal_texture', False)
    if normal:
        mapper = nodes.new('ShaderNodeNormalMap')
        mapper.uv_map = 'UVMap'
        links.new(normal.outputs['Color'], mapper.inputs['Color'])
        links.new(mapper.outputs['Normal'], shader.inputs['Normal'])
    optics = {'version': 1}
    for key in ['translucency', 'translucency_color', 'subsurface', 'subsurface_scale', 'subsurface_radius']:
        if key in recipe:
            optics[key] = recipe[key]
    material['foliageutil'] = optics
    return material


def import_package(directory):
    directory = Path(directory).resolve()
    manifest = json.loads((directory / 'manifest.json').read_text())
    if manifest.get('type') != 'foliage_export' or manifest.get('format') != 'alembic':
        raise ValueError('Expected a FoliageUtil Alembic export package')
    data = json.loads((directory / 'materials.json').read_text())
    scene = bpy.context.scene
    scene.render.fps = int(manifest['fps'])
    scene.render.fps_base = 1
    scene.frame_start, scene.frame_end = 0, manifest['frames'] - 1
    scene.frame_set(0)
    before = set(bpy.data.objects)
    bpy.ops.wm.alembic_import(filepath=str(directory / 'animation.abc'), as_background_job=False)
    objects = [o for o in bpy.data.objects if o not in before and o.type == 'MESH']
    materials = {name: material_from_recipe(name, recipe, directory) for name, recipe in data['materials'].items()}
    apply_foliage_materials(materials.values())
    bindings = {b['object']: b['material'] for b in data['bindings']}
    for obj in objects:
        modifier = next(m for m in obj.modifiers if m.type == 'MESH_SEQUENCE_CACHE')
        name = bindings[modifier.object_path]
        obj.name = name
        obj.data.materials.clear()
        obj.data.materials.append(materials[name])
        # Blender can retain the last mesh on a zero-face cache sample. Explicit
        # keys also cover backward scrubbing, while the archive stays standards compliant.
        previous = None
        for sample in manifest['snapshots']:
            hidden = sample['mesh']['materials'].get(name, 0) == 0
            if hidden != previous:
                obj.hide_render = obj.hide_viewport = hidden
                obj.keyframe_insert(data_path='hide_render', frame=sample['frame'])
                obj.keyframe_insert(data_path='hide_viewport', frame=sample['frame'])
                previous = hidden
    scene.frame_start, scene.frame_end = 0, manifest['frames'] - 1
    scene.frame_set(scene.frame_end)
    return objects, manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path, help='Output .blend file; keep its external Alembic package beside it')
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    if args.out.suffix != '.blend':
        parser.error('--out must end in .blend')
    output = args.out.resolve()
    if args.package.resolve() in output.parents:
        parser.error('Save the scene outside the managed export package')
    import_package(args.package)
    output.parent.mkdir(parents=True, exist_ok=True)
    for item in list(bpy.data.images) + list(bpy.data.cache_files):
        if item.filepath:
            item.filepath = bpy.path.relpath(item.filepath, start=str(output.parent))
    bpy.ops.wm.save_as_mainfile(filepath=str(output))
    print('Saved', output)


if __name__ == '__main__':
    main()
