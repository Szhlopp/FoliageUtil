#!/usr/bin/env python3
"""Render native continuous growth through Blender and save a keyed mesh sequence."""
import argparse
import hashlib
import json
import math
import re
import subprocess
import sys
import time
from pathlib import Path

import bpy
from mathutils import Vector


def visibility(obj, start, end, last):
    for frame, hidden in [(1, True), (start, False)] + ([] if end == last else [(end + 1, True)]):
        obj.hide_render = hidden
        obj.hide_viewport = hidden
        obj.keyframe_insert(data_path='hide_render', frame=frame)
        obj.keyframe_insert(data_path='hide_viewport', frame=frame)


def overlay_text(camera, label, x, y, size, material):
    data = bpy.data.curves.new(label, 'FONT')
    data.body, data.size = label, size
    font = Path('/System/Library/Fonts/Supplemental/Arial.ttf')
    if font.is_file():
        data.font = bpy.data.fonts.load(str(font), check_existing=True)
    obj = bpy.data.objects.new(label, data)
    bpy.context.collection.objects.link(obj)
    obj.rotation_euler = camera.rotation_euler
    obj.location = camera.location + camera.rotation_euler.to_quaternion() @ Vector((x, y, -3))
    obj.visible_shadow = obj.visible_diffuse = obj.visible_glossy = False
    data.materials.append(material)
    return obj


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--recipe', required=True, type=Path)
    parser.add_argument('--generator', required=True, type=Path)
    parser.add_argument('--output-name', required=True)
    parser.add_argument('--scene', required=True, type=Path, help='Verified mature Blender scene supplying the camera, materials, ground and lighting')
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--frames', type=int, default=192)
    parser.add_argument('--fps', type=int, default=24)
    parser.add_argument('--samples', type=int, default=24)
    parser.add_argument('--threads', type=int, default=8)
    parser.add_argument('--resume', action='store_true', help='Reuse completed PNGs after checking the recipe and render settings; rebuild the Blender timeline')
    parser.add_argument('--width', type=int, default=1280)
    parser.add_argument('--height', type=int, default=960)
    parser.add_argument('--preview', nargs='+', type=int, help='Only render these zero-based growth frames; do not save an animation or movie')
    parser.add_argument('--ffmpeg', default='ffmpeg')
    parser.add_argument('--title', default='Sakura')
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    if not 2 <= args.frames <= 600 or not 1 <= args.fps <= 60 or not 1 <= args.samples <= 4096 or not 1 <= args.threads <= 64:
        parser.error('Frame, fps, sample or thread count outside supported range')
    if any(not 128 <= v <= 4096 or v % 2 for v in [args.width, args.height]):
        parser.error('Use even image dimensions within 128..4096')
    if args.preview and any(not 0 <= i < args.frames for i in args.preview):
        parser.error('Preview index outside frame range')
    recipe, generator, source, out = args.recipe.resolve(), args.generator.resolve(), args.scene.resolve(), args.out.resolve()
    if any(not p.is_file() for p in [recipe, generator, source]):
        parser.error('Recipe, generator and source scene must exist')
    if source == out / 'growth-animation.blend':
        parser.error('Output must not replace the reference scene')
    if not args.preview:
        subprocess.run([args.ffmpeg, '-version'], check=True, stdout=subprocess.DEVNULL)
    out.mkdir(parents=True, exist_ok=True)
    (out / 'frames').mkdir(exist_ok=True)
    bpy.ops.wm.open_mainfile(filepath=str(source))
    scene = bpy.context.scene
    canonical = {m.name: m for m in bpy.data.materials if m.get('foliageutil') or m.name in ['cherry_bark', 'cherry_young']}
    # Retain all imported material slots before deleting the mature mesh, so this
    # can also preview other prepared plants without duplicating maps per frame.
    for obj in list(scene.objects):
        if obj.type == 'MESH' and obj.name != 'Presentation ground':
            for slot in obj.material_slots:
                if slot.material:
                    canonical[slot.material.name] = slot.material
            bpy.data.objects.remove(obj, do_unlink=True)
    for material in canonical.values():
        material.use_fake_user = True
    camera = scene.camera
    camera.data.ortho_scale = 16
    scene.render.resolution_x, scene.render.resolution_y = args.width, args.height
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGB'
    scene.render.engine = 'CYCLES'
    scene.cycles.samples, scene.cycles.seed, scene.cycles.device = args.samples, 42, 'CPU'
    scene.cycles.use_denoising = True
    scene.cycles.transparent_max_bounces = 64
    scene.render.threads_mode, scene.render.threads = 'FIXED', args.threads
    scene.render.fps = args.fps
    lead, tail = round(args.fps * .5), args.fps * 2
    scene.frame_start, scene.frame_end = 1, lead + args.frames + tail
    ink = bpy.data.materials.new('Growth labels')
    ink.use_nodes = True
    nodes, links = ink.node_tree.nodes, ink.node_tree.links
    nodes.clear()
    emission = nodes.new('ShaderNodeEmission')
    emission.inputs['Color'].default_value = (.86, .94, 1, 1)
    emission.inputs['Strength'].default_value = 2
    target = nodes.new('ShaderNodeOutputMaterial')
    links.new(emission.outputs[0], target.inputs['Surface'])
    half_height = camera.data.ortho_scale * args.height / args.width / 2
    overlay_text(camera, args.title, -6.85, half_height - 1.0, .58, ink)
    overlay_text(camera, 'PROCEDURAL GROWTH  /  SEED ' + str(json.loads(recipe.read_text()).get('seed', 0)), -6.8, half_height - 1.43, .21, ink)
    document = json.loads(recipe.read_text())
    developmental = document['growth'].get('mode') == 'developmental'
    if developmental:
        stages = document['growth']['stages']
        branch_starts = [stage.get('start', 0) for name, stage in stages.items() if document['nodes'][name]['op'] == 'branch']
        leaf_starts = [stage.get('start', 0) for name, stage in stages.items() if document['nodes'][name]['op'] == 'instance']
        tracks = [(0, 'TRUNK EXTENSION + THICKENING'), (min(branch_starts, default=0), 'BRANCH EMERGENCE + STRENGTH'), (min(leaf_starts, default=0), 'BABY SPRIGS + BLOOM')]
        for row, (start, label) in enumerate(tracks):
            obj = overlay_text(camera, label, -6.8, -half_height + 1.55 - row * .30, .20, ink)
            first = 1 if start == 0 else lead + 1 + math.ceil(start * (args.frames - 1))
            visibility(obj, first, scene.frame_end, scene.frame_end)
    else:
        phases = [(0, .40, 'TRUNK + ROOTS'), (.40, .66, 'BRANCHES'), (.66, .75, 'HANGING SHOOTS'), (.75, 1, 'BLOSSOMS'), (1, 1.1, 'FULL BLOOM')]
        for start, end, label in phases:
            obj = overlay_text(camera, label, -6.8, -half_height + 1.07, .29, ink)
            first = 1 if start == 0 else lead + 1 + math.ceil(start * (args.frames - 1))
            last = scene.frame_end if end > 1 else lead + math.ceil(end * (args.frames - 1))
            visibility(obj, first, last, scene.frame_end)
    bpy.ops.mesh.primitive_plane_add(size=1)
    bar = bpy.context.object
    bar.name = 'Growth progress'
    # Left-anchored bar in the camera's local XY plane.
    for vertex in bar.data.vertices:
        vertex.co.x += .5
    bar.rotation_euler = camera.rotation_euler
    bar.location = camera.location + camera.rotation_euler.to_quaternion() @ Vector((-6.8, -half_height + .69, -3))
    bar.data.materials.append(ink)
    bar.visible_shadow = bar.visible_diffuse = bar.visible_glossy = False
    for frame, factor in [(1, 0), (lead + 1, 0), (lead + args.frames, 1), (scene.frame_end, 1)]:
        bar.scale = (13.6 * factor, .035, 1)
        bar.keyframe_insert(data_path='scale', frame=frame)
    # Force the timeline's progress bar to match the native linear progress.
    for layer in bar.animation_data.action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for curve in bag.fcurves:
                    for point in curve.keyframe_points:
                        point.interpolation = 'LINEAR'
    for value, label in [(0, '0%'), (.25, '25%'), (.5, '50%'), (.75, '75%'), (1, '100%')]:
        overlay_text(camera, label, -6.8 + value * 13.3, -half_height + .27, .19, ink)
    metadata = {'version': 2, 'recipe': str(recipe), 'recipe_sha256': hashlib.sha256(recipe.read_bytes()).hexdigest(), 'source_scene': str(source), 'source_scene_sha256': hashlib.sha256(source.read_bytes()).hexdigest(), 'generator': str(generator), 'generator_sha256': hashlib.sha256(generator.read_bytes()).hexdigest(), 'renderer_script_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), 'growth': document['growth'], 'frames': args.frames, 'fps': args.fps, 'size': [args.width, args.height], 'samples': args.samples, 'renderer': bpy.app.version_string, 'device': 'CPU', 'title': args.title, 'camera_fixed': True, 'snapshots': []}
    cached = {}
    if args.resume:
        existing = json.loads((out / 'animation.json').read_text())
        for key in ['recipe_sha256', 'source_scene', 'source_scene_sha256', 'generator_sha256', 'renderer_script_sha256', 'frames', 'fps', 'size', 'samples', 'renderer']:
            if existing.get(key) != metadata[key]:
                raise ValueError('Cannot resume after changing ' + key)
        if existing.get('title', 'Sakura') != args.title:
            raise ValueError('Cannot resume after changing title')
        cached = {item['index']: item for item in existing['snapshots']}
    selected = args.preview if args.preview else range(args.frames)
    preview_objects = []
    for index in selected:
        start = time.monotonic()
        if args.preview:
            for obj in preview_objects:
                bpy.data.objects.remove(obj, do_unlink=True)
        progress, frame = index / (args.frames - 1), lead + index + 1
        generated = subprocess.run([str(generator), str(recipe), str(out / 'current'), args.output_name, format(progress, '.17g')], capture_output=True, text=True)
        if generated.returncode:
            raise RuntimeError(f'Growth frame {index} ({progress}): {generated.stderr}')
        report = json.loads(generated.stdout)
        imported = []
        if report.get('outputs'):
            before = set(scene.objects)
            bpy.ops.import_scene.gltf(filepath=report['outputs'][0]['path'])
            imported = list(set(scene.objects) - before)
            for obj in imported:
                obj.name = f'Growth {index:03d} / ' + obj.name
                if obj.type == 'MESH':
                    for slot in obj.material_slots:
                        base = re.sub(r'\.\d{3}$', '', slot.material.name)
                        if base not in canonical:
                            raise ValueError('Reference scene lacks prepared material ' + base)
                        slot.material = canonical[base]
                visibility(obj, frame, scene.frame_end if index == args.frames - 1 else frame, scene.frame_end)
        preview_objects = imported
        bpy.data.orphans_purge(do_recursive=True)
        scene.frame_set(frame)
        destination = out / 'frames' / f'{index:04d}.png'
        scene.render.filepath = str(destination)
        reuse = index in cached and destination.is_file() and destination.stat().st_size > 0
        if not reuse:
            bpy.ops.render.render(write_still=True)
        metadata['snapshots'].append({'index': index, 'frame': frame, 'progress': progress, 'render': str(destination), 'outputs': report.get('outputs', []), 'elapsed_seconds': round(time.monotonic() - start, 3), 'reused_render': reuse})
        (out / 'animation.json').write_text(json.dumps(metadata, indent=2) + '\n')
        print(f'GROWTH_FRAME {index + 1}/{args.frames}: {progress:.4f} in {time.monotonic() - start:.1f}s', flush=True)
    if args.preview:
        return
    scene.frame_set(scene.frame_end)
    bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=str(out / 'growth-animation.blend'), compress=True)
    movie = out / 'sakura-growth.mp4'
    subprocess.run([args.ffmpeg, '-y', '-framerate', str(args.fps), '-i', str(out / 'frames/%04d.png'), '-vf', 'tpad=start_mode=clone:start_duration=0.5:stop_mode=clone:stop_duration=2', '-c:v', 'libx264', '-crf', '18', '-pix_fmt', 'yuv420p', '-movflags', '+faststart', str(movie)], check=True)
    metadata['movie'], metadata['blend'] = str(movie), str(out / 'growth-animation.blend')
    (out / 'animation.json').write_text(json.dumps(metadata, indent=2) + '\n')
    print('Rendered growth animation: ' + str(movie), flush=True)


if __name__ == '__main__':
    main()
