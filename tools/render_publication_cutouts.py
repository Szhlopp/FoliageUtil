#!/usr/bin/env python3
"""Render transparent publication layers from saved, verified Blender scenes."""
import argparse
import json
import sys
from pathlib import Path

import bpy


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--only', nargs='*')
    parser.add_argument('--samples', type=int, default=48)
    parser.add_argument('--threads', type=int, default=6)
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    if not 1 <= args.samples <= 4096 or not 1 <= args.threads <= 64:
        parser.error('Samples must be 1..4096 and threads 1..64')
    manifest = args.manifest.resolve()
    jobs = json.loads(manifest.read_text())['layers']
    if args.only and set(args.only) - jobs.keys():
        parser.error('Unknown layer name')
    for name, job in jobs.items():
        if args.only and name not in args.only:
            continue
        source = (manifest.parent / job['scene']).resolve()
        output = (manifest.parent / job['output']).resolve()
        size = job.get('size', [1100, 1100])
        if not source.is_file() or source.suffix.lower() != '.blend':
            parser.error('Missing Blender scene for ' + name)
        if output.suffix.lower() != '.png' or len(size) != 2 or any(type(v) is not int or not 64 <= v <= 4096 for v in size):
            parser.error('Use PNG output and image dimensions within 64..4096 for ' + name)
    for name, job in jobs.items():
        if args.only and name not in args.only:
            continue
        source = (manifest.parent / job['scene']).resolve()
        output = (manifest.parent / job['output']).resolve()
        bpy.ops.wm.open_mainfile(filepath=str(source))
        scene = bpy.context.scene
        hidden = []
        for obj in scene.objects:
            if obj.type == 'FONT' or obj.name == 'Presentation ground':
                obj.hide_render = True
                hidden.append(obj.name)
        scene.render.film_transparent = True
        scene.render.image_settings.file_format = 'PNG'
        scene.render.image_settings.color_mode = 'RGBA'
        scene.render.resolution_x, scene.render.resolution_y = job.get('size', [1100, 1100])
        scene.render.resolution_percentage = 100
        scene.cycles.samples, scene.cycles.seed = args.samples, 42
        scene.cycles.transparent_max_bounces = 64
        scene.cycles.use_denoising = True
        scene.cycles.device = 'CPU'
        scene.render.threads_mode, scene.render.threads = 'FIXED', args.threads
        output.parent.mkdir(parents=True, exist_ok=True)
        scene.render.filepath = str(output)
        bpy.ops.render.render(write_still=True)
        output.with_suffix('.json').write_text(json.dumps({'source': str(source), 'hidden_presentation_objects': hidden, 'renderer': bpy.app.version_string, 'samples': args.samples, 'size': [scene.render.resolution_x, scene.render.resolution_y], 'output': str(output), 'note': 'Source camera, meshes and materials retained. Only camera background, ground and labels hidden.'}, indent=2) + '\n')
        print('Rendered publication layer: ' + name, flush=True)


if __name__ == '__main__':
    main()
