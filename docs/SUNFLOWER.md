# Sunflower and facing-surface scatter

Prepare sample assets first with `python3 tools/prepare_samples.py --texutil
../TexUtil/build/texutil`. Source recipes are under `samples/`; executable textured
graphs are under `out/samples/graphs/`. Generated maps and bakes stay under `out/`.
See [sample setup](../samples/README.md).

[The sunflower recipe](../samples/graphs/sunflower.json) composes generic nodes: a stem,
petioles, leaves, a flattened ellipsoid head, seed instances, and two petal rings.
It includes LOD and growth configurations. It does not introduce a species node.

```json
"seed_sites": {
  "op": "scatter",
  "input": "head",
  "count": 6500,
  "normal_direction": [0, 1, 0],
  "max_surface_angle": 84,
  "angle": 0,
  "offset": -0.001,
  "scale": [0.7, 1.15]
}
```

The seed head is authored with local +Y facing forward, then the completed flower
is attached and tilted on its stem. A candidate passes the surface filter when
`dot(normalize(surface_normal), normalize(normal_direction)) >= cos(max_surface_angle)`
within float tolerance. The normal is interpolated from triangle vertex normals.
It is evaluated in current graph coordinates, before scatter offsets, orientation
tilt and optional proximity snapping. A normal cone selects orientation, not
visibility, occlusion or a specific connected patch. Reversed input normals reverse
what counts as facing. `normal_direction` requires mesh input and must be nonzero.

`max_surface_angle:90` selects a facing hemisphere, 0 requires aligned normals,
and 180 accepts all. The direction's magnitude has no effect. `angle:0` independently
orients prototype +Y along each accepted surface normal. `orient` aims prototype
+Z and serves a different purpose. `count` is an attempt limit with no retries;
the seed-42 example accepts 3,307 of 6,500 candidates. The node report includes
`normal_filter` counts. When combined with proximity, its counts start after this
normal filter. Default unfiltered graphs keep their original seeded stream.

This example uses area-random seeds with overlaps, not collision-aware Fibonacci
packing. `radial.mode:"spiral"` provides a separate regular spiral construction
for authored seed patterns. The textured example demonstrates surface selection,
curved tissue and reproducible asset generation, not a photographic reconstruction.

## Petals and material maps

The reference's long, cupped petals need both geometry and maps. The recipe uses
`leaf.width_profile` for a narrow neck, a fuller middle and a tapered tip;
`fold`, `curl`, `twist`, `lateral_bend`, `edge_wave` and `edge_frequency` shape the
surface. Point rotation jitter and per-instance size jitter vary the two rings.
Width profile endpoints are zero, interior keys positive, and interpolation is
linear. Enough densely spaced keys give a smooth outline; keep enough segments
for the intended bends, ripples and LOD silhouettes. Deformation retains UVs.

Six TexUtil petal variants supply aligned yellow pigment, OpenGL normal and
packed roughness maps. Fine longitudinal veins belong in those maps, while
cupping and edge ripples are geometry. Dedicated leaf maps add a midrib, diagonal
veins and pigment variation; stem maps add vertical fibers and subtle pores.
See [the material package](../samples/materials/sunflower/README.md) for native source graphs.
Alpha is opaque on these geometry petals; translucency is saved separately and
applied by the Blender adapter.

```sh
./build/foliageutil out/samples/graphs/sunflower.json --out out/sunflower --json
blender --background --factory-startup --python-exit-code 1 --python tools/render_blender.py -- --model out/sunflower/sunflower.glb --out out/sunflower/final.png --hdr ../TexUtil/assets/hdri/outdoor.hdr --samples 96 --size 1200 --light-scale 1.6 --elevation 9 --azimuth 10 --ground-height 0 --exposure -1.7
blender --background --factory-startup --python-exit-code 1 --python tools/render_blender.py -- --model out/sunflower/sunflower.glb --out out/sunflower/final-closeup.png --hdr ../TexUtil/assets/hdri/outdoor.hdr --samples 96 --size 1200 --light-scale 1.6 --elevation 15 --azimuth 15 --focus bloom --exposure -1.7
```

Use the installed Blender executable. Development renders use Blender 5.2.1 LTS,
Cycles CPU, AgX and the same -1.7-stop exposure in both views. Packed scenes and
JSON render manifests stay beside the output PNGs. The ground is a separate
presentation plane. No geometry or UV repair is performed by the renderer.
