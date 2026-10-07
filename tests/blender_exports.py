#!/usr/bin/env python3
"""Optional Blender roundtrip: compare mature Alembic triangles and corner attributes with a real GLB."""
import argparse
import json
import struct
import sys
from pathlib import Path

import bpy
from mathutils import Vector

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from import_growth_alembic import import_package


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--reference', type=Path, required=True)
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    raw = args.reference.read_bytes()
    size = struct.unpack_from('<I', raw, 12)[0]
    doc = json.loads(raw[20:20 + size])
    binary = memoryview(raw)[28 + size:]

    def accessor(index):
        item = doc['accessors'][index]
        view = doc['bufferViews'][item['bufferView']]
        count = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}[item['type']]
        form = '<' + ('I' if item['componentType'] == 5125 else 'f') * count
        stride = view.get('byteStride', struct.calcsize(form))
        offset = view.get('byteOffset', 0) + item.get('byteOffset', 0)
        return [struct.unpack_from(form, binary, offset + i * stride) for i in range(item['count'])]

    bpy.ops.wm.read_factory_settings(use_empty=True)
    objects, manifest = import_package(args.package)
    scene = bpy.context.scene
    for frame in [0, 1, manifest['frames'] // 2, manifest['frames'] - 1, 0, manifest['frames'] - 1]:
        scene.frame_set(frame)
        deps = bpy.context.evaluated_depsgraph_get()
        count = sum(len(obj.evaluated_get(deps).data.polygons) for obj in objects if not obj.hide_render)
        assert count == manifest['snapshots'][frame]['mesh']['triangles'], (frame, count)
    lookup = {obj.name: obj.evaluated_get(deps).data for obj in objects}
    corners = 0
    max_normal_error = 0.0
    for primitive in doc['meshes'][0]['primitives']:
        name = doc['materials'][primitive['material']]['name']
        mesh = lookup[name]
        indices = accessor(primitive['indices'])
        positions = accessor(primitive['attributes']['POSITION'])
        normals = accessor(primitive['attributes']['NORMAL'])
        uvs = accessor(primitive['attributes']['TEXCOORD_0'])
        colors = accessor(primitive['attributes']['COLOR_0'])
        assert len(mesh.loops) == len(indices)
        for loop, (source,) in zip(mesh.loops, indices):
            x, y, z = positions[source]
            assert (mesh.vertices[loop.vertex_index].co - Vector((x, -z, y))).length < 1e-5, ('position', name, loop.index)
            x, y, z = normals[source]
            max_normal_error = max(max_normal_error, (mesh.corner_normals[loop.index].vector - Vector((x, -z, y))).length)
            u, v = uvs[source]
            actual = mesh.uv_layers['UVMap'].data[loop.index].uv
            assert abs(actual.x - u) < 1e-5 and abs(actual.y - (1 - v)) < 1e-5, ('UV', name, loop.index, tuple(actual), (u, 1-v))
            # Blender's Alembic reader stores byte colors without converting linear
            # values to sRGB. The material adapter compensates this interpretation.
            color = mesh.color_attributes['Color'].data[loop.index].color_srgb
            assert max(abs(a-b) for a,b in zip(color, colors[source])) < .005, ('color', name, loop.index)
            corners += 1
    # Blender compresses custom normals internally; require less than one degree.
    assert max_normal_error < .01745, max_normal_error
    print(json.dumps({'verified_corners': corners, 'frames': manifest['frames'], 'reverse_empty_scrub': True, 'max_normal_difference': max_normal_error}))


if __name__ == '__main__':
    main()
