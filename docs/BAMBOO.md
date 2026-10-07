# Green bamboo

Prepare sample assets first with `python3 tools/prepare_samples.py --texutil
../TexUtil/build/texutil`. Source recipes are under `samples/`; executable textured
graphs are under `out/samples/graphs/`. Generated maps and bakes stay under `out/`.
See [sample setup](../samples/README.md).

[samples/graphs/bamboo.json](../samples/graphs/bamboo.json) builds a seven-culm grove from
three seeded stem prototypes. Raised pale joints, slender branch sprays and long
leaves use the existing path, tube, branch, leaf and instance nodes.

## Shape and variation

The three prototypes are 3.5, 4.2 and 4.9 meters long before placement variation.
They are repeated in groups of two, three and two. Seed 42 produces a grove about
5.04 meters high, with 2,240 leaves, 548,184 triangles and 365,673 vertices.
Copies of a prototype share its branch shape; three independently named paths
give the grove more structural variation than repeating one plant.

| Controls | Effect |
| --- | --- |
| Each `*_culm` | Length, radius, taper, bend, noise and path samples. |
| Each `*_wood` | Joint spacing and strength, tube sides and texture repeats. |
| Each `*_branches` | Branch pairs, attachment range, azimuth, droop and length progression. |
| Each `*_branchlets` | Fine sprays carrying the foliage. |
| `leaf_shape`, `leaf_sheath` | Narrow blade profile, curvature, subdivisions and the short basal sheath. |
| Each `*_leaves` | Random atlas cell, independent size variation and tint. |
| Each `*_sites`, `*_offset`, `*_lean` | Grove footprint, position, scale and small angular variation. |

Joint spacing is 0.35 meters along the culm. The tube uses one longitudinal
texture repeat per joint, `uv_scale:[1,2.857142857142857]`, so its pale bands
follow the same arc-length coordinate as the raised rings. Branch pairs use
normalized attachment intervals near those joints. Stronger bends or path noise
change arc length and can move the branch attachments away from a joint.

Each `*_leaf_sites` scatter now checks its branch mesh with `max_distance:0.002`
and `snap_to_mesh:true`. All 320, 240 and 400 prototype leaf placements pass the
2 mm threshold and snap onto supporting triangles. A short leaf sheath overlaps
the attachment and meets the blade; thicker fine twigs make the connection visible.
See [SCATTER_PROXIMITY.md](SCATTER_PROXIMITY.md) for distance filtering, snapping
and the accepted/rejected counts in the JSON report.

The dense culm samples resolve the narrow ring profiles. This is a detailed
demonstration, not an automatic game LOD. For a cheaper grove, reduce tube sides,
leaf subdivisions or branch counts. Increasing tube stride can lose narrow rings.
Tubes overlap at branch junctions and are capped, not welded or hollow bamboo.
Wind remains exported shader metadata.

## Overlapping growth

The recipe now saves `growth.mode:"developmental"` with 17 snapshots. Its three
culm prototypes begin at 0%, 4.5% and 9%, respectively. Copies within each group
share their prototype's development. The culms extend along their mature guides
and continue thickening while branches, branchlets and leaves develop.

| Staged nodes | Earliest start across the three prototypes | Behavior |
| --- | --- | --- |
| `*_culm` | 0..9% | Full length at 72% of local age; radius increases through maturity. |
| `*_branches` | 12..21% | Birth waits for the culm tip to reach the attachment; extension finishes at 38% of local age. |
| `*_branchlets` | 20..29% | Attached sprays extend before they finish thickening. |
| `*_leaves` | 26..35% | Baby leaves appear on reached supporting twigs, then expand at their own pivots. |

These are earliest start times; each attachment's actual arrival can be later.
Proximity/snap checks still use the supporting twig mesh, with stable mature
candidate membership. Full growth retains the original seeded mesh and atlas
selection. The grove instances are not staged again, so there is no additional
whole-plant shrinking on top of the developing culms.

```sh
./build/foliage_growth_frame out/samples/graphs/bamboo.json --check-sequence 96
./build/foliageutil out/samples/graphs/bamboo.json --out out/bamboo-growth --growth-step 10 --json
```

In Unity, package the prepared recipe and set **Source Node** to `wind` to select
the complete grove. This matters because the recipe also exports individual
culm/stem studies. See [the growth demo controls](../unity/demo/README.md).

## TexUtil materials

[culm.texutil.json](../samples/materials/bamboo/culm.texutil.json) builds longitudinal fibers,
green pigment variation, pale joint bands and a dark seam. The unbanded branch
color shares the same fine normal and roughness maps. Fine relief stays in the
normal texture while the large rings belong to geometry.

[leaves.texutil.json](../samples/materials/bamboo/leaves.texutil.json) generates eight
variants from [leaf.texutil.json](../samples/materials/bamboo/leaf.texutil.json), with parallel
veins and pigment variation. The atlas is 4 by 2 cells of 256 pixels, including
four pixels of padding on each side. It has matching color, normal and packed
roughness images. The leaf outline is actual lanceolate geometry, so these leaves
use opaque coverage with double-sided shading and saved `translucency:0.18`.

Color textures are sRGB, normals are 16-bit OpenGL data, and packed roughness is
in G with metallic zero in B. Repeated UVs are not a unique bake atlas.

```sh
../TexUtil/build/texutil samples/materials/bamboo/culm.texutil.json --out out/bamboo-materials --threads 8 --json
../TexUtil/build/texutil samples/materials/bamboo/leaves.texutil.json --out out/bamboo-leaves --threads 8 --json
./build/foliageutil out/samples/graphs/bamboo.json --out out/bamboo --seed 42 --no-growth --json
```

The foliage recipe reads prepared textures under `out/samples/assets/bamboo/`.
Rerun sample preparation after material edits; inspect the four culm/branch PNGs
and the leaf atlas manifest with its entire `.assets` directory before exporting
the bamboo again. TexUtil is only required to regenerate those maps.

| Output | Contents | Triangles |
| --- | --- | --- |
| `bamboo.glb`, `bamboo.obj` | Seven-culm grove | 548,184 |
| `bamboo-culm.glb` | One culm with branches and leaves | 78,312 |
| `bamboo-stem.glb` | Isolated culm for inspecting its joints | 15,400 |

## Render and verification

```sh
blender --background --factory-startup --python-exit-code 1 --python tools/render_blender.py -- --model out/bamboo/bamboo.glb --out out/bamboo/bamboo.png --hdr ../TexUtil/assets/hdri/outdoor.hdr --samples 96 --size 1400 --elevation 8 --azimuth 25 --light-scale 5
blender --background --factory-startup --python-exit-code 1 --python tools/render_blender.py -- --model out/bamboo/bamboo-stem.glb --out out/bamboo/stem-preview.png --hdr ../TexUtil/assets/hdri/studio.hdr --samples 32 --size 1000 --elevation 5 --azimuth 35 --light-scale 2 --target .06 0 1.72 --ortho-scale 1.10
```

On this Mac, use `/Applications/Blender.app/Contents/MacOS/Blender`. These are
actual GLB imports rendered with Blender 5.2.1 LTS, Cycles CPU, eight workers,
denoising, seed 42 and AgX at exposure zero. The saved foliage material adapter
applies translucency. Ordinary GLB viewers need their own adapter for that metadata.

The attachment close-up uses `bamboo-culm.glb`, 48 samples, size 1100, elevation
15, azimuth 40, light scale 3.5, target `.35 0 2.7` and ortho scale 1.3.

All three GLBs pass Khronos validation with zero errors. The grove and complete
culm have three runtime tangent-generation warnings each; the isolated stem has
one. A repeat with seed 42 is byte-identical on this build, and seed 43 changes
the export. The Filament helper also rendered the geometry with solid materials.
