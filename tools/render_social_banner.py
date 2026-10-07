#!/usr/bin/env python3
"""Render a README/social banner from actual FoliageUtil GLB assets in Blender."""
import argparse
import importlib.util
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector


def bounds(objects):
    vertices = []
    for obj in objects:
        if obj.type != 'MESH':
            continue
        vertices.extend(obj.matrix_world @ vertex.co for vertex in obj.data.vertices)
    if not vertices:
        raise ValueError('No geometry for banner framing')
    return Vector([min(v[i] for v in vertices) for i in range(3)]), Vector([max(v[i] for v in vertices) for i in range(3)])


def place_asset(path, label, x, y, bottom, height=None, rotation=(0, 0, 0), width=None):
    before = set(bpy.context.scene.objects)
    bpy.ops.import_scene.gltf(filepath=str(path))
    objects = list(set(bpy.context.scene.objects) - before)
    parent = bpy.data.objects.new(label + ' presentation scale', None)
    bpy.context.collection.objects.link(parent)
    for obj in objects:
        if obj.parent not in objects:
            world = obj.matrix_world.copy()
            obj.parent = parent
            obj.matrix_world = world
    parent.rotation_euler = [math.radians(a) for a in rotation]
    bpy.context.view_layer.update()
    lo, hi = bounds(objects)
    center = (lo + hi) * .5
    scale = width / (hi.x - lo.x) if width else height / (hi.z - lo.z)
    parent.scale = (scale, scale, scale)
    parent.location = (x - center.x * scale, y - center.y * scale, bottom - lo.z * scale)
    bpy.context.view_layer.update()
    placed_lo, placed_hi = bounds(objects)
    return {'source': str(path), 'label': label, 'height': height, 'width': width, 'position': [x, y, bottom], 'rotation': list(rotation), 'scale': scale, 'bounds': [list(placed_lo), list(placed_hi)]}


def text_object(text, x, z, size, font, color):
    data = bpy.data.curves.new(text, 'FONT')
    data.body, data.size, data.space_character = text, size, 1.0
    if font:
        data.font = bpy.data.fonts.load(str(font))
    obj = bpy.data.objects.new(text, data)
    bpy.context.collection.objects.link(obj)
    camera = bpy.context.scene.camera
    obj.location = camera.location + camera.rotation_euler.to_quaternion() @ Vector((x, z, -3))
    obj.rotation_euler = camera.rotation_euler
    obj.visible_shadow = False
    obj.visible_diffuse = False
    obj.visible_glossy = False
    material = bpy.data.materials.new(text + ' ink')
    material.use_nodes = True
    nodes, links = material.node_tree.nodes, material.node_tree.links
    nodes.clear()
    emission = nodes.new('ShaderNodeEmission')
    emission.inputs['Color'].default_value = (*color, 1)
    emission.inputs['Strength'].default_value = 2
    output = nodes.new('ShaderNodeOutputMaterial')
    links.new(emission.outputs[0], output.inputs['Surface'])
    data.materials.append(material)
    return obj


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--willow', type=Path, required=True)
    parser.add_argument('--birch', type=Path, required=True)
    parser.add_argument('--bamboo', type=Path, required=True)
    parser.add_argument('--rose', type=Path, required=True)
    parser.add_argument('--wheat', type=Path, required=True)
    parser.add_argument('--hdr', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--samples', type=int, default=96)
    parser.add_argument('--font', type=Path)
    parser.add_argument('--bold-font', type=Path)
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    if not 1 <= args.samples <= 4096 or args.out.suffix.lower() != '.png':
        parser.error('Use PNG output and 1..4096 samples')
    for path in [args.willow, args.birch, args.bamboo, args.rose, args.wheat, args.hdr]:
        if not path.is_file():
            raise FileNotFoundError(path)
    regular = args.font or Path('/System/Library/Fonts/Supplemental/Arial.ttf')
    bold = args.bold_font or Path('/System/Library/Fonts/Supplemental/Arial Bold.ttf')
    regular = regular if regular.is_file() else None
    bold = bold if bold.is_file() else None
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    spec = importlib.util.spec_from_file_location('foliage_preview', Path(__file__).with_name('render_blender.py'))
    preview = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(preview)
    scene = bpy.context.scene
    elevation = 0
    bpy.ops.object.camera_add(location=(0, -10 * math.cos(elevation), 10 * math.sin(elevation)))
    camera = bpy.context.object
    preview.point_at(camera, (0, 0, 0))
    camera.data.type, camera.data.ortho_scale = 'ORTHO', 4
    camera.data.clip_end = 100
    scene.camera = camera
    sources = [
        place_asset(args.willow.resolve(), 'Weeping willow', -1.80, .30, -1.13, 2.06, (0, 0, -20)),
        place_asset(args.birch.resolve(), 'Birch', 1.20, .30, -1.06, 1.86, (0, 0, -18)),
        place_asset(args.bamboo.resolve(), 'Bamboo', 1.67, .25, -1.06, 1.90, (0, 0, 12)),
        place_asset(args.wheat.resolve(), 'Wheat', 1.75, .05, -1.09, 1.29, (0, 0, -12)),
        place_asset(args.rose.resolve(), 'Rose', 1.16, -.80, -1.80, 1.48, (26, 0, 0)),
    ]
    optics = preview.apply_foliage_materials()
    white, muted, green = (1, 1, 1), (.48, .63, .72), (.035, .8, .38)
    text_object('Foliage', -.66, .20, .36, bold, white)
    bpy.context.view_layer.update()
    title = bpy.data.objects['Foliage']
    text_object('Util', -.66 + title.dimensions.x + .018, .308, .108, bold, white)
    text_object('PROCEDURAL FOLIAGE', -.64, .67, .072, regular, green)
    text_object('Seeded foliage', -.64, -.045, .16, regular, white)
    text_object('from JSON graphs', -.64, -.205, .16, regular, white)
    text_object('Trees  •  Plants  •  Flowers', -.64, -.60, .072, regular, muted)
    text_object('GLB + OBJ  •  TexUtil materials', -.64, -.73, .058, regular, muted)
    scene.world = bpy.data.worlds.new('Deep blue studio')
    scene.world.use_nodes = True
    nodes, links = scene.world.node_tree.nodes, scene.world.node_tree.links
    nodes.clear()
    env = nodes.new('ShaderNodeTexEnvironment')
    env.image = bpy.data.images.load(str(args.hdr.resolve()))
    illumination = nodes.new('ShaderNodeBackground')
    illumination.inputs['Strength'].default_value = .45
    links.new(env.outputs['Color'], illumination.inputs['Color'])
    background = nodes.new('ShaderNodeBackground')
    background.inputs['Color'].default_value = (.004, .010, .018, 1)
    ray = nodes.new('ShaderNodeLightPath')
    mix = nodes.new('ShaderNodeMixShader')
    links.new(ray.outputs['Is Camera Ray'], mix.inputs[0])
    links.new(illumination.outputs[0], mix.inputs[1])
    links.new(background.outputs[0], mix.inputs[2])
    output = nodes.new('ShaderNodeOutputWorld')
    links.new(mix.outputs[0], output.inputs['Surface'])
    lights = []
    for name, position, energy, size in [('Key', (-1.5, -2.5, 3), 180, 3), ('Rim', (2, 1, 2), 200, 2), ('Fill', (2, -2, .5), 35, 2), ('Willow fill', (-1.8, -1.5, 1.2), 45, 1.8)]:
        light = bpy.data.lights.new(name, 'AREA')
        light.energy, light.size = energy, size
        obj = bpy.data.objects.new(name, light)
        bpy.context.collection.objects.link(obj)
        obj.location = position
        target = (-1.6, 0, .3) if name == 'Willow fill' else (.9, 0, 0)
        preview.point_at(obj, target)
        lights.append({'name': name, 'position': list(position), 'target': list(target), 'energy': energy, 'size': size})
    scene.render.engine = 'CYCLES'
    scene.cycles.device, scene.cycles.samples, scene.cycles.seed = 'CPU', args.samples, 42
    scene.cycles.use_denoising = True
    scene.cycles.max_bounces, scene.cycles.transparent_max_bounces = 10, 64
    scene.render.threads_mode, scene.render.threads = 'FIXED', 8
    scene.render.resolution_x, scene.render.resolution_y, scene.render.resolution_percentage = 1280, 640, 100
    scene.render.image_settings.file_format, scene.render.image_settings.color_mode = 'PNG', 'RGB'
    scene.view_settings.view_transform, scene.view_settings.exposure = 'AgX', 0
    out = args.out.resolve()
    out.parent.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str(out)
    bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=str(out.with_suffix('.blend')))
    bpy.ops.render.render(write_still=True)
    manifest = {'size': [1280, 640], 'renderer': bpy.app.version_string, 'samples': args.samples, 'seed': 42, 'sources': sources, 'lights': lights, 'camera_elevation': 0, 'optics': optics, 'fonts': [str(regular), str(bold)], 'title_style': {'color': 'white', 'util': 'superscript', 'util_size_ratio': .3, 'util_baseline_raise': .108, 'title_x': -.66, 'copy_x': -.64}, 'copy': ['FoliageUtil', 'PROCEDURAL FOLIAGE', 'Seeded foliage', 'from JSON graphs', 'Trees  •  Plants  •  Flowers', 'GLB + OBJ  •  TexUtil materials'], 'note': 'Actual imported weeping willow on the left and birch, bamboo, wheat and rose GLBs, independently scaled and arranged on the right with their bases cropped by the bottom edge. The rose bloom sits low in the foreground. No ground, water or lily pad is present. Util is a small white superscript.', 'output': str(out)}
    out.with_suffix('.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('Rendered banner: ' + str(out), flush=True)


if __name__ == '__main__':
    main()
