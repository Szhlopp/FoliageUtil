#!/usr/bin/env python3
"""Render an exported GLB in a separate Blender background process using Cycles."""
import argparse
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector


def point_at(obj, target):
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat('-Z', 'Y').to_euler()


def apply_foliage_materials(materials=None):
    applied = {}
    for material in bpy.data.materials if materials is None else materials:
        metadata = material.get('foliageutil')
        if not metadata or metadata.get('version') != 1 or not material.use_nodes:
            continue
        settings = metadata.to_dict()
        nodes, links = material.node_tree.nodes, material.node_tree.links
        shader = next((n for n in nodes if n.type == 'BSDF_PRINCIPLED'), None)
        if shader is None:
            raise ValueError(f'No Principled shader for foliage material {material.name}')
        weight = settings.get('subsurface', 0)
        if weight > 0:
            shader.subsurface_method = 'BURLEY'
            shader.inputs['Subsurface Weight'].default_value = weight
            shader.inputs['Subsurface Radius'].default_value = settings.get('subsurface_radius', [1, .45, .25])
            shader.inputs['Subsurface Scale'].default_value = settings.get('subsurface_scale', .0015)
        transmission = settings.get('translucency', 0)
        if transmission > 0:
            # Mix diffuse transmission into the tissue, then restore the importer's
            # alpha coverage outside the mix so cutout holes stay transparent.
            output = next(n for n in nodes if n.type == 'OUTPUT_MATERIAL' and n.is_active_output)
            destinations = [link.to_socket for link in shader.outputs['BSDF'].links]
            if output.inputs['Surface'] not in destinations:
                raise ValueError(f'Unexpected surface graph for foliage material {material.name}')
            tint = nodes.new('ShaderNodeMixRGB')
            tint.name = 'Foliage transmission pigment'
            tint.blend_type = 'MULTIPLY'
            tint.inputs[0].default_value = 1
            tint.inputs[2].default_value = (*settings.get('translucency_color', [1, 1, 1]), 1)
            color = shader.inputs['Base Color']
            if color.is_linked:
                links.new(color.links[0].from_socket, tint.inputs[1])
            else:
                tint.inputs[1].default_value = color.default_value
            translucent = nodes.new('ShaderNodeBsdfTranslucent')
            translucent.name = 'Foliage thin tissue'
            links.new(tint.outputs[0], translucent.inputs['Color'])
            if shader.inputs['Normal'].is_linked:
                links.new(shader.inputs['Normal'].links[0].from_socket, translucent.inputs['Normal'])
            tissue = nodes.new('ShaderNodeMixShader')
            tissue.name = 'Foliage translucency'
            tissue.inputs[0].default_value = transmission
            links.new(shader.outputs['BSDF'], tissue.inputs[1])
            links.new(translucent.outputs[0], tissue.inputs[2])
            coverage = nodes.new('ShaderNodeMixShader')
            coverage.name = 'Foliage alpha coverage'
            alpha = shader.inputs['Alpha']
            coverage.inputs[0].default_value = alpha.default_value
            if alpha.is_linked:
                source = alpha.links[0].from_socket
                links.remove(alpha.links[0])
                links.new(source, coverage.inputs[0])
            alpha.default_value = 1
            transparent = nodes.new('ShaderNodeBsdfTransparent')
            links.new(transparent.outputs[0], coverage.inputs[1])
            links.new(tissue.outputs[0], coverage.inputs[2])
            for destination in destinations:
                links.new(coverage.outputs[0], destination)
        applied[material.name] = settings
    return applied


def apply_petal_subsurface(weight, scale):
    materials = []
    for material in bpy.data.materials:
        if not material.name.startswith('petal') or not material.use_nodes:
            continue
        for shader in material.node_tree.nodes:
            if shader.type != 'BSDF_PRINCIPLED':
                continue
            # Burley is a surface diffusion approximation for these open petals.
            # Keep the imported base-color, roughness and normal texture links.
            shader.subsurface_method = 'BURLEY'
            shader.inputs['Subsurface Weight'].default_value = weight
            shader.inputs['Subsurface Radius'].default_value = (1.0, .45, .25)
            shader.inputs['Subsurface Scale'].default_value = scale
            materials.append(material.name)
    if not materials:
        raise ValueError('No Principled petal materials for --petal-subsurface')
    return materials


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--model', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--hdr', type=Path)
    parser.add_argument('--transparent-bounces', type=int, default=64, help='Cycles pass-through limit for layered alpha cards')
    parser.add_argument('--samples', type=int, default=64)
    parser.add_argument('--size', type=int, default=900)
    parser.add_argument('--threads', type=int, default=8)
    parser.add_argument('--focus', choices=['full', 'bloom'], default='full')
    parser.add_argument('--lighting', choices=['studio', 'backlit'], default='studio', help='Backlit isolates thin-tissue transmission with a weak front fill')
    parser.add_argument('--light-scale', type=float, default=1, help='Scale the light rig for larger assets; preserves illuminance by scaling power quadratically')
    parser.add_argument('--exposure', type=float, default=0, help='Camera exposure in stops, shared by the full and close-up renders')
    parser.add_argument('--elevation', type=float, default=22)
    parser.add_argument('--azimuth', type=float, default=20)
    parser.add_argument('--ortho-scale', type=float)
    parser.add_argument('--target', type=float, nargs=3)
    parser.add_argument('--ground-height', type=float, help='Optional flat presentation ground at this source Y height in meters; separate from the imported foliage')
    parser.add_argument('--petal-subsurface', type=float, default=None, help='Optional override of saved petal subsurface weight, 0..1 (default: use recipe)')
    parser.add_argument('--petal-subsurface-scale', type=float, default=.0015, help='Petal scattering scale in meters (default: 0.0015)')
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
    if not 1 <= args.transparent_bounces <= 256:
        parser.error('Transparent bounces must be within 1..256')
    if not (1 <= args.samples <= 4096 and 64 <= args.size <= 4096 and 1 <= args.threads <= 64):
        parser.error('Invalid sample, image-size or thread limit')
    if not math.isfinite(args.light_scale) or not .001 <= args.light_scale <= 1000:
        parser.error('Light scale must be within 0.001..1000')
    if args.ground_height is not None and (not math.isfinite(args.ground_height) or not -100000 <= args.ground_height <= 100000):
        parser.error('Ground height must be finite and within -100000..100000 meters')
    if not ((args.petal_subsurface is None or (math.isfinite(args.petal_subsurface) and 0 <= args.petal_subsurface <= 1)) and math.isfinite(args.petal_subsurface_scale) and 0 < args.petal_subsurface_scale <= 1):
        parser.error('Subsurface weight must be 0..1 and scattering scale must be greater than 0 and at most 1 meter')
    if not math.isfinite(args.exposure) or not -10 <= args.exposure <= 10:
        parser.error('Exposure must be within -10..10 stops')
    model, output = args.model.resolve(), args.out.resolve()
    if not model.is_file():
        raise FileNotFoundError(model)
    if not output.suffix.lower() == '.png':
        parser.error('--out must be a PNG filename')
    output.parent.mkdir(parents=True, exist_ok=True)
    hdr = args.hdr.resolve() if args.hdr else Path(bpy.utils.system_resource('DATAFILES')) / 'studiolights/world/studio.exr'
    if not hdr.is_file():
        raise FileNotFoundError(f'Provide --hdr for an HDR environment: {hdr}')
    # This script is intentionally run with --background --factory-startup.
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    bpy.ops.import_scene.gltf(filepath=str(model))
    foliage_materials = apply_foliage_materials()
    subsurface_materials = apply_petal_subsurface(args.petal_subsurface, args.petal_subsurface_scale) if args.petal_subsurface is not None else []
    objects = [obj for obj in bpy.context.scene.objects if obj.type == 'MESH']
    vertices = []
    for obj in objects:
        if args.focus == 'bloom':
            indices = set()
            for face in obj.data.polygons:
                material = obj.material_slots[face.material_index].material
                if material and material.name.startswith('petal'):
                    indices.update(face.vertices)
            vertices.extend(obj.matrix_world @ obj.data.vertices[i].co for i in indices)
        else:
            vertices.extend(obj.matrix_world @ v.co for v in obj.data.vertices)
    if not vertices:
        raise ValueError('No geometry for the requested focus')
    lo = Vector([min(v[i] for v in vertices) for i in range(3)])
    hi = Vector([max(v[i] for v in vertices) for i in range(3)])
    target = Vector(args.target) if args.target else (lo + hi) * .5
    scene = bpy.context.scene
    scene.world = bpy.data.worlds.new('HDR studio with neutral camera background')
    scene.world.use_nodes = True
    nodes, links = scene.world.node_tree.nodes, scene.world.node_tree.links
    nodes.clear()
    environment = nodes.new('ShaderNodeTexEnvironment')
    environment.image = bpy.data.images.load(str(hdr), check_existing=True)
    illumination = nodes.new('ShaderNodeBackground')
    illumination.inputs['Strength'].default_value = .4 if args.lighting == 'studio' else .015
    links.new(environment.outputs['Color'], illumination.inputs['Color'])
    background = nodes.new('ShaderNodeBackground')
    background.inputs['Color'].default_value = (.014, .021, .025, 1)
    ray = nodes.new('ShaderNodeLightPath')
    mix = nodes.new('ShaderNodeMixShader')
    links.new(ray.outputs['Is Camera Ray'], mix.inputs[0])
    links.new(illumination.outputs[0], mix.inputs[1])
    links.new(background.outputs[0], mix.inputs[2])
    world_output = nodes.new('ShaderNodeOutputWorld')
    links.new(mix.outputs[0], world_output.inputs['Surface'])
    lights = [('Key softbox', (-.65, -1.1, 1.55), 70, 1.0), ('Rim softbox', (.7, .45, 1.15), 85, .8), ('Fill softbox', (.8, -.6, .65), 15, 1.0)] if args.lighting == 'studio' else [('Backlight', (0, .35, .85), 25, .4), ('Weak front fill', (0, -.7, .85), .4, .8)]
    for name, position, energy, size in lights:
        data = bpy.data.lights.new(name, 'AREA')
        data.energy, data.shape, data.size = energy * args.light_scale ** 2, 'DISK', size * args.light_scale
        obj = bpy.data.objects.new(name, data)
        bpy.context.collection.objects.link(obj)
        obj.location = Vector(position) * args.light_scale
        point_at(obj, (0, 0, .65 * args.light_scale))
    bpy.ops.object.camera_add()
    camera = bpy.context.object
    scene.camera = camera
    a, e = math.radians(args.azimuth), math.radians(args.elevation)
    distance = max(2, (hi - lo).length * 2, (target - (lo + hi) * .5).length * 2)
    camera.location = target + Vector((math.sin(a) * math.cos(e), -math.cos(a) * math.cos(e), math.sin(e))) * distance
    point_at(camera, target)
    camera.data.type = 'ORTHO'
    camera.data.clip_start = .001
    camera.data.clip_end = max(100, distance * 4)
    inverse = camera.rotation_euler.to_matrix().transposed()
    projected = [inverse @ (v - target) for v in vertices]
    width = max(v.x for v in projected) - min(v.x for v in projected)
    height = max(v.y for v in projected) - min(v.y for v in projected)
    camera.data.ortho_scale = args.ortho_scale or 1.18 * max(width, height)
    if args.ground_height is not None:
        bpy.ops.mesh.primitive_plane_add(size=max(2, (hi - lo).length * 40), location=(target.x, target.y, args.ground_height))
        ground = bpy.context.object
        ground.name = 'Presentation ground'
        material = bpy.data.materials.new('Matte ground')
        material.use_nodes = True
        shader = next(n for n in material.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
        shader.inputs['Base Color'].default_value = (.10, .085, .06, 1)
        shader.inputs['Roughness'].default_value = .95
        ground.data.materials.append(material)
    scene.render.engine = 'CYCLES'
    scene.cycles.device = 'CPU'
    scene.cycles.samples = args.samples
    scene.cycles.seed = 42
    scene.cycles.use_denoising = True
    scene.cycles.transparent_max_bounces = args.transparent_bounces
    scene.cycles.max_bounces = 10
    scene.render.threads_mode = 'FIXED'
    scene.render.threads = args.threads
    scene.render.resolution_x = scene.render.resolution_y = args.size
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGBA'
    scene.render.filepath = str(output)
    scene.view_settings.view_transform = 'AgX'
    scene.view_settings.exposure = args.exposure
    # The GLB importer supplies PBR textures and coverage. The adapter above
    # applies saved FoliageUtil optics; both are recorded in the manifest.
    bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=str(output.with_suffix('.blend')))
    bpy.ops.render.render(write_still=True)
    manifest = {'blender': bpy.app.version_string, 'engine': 'Cycles', 'device': 'CPU', 'samples': args.samples, 'transparent_bounces': args.transparent_bounces, 'size': args.size, 'threads': args.threads, 'seed': 42, 'model': str(model), 'hdr': str(hdr), 'view_transform': 'AgX', 'exposure': args.exposure, 'focus': args.focus, 'target': list(target), 'ortho_scale': camera.data.ortho_scale, 'elevation': args.elevation, 'azimuth': args.azimuth, 'mesh_objects': len(objects), 'triangles': sum(len(o.data.loop_triangles) for o in objects), 'materials': [m.name for m in bpy.data.materials], 'images': [{'name': i.name, 'color_space': i.colorspace_settings.name, 'packed': bool(i.packed_file)} for i in bpy.data.images if i.type == 'IMAGE'], 'output': str(output)}
    manifest['petal_subsurface'] = {'weight': args.petal_subsurface, 'scale_meters': args.petal_subsurface_scale, 'radius_rgb': [1.0, .45, .25], 'method': 'BURLEY', 'materials': subsurface_materials}
    manifest['foliage_materials'] = foliage_materials
    manifest['lighting'] = args.lighting
    manifest['light_scale'] = args.light_scale
    manifest['ground_height'] = args.ground_height
    output.with_suffix('.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('Rendered ' + str(output), flush=True)


if __name__ == '__main__':
    main()
