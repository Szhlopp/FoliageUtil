#!/usr/bin/env python3
"""Render actual GLB variants side by side at the same physical scale in Blender."""
import argparse
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
from render_blender import apply_foliage_materials, point_at


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--models', nargs='+', required=True, type=Path)
    parser.add_argument('--labels', nargs='+', required=True)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--hdr', required=True, type=Path)
    parser.add_argument('--transparent-bounces', type=int, default=64, help='Cycles pass-through limit for layered alpha cards')
    parser.add_argument('--samples', type=int, default=48)
    parser.add_argument('--exposure', type=float, default=0, help='Shared camera exposure in stops')
    parser.add_argument('--width', type=int, default=1800)
    parser.add_argument('--height', type=int, default=700)
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    if not 1 <= args.transparent_bounces <= 256:
        parser.error('Transparent bounces must be within 1..256')
    if len(args.models) != len(args.labels) or not 1 <= len(args.models) <= 8:
        parser.error('Provide matching model and label lists, 1..8 items')
    if not 1 <= args.samples <= 512 or not 128 <= args.width <= 4096 or not 128 <= args.height <= 4096:
        parser.error('Invalid render limits')
    if not math.isfinite(args.exposure) or not -10 <= args.exposure <= 10:
        parser.error('Exposure must be finite and within -10..10 stops')
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    groups = []
    widths, heights = [], []
    for path in args.models:
        before = set(bpy.context.scene.objects)
        bpy.ops.import_scene.gltf(filepath=str(path.resolve()))
        objects = [o for o in bpy.context.scene.objects if o not in before and o.type == 'MESH']
        vertices = [o.matrix_world @ v.co for o in objects for v in o.data.vertices]
        lo = Vector([min(v[i] for v in vertices) for i in range(3)])
        hi = Vector([max(v[i] for v in vertices) for i in range(3)])
        groups.append((objects, lo, hi))
        widths.append(hi.x - lo.x)
        heights.append(hi.z)
    size = max(max(widths), max(heights))
    gap = max(max(widths) * 1.12, size * .7)
    span = gap * (len(groups) - 1) + max(widths)
    for i, (objects, lo, hi) in enumerate(groups):
        x = (i - (len(groups) - 1) / 2) * gap
        for obj in objects:
            obj.matrix_world.translation.x += x
        data = bpy.data.curves.new('Variant label', 'FONT')
        data.body, data.align_x, data.size = args.labels[i], 'CENTER', size * .038
        obj = bpy.data.objects.new('Variant label', data)
        bpy.context.collection.objects.link(obj)
        obj.location = (x, -size * .08, -size * .13)
        obj.rotation_euler.x = math.pi / 2
        mat = bpy.data.materials.get('Labels') or bpy.data.materials.new('Labels')
        mat.use_nodes = True
        shader = mat.node_tree.nodes.get('Principled BSDF')
        shader.inputs['Base Color'].default_value = (.8, .9, .85, 1)
        shader.inputs['Emission Color'].default_value = (.8, .9, .85, 1)
        shader.inputs['Emission Strength'].default_value = .7
        data.materials.append(mat)
    optics = apply_foliage_materials()
    scene = bpy.context.scene
    scene.world = bpy.data.worlds.new('Environment')
    scene.world.use_nodes = True
    nodes, links = scene.world.node_tree.nodes, scene.world.node_tree.links
    nodes.clear()
    env = nodes.new('ShaderNodeTexEnvironment')
    env.image = bpy.data.images.load(str(args.hdr.resolve()))
    illumination = nodes.new('ShaderNodeBackground')
    illumination.inputs['Strength'].default_value = .55
    links.new(env.outputs['Color'], illumination.inputs['Color'])
    background = nodes.new('ShaderNodeBackground')
    background.inputs['Color'].default_value = (.009, .016, .018, 1)
    ray = nodes.new('ShaderNodeLightPath')
    mix = nodes.new('ShaderNodeMixShader')
    links.new(ray.outputs['Is Camera Ray'], mix.inputs[0])
    links.new(illumination.outputs[0], mix.inputs[1])
    links.new(background.outputs[0], mix.inputs[2])
    output = nodes.new('ShaderNodeOutputWorld')
    links.new(mix.outputs[0], output.inputs['Surface'])
    for x in [-span * .35, span * .35]:
        data = bpy.data.lights.new('Softbox', 'AREA')
        data.energy, data.size = 100 * size ** 2, size * 1.8
        obj = bpy.data.objects.new('Softbox', data)
        bpy.context.collection.objects.link(obj)
        obj.location = (x, -size, size * 1.5)
        point_at(obj, (x, 0, size * .5))
    bpy.ops.object.camera_add(location=(0, -size * 5, size * .9))
    camera = bpy.context.object
    point_at(camera, (0, 0, size * .43))
    camera.data.type = 'ORTHO'
    camera.data.ortho_scale = max(span * 1.08, size * 1.32 * args.width / args.height)
    camera.data.clip_end = size * 100
    scene.camera = camera
    scene.render.engine = 'CYCLES'
    scene.cycles.samples, scene.cycles.seed = args.samples, 42
    scene.cycles.use_denoising = True
    scene.cycles.transparent_max_bounces = args.transparent_bounces
    scene.render.threads_mode, scene.render.threads = 'FIXED', 8
    scene.render.resolution_x, scene.render.resolution_y = args.width, args.height
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.view_settings.view_transform = 'AgX'
    scene.view_settings.exposure = args.exposure
    out = args.out.resolve()
    out.parent.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str(out)
    bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=str(out.with_suffix('.blend')))
    bpy.ops.render.render(write_still=True)
    out.with_suffix('.json').write_text(json.dumps({'models': [str(p.resolve()) for p in args.models], 'labels': args.labels, 'same_scale': True, 'transparent_bounces': args.transparent_bounces, 'samples': args.samples, 'exposure': args.exposure, 'size': [args.width, args.height], 'optics': optics}, indent=2) + '\n')


if __name__ == '__main__':
    main()
