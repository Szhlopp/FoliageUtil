#!/usr/bin/env python3
"""Render exported GLBs in individually framed catalog cells using Blender."""
import argparse
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
from render_blender import apply_foliage_materials, point_at


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--models', nargs='+', required=True, type=Path)
    parser.add_argument('--labels', nargs='+', required=True)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--hdr', required=True, type=Path)
    parser.add_argument('--title', default='FOLIAGE COLLECTION')
    parser.add_argument('--columns', type=int, default=3)
    parser.add_argument('--azimuth', type=float, default=0)
    parser.add_argument('--elevation', type=float, default=24)
    parser.add_argument('--samples', type=int, default=48)
    parser.add_argument('--width', type=int, default=1800)
    parser.add_argument('--height', type=int, default=1200)
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    if len(args.models) != len(args.labels) or not 1 <= len(args.models) <= 16:
        parser.error('Provide matching model and label lists, 1..16 items')
    if not 1 <= args.columns <= 4 or not 1 <= args.samples <= 512:
        parser.error('Invalid column or sample count')
    if not 128 <= args.width <= 4096 or not 128 <= args.height <= 4096:
        parser.error('Image dimensions must be within 128..4096')
    if not math.isfinite(args.azimuth) or not -80 <= args.elevation <= 80:
        parser.error('Invalid viewing angle')
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    elevation = math.radians(args.elevation)
    right = Vector((1, 0, 0))
    up = Vector((0, math.sin(elevation), math.cos(elevation)))
    depth = right.cross(up)
    rows = math.ceil(len(args.models) / args.columns)
    rotation = Matrix.Rotation(math.radians(args.azimuth), 4, 'Z')
    scene = bpy.context.scene
    bpy.ops.object.camera_add(location=depth * 12)
    camera = bpy.context.object
    point_at(camera, (0, 0, 0))
    camera.data.type = 'ORTHO'
    camera.data.ortho_scale = max(args.columns * 2.2, (rows * 1.95 + 1.0) * args.width / args.height)
    scene.camera = camera
    label_material = bpy.data.materials.new('Catalog text')
    label_material.use_nodes = True
    shader = label_material.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = (.65, .77, .65, 1)
    shader.inputs['Emission Color'].default_value = (.65, .77, .65, 1)
    shader.inputs['Emission Strength'].default_value = .8

    def label(text, x, y, size):
        data = bpy.data.curves.new(text, 'FONT')
        data.body, data.align_x, data.size = text, 'CENTER', size
        obj = bpy.data.objects.new(text, data)
        bpy.context.collection.objects.link(obj)
        obj.location = right * x + up * y + depth * 1.1
        obj.rotation_euler = camera.rotation_euler
        data.materials.append(label_material)

    records = []
    foliage_materials = set()
    for i, path in enumerate(args.models):
        before = set(scene.objects)
        bpy.ops.import_scene.gltf(filepath=str(path.resolve()))
        objects = [o for o in scene.objects if o not in before and o.type == 'MESH']
        if not objects:
            raise ValueError(f'No mesh in {path}')
        foliage_materials.update(slot.material for obj in objects for slot in obj.material_slots if slot.material)
        matrices = {o: rotation @ o.matrix_world for o in objects}
        vertices = [matrices[o] @ v.co for o in objects for v in o.data.vertices]
        lo = Vector([min(v.dot(axis) for v in vertices) for axis in (right, up, depth)])
        hi = Vector([max(v.dot(axis) for v in vertices) for axis in (right, up, depth)])
        scale = min(1.83 / max(hi.x - lo.x, .001), 1.32 / max(hi.y - lo.y, .001))
        x = (i % args.columns - (args.columns - 1) / 2) * 2.2
        y = ((rows - 1) / 2 - i // args.columns) * 1.95
        offset = right * (x - (lo.x + hi.x) * .5 * scale) + up * (y - .46 - lo.y * scale) - depth * ((lo.z + hi.z) * .5 * scale)
        for obj in objects:
            obj.matrix_world = Matrix.Translation(offset) @ Matrix.Scale(scale, 4) @ matrices[obj]
        triangles = sum(len(poly.vertices) - 2 for obj in objects for poly in obj.data.polygons)
        label(args.labels[i], x, y - .71, .10)
        label(f'{triangles:,} triangles', x, y - .88, .075)
        records.append({'model': str(path.resolve()), 'label': args.labels[i], 'triangles': triangles, 'display_scale': scale})
    label(args.title, 0, rows * .975 + .25, .145)
    label('Actual exported geometry  /  Individually framed assets', 0, -rows * .975 - .12, .069)
    optics = apply_foliage_materials(foliage_materials)
    scene.world = bpy.data.worlds.new('Environment')
    scene.world.use_nodes = True
    nodes, links = scene.world.node_tree.nodes, scene.world.node_tree.links
    nodes.clear()
    env = nodes.new('ShaderNodeTexEnvironment')
    env.image = bpy.data.images.load(str(args.hdr.resolve()))
    illumination = nodes.new('ShaderNodeBackground')
    illumination.inputs['Strength'].default_value = .65
    links.new(env.outputs['Color'], illumination.inputs['Color'])
    background = nodes.new('ShaderNodeBackground')
    background.inputs['Color'].default_value = (.011, .018, .015, 1)
    ray = nodes.new('ShaderNodeLightPath')
    mix = nodes.new('ShaderNodeMixShader')
    links.new(ray.outputs['Is Camera Ray'], mix.inputs[0])
    links.new(illumination.outputs[0], mix.inputs[1])
    links.new(background.outputs[0], mix.inputs[2])
    output = nodes.new('ShaderNodeOutputWorld')
    links.new(mix.outputs[0], output.inputs['Surface'])
    for x in (-3, 3):
        data = bpy.data.lights.new('Softbox', 'AREA')
        data.energy, data.size = 600, 5
        obj = bpy.data.objects.new('Softbox', data)
        bpy.context.collection.objects.link(obj)
        obj.location = (x, -4, 6)
        point_at(obj, (0, 0, 0))
    scene.render.engine = 'CYCLES'
    scene.cycles.samples, scene.cycles.seed = args.samples, 42
    scene.cycles.use_denoising = True
    scene.cycles.transparent_max_bounces = 64
    scene.render.threads_mode, scene.render.threads = 'FIXED', 8
    scene.render.resolution_x, scene.render.resolution_y = args.width, args.height
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.view_settings.view_transform = 'AgX'
    out = args.out.resolve()
    out.parent.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str(out)
    bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=str(out.with_suffix('.blend')))
    bpy.ops.render.render(write_still=True)
    out.with_suffix('.json').write_text(json.dumps({'assets': records, 'same_scale': False, 'azimuth': args.azimuth, 'elevation': args.elevation, 'transparent_bounces': 64, 'samples': args.samples, 'size': [args.width, args.height], 'optics': optics}, indent=2) + '\n')


if __name__ == '__main__':
    main()
