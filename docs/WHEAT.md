# Golden wheat

Prepare sample assets first with `python3 tools/prepare_samples.py --texutil
../TexUtil/build/texutil`. Source recipes are under `samples/`; executable textured
graphs are under `out/samples/graphs/`. Generated maps and bakes stay under `out/`.
See [sample setup](../samples/README.md).

[samples/graphs/wheat.json](../samples/graphs/wheat.json) builds an awned wheat patch with
three seeded stalk shapes, textured straw, narrow curved blades and detailed ears.
Each ear follows the actual bent stalk tip. The older recipe positioned ears at
fixed world coordinates, which could separate them from curved stems.

The example approximates alternating spikelets along an ear, with three floret
forms per group. This follows the general arrangement described in the
[University of Wisconsin wheat botany reference](https://corn.aae.wisc.edu/Crops/Wheat/L001.aspx).
It is a procedural visual study, not a cultivar model or developmental simulation.

## Structure and controls

The seed-42 patch has **36 stalks** assembled from three separately generated
prototypes in groups of 14, 12 and 10. Scatter controls position, yaw and scale;
orientation adds a small lean, and instance controls vary dimensions and tint.
Copies within a group share that group's prototype. Changing the root seed changes
stalk noise, placements and variations. The material-generation seed is separate.

The full patch is about **1.30 meters** high and contains **435,960 triangles** and
**308,484 vertices**. Each head has 22 alternating spikelet groups, each composed
of three grain forms with pointed front/back husks and tapered tube awns. Rachis,
husks and awns use geometry; fine longitudinal fibers and ribs use TexUtil maps.
The full ear, including awns, is about 14 cm long before per-plant scaling.

| Controls | Effect |
| --- | --- |
| `a_stalk`, `b_stalk`, `c_stalk` | Stem length, radius, bend and seeded path noise. |
| `a_tip`, `b_tip`, `c_tip` | Tip placement follows each stem's endpoint and tangent. |
| `spikelet_sites` | Number, alternating angle, vertical range, tilt and size progression of the grain groups. |
| `kernel_shape`, `husk_shape` | Grain volume and pointed enclosing surfaces. |
| `awn_path` | Awn length, curvature, thickness and taper. |
| `blade_shape` and each `*_blade_sites` | Long narrow foliage, curvature, attachment and size. |
| Each `*_bases`, `*_lean`, `*_patch` | Patch density, footprint, leaning and seeded variation. |

This is a detailed demonstration. For a cheaper mesh, reduce floret tessellation,
head/patch counts, blade subdivisions or tube sides. Awns are real thin geometry
and can alias or disappear at distance; an atlas/impostor LOD would be a separate
asset. Wind remains exported metadata rather than an animated simulation.

## Materials and exports

The portable [TexUtil source](../samples/materials/wheat/wheat.texutil.json) generates husk,
straw and blade base colors, shared fine normal relief and packed roughness. Color
is sRGB, normals are 16-bit OpenGL data, and roughness uses G with metallic zero in
B. The maps are 1024 square and tile. Blades use saved `translucency:0.12` through
the existing Blender material adapter. Core PBR works in ordinary GLB viewers;
tissue optics and `_WIND` need supporting adapters/shaders.

```sh
../TexUtil/build/texutil samples/materials/wheat/wheat.texutil.json --out out/wheat-materials --threads 8 --json
./build/foliageutil out/samples/graphs/wheat.json --out out/wheat --seed 42 --json
```

The example reads prepared maps under `out/samples/assets/wheat/`, so TexUtil is
needed only for regeneration. Rerun preparation after edits and inspect the updated
maps before exporting the wheat again.

| Output | Contents | Triangles |
| --- | --- | --- |
| `wheat.glb`, `wheat.obj` | Full 36-stalk patch | 435,960 |
| `wheat-stalk.glb` | One complete stem with blades and attached ear | 12,110 |
| `wheat-head.glb` | Isolated ear at the origin for close inspection | 11,144 |

GLB embeds textures, materials and UVs. OBJ/MTL has the previously documented
losses in material properties and wind data. These repeated UVs are not a unique
whole-model bake atlas.

## Actual Blender renders

```sh
blender --background --factory-startup --python-exit-code 1 --python tools/render_blender.py -- --model out/wheat/wheat.glb --out out/wheat/wheat.png --hdr ../TexUtil/assets/hdri/outdoor.hdr --samples 96 --size 1400 --elevation 9 --azimuth 20 --light-scale 1.5
blender --background --factory-startup --python-exit-code 1 --python tools/render_blender.py -- --model out/wheat/wheat-head.glb --out out/wheat/wheat-head.png --hdr ../TexUtil/assets/hdri/studio.hdr --samples 96 --size 1000 --elevation 7 --azimuth 30 --light-scale 0.18
```

On this Mac, use `/Applications/Blender.app/Contents/MacOS/Blender`. The isolated
process imports exported geometry and writes PNG, packed Blender scene and JSON
render settings. These views use Cycles CPU, eight workers, seed 42, denoising and
AgX at exposure zero. No shape modifiers or retouching are applied.

The Release suite passes 19/19 tests. All three wheat GLBs pass Khronos validation
with zero errors; there are three runtime tangent-generation warnings on the full
patch and stalk, and two on the isolated head. Seed 42 produces a byte-identical
GLB on repeat; seed 43 changes the export. A Filament solid-material preview also
checks the patch geometry. The helper now prefers the GLB matching the recipe
name, so additional detail outputs do not replace the patch in the gallery.
