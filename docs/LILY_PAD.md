# Lily pads

Prepare sample assets first with `python3 tools/prepare_samples.py --texutil
../TexUtil/build/texutil`. Source recipes are under `samples/`; executable textured
graphs are under `out/samples/graphs/`. Generated maps and bakes stay under `out/`.
See [sample setup](../samples/README.md).

[samples/graphs/lily-pad.json](../samples/graphs/lily-pad.json) creates a single lily pad and
a seven-pad cluster using curved cards, a TexUtil stencil atlas, placement frames
and tube petioles. It adds a floating-leaf example without a species-specific node.

## Geometry, stencil and material

The pad starts as a 0.40-meter square card with a center pivot and 32 by 32
subdivisions. Gentle curl, fold and twist cup the surface. Its visible stencil is
about 36 centimeters across before instance variation. A local -90 degree X
rotation turns its face upward before placement. Petioles extend downward from
the pad centers, with their upper ends meeting the surface.

The round outline, slightly irregular rim and narrow notch are **alpha coverage**.
The underlying mesh remains a curved rectangle. A renderer must honor the GLB
`MASK` material to show the pad silhouette. Collision meshes, solid print geometry
or viewers that ignore alpha will still see the rectangular card.

Fourteen gently warped radial veins and a finer cellular network are TexUtil map
detail. The network is an artistic approximation, not a simulated vascular system.
Pigment variation, a restrained rim tint, low roughness and saved
`translucency:0.08` complete the surface. The underside uses the same material;
this is a thin double-sided surface, not a thick leaf with separate underside maps.

| Controls | Effect |
| --- | --- |
| `pad_card` | Width, height, pivot, curl, fold, twist and tessellation. |
| `outer_sites`, `center_site` | Cluster spacing, count, size progression and height. |
| `pad_sites` | Seeded yaw while keeping the pads horizontal. |
| `upright_cards` | Atlas selection, independent size jitter and tint. |
| `petiole_path` | Underwater stem length, bend, radius and taper. |
| TexUtil `disk`, `notch_depth`, `notch_width` | Outline proportions, notch depth and opening. |
| TexUtil `silhouette`, `rays_strip`, `rays_bent`, `fine_veins` | Rim irregularity and coarse/fine vein detail. |
| Material `pad` | Color multiplier, roughness, alpha cutoff and tissue translucency. |

The cluster uses a central pad and six surrounding placements with seeded yaw,
size and atlas choices. Spacing avoids intersections in the checked seed-42
render. These nodes do not solve collisions; denser layouts can overlap. The
texture-generation seed is separate from the foliage seed.

## Atlas and exports

[pads.texutil.json](../samples/materials/lily-pad/pads.texutil.json) makes six variants from
[pad.texutil.json](../samples/materials/lily-pad/pad.texutil.json). The 1536 by 1024 atlas has
3 by 2 cells of 512 pixels, including four pixels of internal padding on each
side. Its manifest controls the shared UV placement for color, normal and packed
roughness. The color image includes actual alpha; normals use 16-bit OpenGL data;
packed roughness is G with metallic zero in B.

```sh
../TexUtil/build/texutil samples/materials/lily-pad/pads.texutil.json --out out/lily-pad-materials --threads 8 --json
./build/foliageutil out/samples/graphs/lily-pad.json --out out/lily-pad --seed 42 --json
```

The example reads the prepared atlas, so mesh generation does not require TexUtil.
After recipe edits, rerun sample preparation and inspect the updated manifest
and complete `.assets` directory under `out/samples/assets/lily-pad/`.

| Output | Contents | Triangles |
| --- | --- | --- |
| `lily-pad.glb`, `lily-pad.obj` | Seven pads with submerged petioles | 14,896 |
| `lily-pad-single.glb` | One pad and petiole | 2,128 |
| `lily-pad-surface.glb` | One isolated pad surface | 2,048 |
| `lily-pad-surfaces.glb` | Seven surfaces for viewing above water | 14,336 |

Surface-only outputs support close inspection or use with a separate water scene.
These exports do not include water, flowers or an animated buoyancy simulation.
The stencil UVs repeat between copies. For a cheaper card, reduce both subdivision
counts while checking the resulting curl and shading. GLB preserves alpha and
saved optics more completely than OBJ/MTL.

## Render and verification

```sh
blender --background --factory-startup --python-exit-code 1 --python tools/render_blender.py -- --model out/lily-pad/lily-pad-surfaces.glb --out out/lily-pad/lily-pad.png --hdr ../TexUtil/assets/hdri/outdoor.hdr --samples 96 --size 1400 --elevation 50 --azimuth 12 --light-scale 1.1
blender --background --factory-startup --python-exit-code 1 --python tools/render_blender.py -- --model out/lily-pad/lily-pad-surface.glb --out out/lily-pad/lily-pad-closeup.png --hdr ../TexUtil/assets/hdri/outdoor.hdr --samples 96 --size 1100 --elevation 38 --azimuth 15 --light-scale .4
```

On this Mac, use `/Applications/Blender.app/Contents/MacOS/Blender`. The renderer
imports the actual exports and applies their saved optics. The images use Blender
5.2.1 LTS, Cycles CPU, eight workers, seed 42, denoising and AgX at exposure zero.
No geometry modifiers or image retouching are applied.

All four GLBs pass Khronos validation with zero errors and one runtime
tangent-generation warning each. The exported pad material was checked for
double-sided `MASK` coverage, the embedded RGBA atlas and saved translucency.
Seed 42 repeats byte-for-byte on this build; seed 43 changes the patch. The
Filament helper checks the curved cards and stems using solid materials, so
Blender provides the actual stencil and appearance verification.
