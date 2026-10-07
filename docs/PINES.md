# Upright and spreading pines

Two seeded pine studies use the same paired needle geometry and native TexUtil
bark material, with different branching structures:

| Recipe | Shape | Default seed | Full mesh triangles | Approximate width / height / depth |
| --- | --- | ---: | ---: | --- |
| [pine-upright.json](../samples/graphs/pine-upright.json) | Straight leader, tapered crown, five-member branch whorls, longer lower sprays | 42 | 933,708 | 7.23 / 9.35 / 7.50 m |
| [pine-spreading.json](../samples/graphs/pine-spreading.json) | Gently curved trunk, long rising boughs, open asymmetric crown | 73 | 718,716 | 11.51 / 9.21 / 10.89 m |

Dimensions are whole-mesh bounds, including the small buried part of the roots.
Ordinary exports are detailed geometry assets with individual needles. Both
recipes also save two optional fitted CardBake profiles, described below.
No runtime performance claim is implied.

## Generate

```sh
cmake --build build --parallel 8
python3 tools/prepare_samples.py --texutil ../TexUtil/build/texutil
./build/foliageutil out/samples/graphs/pine-upright.json --out out/pines/upright --json
./build/foliageutil out/samples/graphs/pine-spreading.json --out out/pines/spreading --json
```

Each graph exports the complete `pine-*.glb` and a `pine-*-wood.glb` structural
study. The full GLB embeds bark color, OpenGL normals and packed roughness maps.
Needles use dark green scalar materials, per-instance color variation and lighter
tips. Saved thin translucency is applied by the Blender preview adapter.

The [pine material source](../samples/materials/pine/README.md) imports only the
packaged rooted-tree bark recipe. All assets remain portable within `out/samples/`;
TexUtil is needed for preparation, not for generation from the prepared bundle.

## Shape and attachment controls

- `trunk` controls height, lean, taper and basal flare. `roots` reaches the Y=0
  presentation ground plane and buries its tips.
- `lower_boughs` and `upper_boughs` control the crown envelope. The upright recipe
  uses whorls with decreasing branch length; the spreading recipe uses alternate
  attachments, longer limbs and more length/angle variation.
- `lower_laterals` and `upper_laterals` add the fine branch structure. Upright
  `lower_shoot_guides` and `upper_shoot_guides` separately taper the final sprays.
- `shoots` bends the final needle-bearing paths upward before tubing or scattering.
  `needle_sites` and `tip_sites` snap each pivot to `shoot_mesh` within 12 mm.
- `needle` and `paired_needle` share an attachment base. Their merged `fascicle`
  supplies two diverging needles at each site; `tip_needles` adds short young tips.

Trunk, roots, boughs and laterals form one connected watertight `wood` solid.
The fine shoot tubes overlap their parent laterals and stay visible beneath the
needles. Those shoots and the open needle surfaces are merged after the structural
union, so the full tree is not a single watertight body. Wind is saved metadata.

Reduce `needles.density` and `tip_needles.density` for fewer needles while retaining
their attachment streams. Changing branch counts or renaming stochastic nodes
changes the generated shape. Ordinary generation retains dense needle geometry
for close inspection; select a saved CardBake profile for a fitted card export.

## CardBake versions

Both graphs provide the same two profiles:

- `NeedleShoots` fits two crossed cards to every final needle-bearing shoot. It
  captures `shoot_mesh`, `needles` and `tip_needles`, and retains the original
  connected structural wood. This preserves more of the individual tufts and is
  the preferred starting point for visual fidelity.
- `NeedleSprays` captures the finer lateral branch tubes, shoots and needles
  together along each lateral guide. Only the trunk, roots and main boughs remain
  as solid geometry. The larger cards give a much smaller mesh, with more visible
  flattening and clumping at close range.

Both use the actual generated foliage and its colors, including the supporting
twig at each base. Neither profile is a whole-tree billboard. They are explicit
exports, with no automatic runtime LOD switching.

| Tree | Profile | Triangles | Reduction from original | Cards | 2048-square pages per map | Baked map memory, RGBA8 |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Upright | `NeedleShoots` | 138,368 | 85.2% | 4,370 | 7 | 336 MiB |
| Upright | `NeedleSprays` | 44,616 | 95.2% | 1,090 | 5 | 240 MiB |
| Spreading | `NeedleShoots` | 73,524 | 89.8% | 3,132 | 5 | 240 MiB |
| Spreading | `NeedleSprays` | 22,286 | 96.9% | 522 | 3 | 144 MiB |

Memory here counts the three baked RGBA8 maps only, before mipmaps or target-engine
compression; retained bark maps are additional. Compressed PNG/GLB file size is
not GPU texture memory. Atlas memory, alpha overdraw and mip coverage remain costs
despite the geometry reduction. No Unity/player performance has been measured for
these pine bakes.

```sh
python3 tools/prepare_samples.py --texutil ../TexUtil/build/texutil
./build/foliageutil out/samples/graphs/pine-upright.json --card-bake NeedleShoots --out out/pines/card-baked/upright --json
./build/foliageutil out/samples/graphs/pine-upright.json --card-bake NeedleSprays --out out/pines/card-baked/upright --json
./build/foliageutil out/samples/graphs/pine-spreading.json --card-bake NeedleShoots --out out/pines/card-baked/spreading --json
./build/foliageutil out/samples/graphs/pine-spreading.json --card-bake NeedleSprays --out out/pines/card-baked/spreading --json
```

`NeedleShoots` writes `pine-*-card-baked.glb`; `NeedleSprays` writes
`pine-*-card-baked-low.glb`. Keep each GLB and its matching `.cardbake/` directory,
including `manifest.json` and the whole set of PNG pages, together. The GLBs embed
their maps, while the sidecar packages retain the bake layout and editable images.
Edit profiles in `samples/graphs/` and prepare again before regenerating.

The finer profile uses three segments per plane and 64x96 cells with three-pixel
padding. The compact profile uses four segments and 128x128 cells with four-pixel
padding. Both use 3x3 supersampling, MASK cutoff 0.3 and translucency 0.1 to retain
fine needle coverage. Every occupied guide receives two unique atlas cells.

The compact profile's `max_distance` is 0.65 m for upright and 0.9 m for spreading,
because needle bases lie on offshoots extending away from the lateral guides.
The finer profile checks those bases directly against shoot guides within 25 mm.
These are component-to-guide assignment bounds, not geometric gap filling.
Original needle scatter still snaps each base to the real shoot surface.

All four bakes retain `_WIND` metadata. Each profile applies the original tree's
wind height separately to the captured foliage and retained wood. The ordinary
tree and wood exports remain byte-for-byte unchanged by these added profiles.

## Render the actual exports

Using the installed Blender executable, run the following from the repository
root. On this macOS host it is `/Applications/Blender.app/Contents/MacOS/Blender`.
The renderer loads Blender's bundled studio HDR unless `--hdr` supplies another.

```sh
blender --background --factory-startup --python-exit-code 1 --python tools/render_blender.py -- --model out/pines/upright/pine-upright.glb --out out/pines/upright/pine-upright.png --size 1400 --samples 64 --light-scale 10 --ground-height 0 --elevation 12 --azimuth 22
blender --background --factory-startup --python-exit-code 1 --python tools/render_blender.py -- --model out/pines/spreading/pine-spreading.glb --out out/pines/spreading/pine-spreading.png --size 1400 --samples 64 --light-scale 10 --ground-height 0 --elevation 12 --azimuth 22
```

Use `--azimuth 120 --elevation 16` and different output filenames for a second
angle. Ground is separate presentation geometry and is absent from the GLBs.
The renders save packed Blender scenes and settings alongside their PNGs.

`tools/render_variants.py` makes a labeled comparison at the same physical scale:

```sh
blender --background --factory-startup --python-exit-code 1 --python tools/render_variants.py -- --models out/pines/upright/pine-upright.glb out/pines/spreading/pine-spreading.glb --labels "Upright pine" "Spreading pine" --hdr PATH_TO_STUDIO_HDR --out out/pines/pine-variants.png --width 2000 --height 1150 --samples 64
```

Replace `PATH_TO_STUDIO_HDR` with a real HDR environment. The local comparison
uses TexUtil's `assets/hdri/studio.hdr`; this optional preview input is not a
dependency of either foliage graph or material package.

To compare all three upright representations with the same scale and lighting:

```sh
blender --background --factory-startup --python-exit-code 1 --python tools/render_variants.py -- --models out/pines/upright/pine-upright.glb out/pines/card-baked/upright/pine-upright-card-baked.glb out/pines/card-baked/upright/pine-upright-card-baked-low.glb --labels "Original: 933,708 tris" "NeedleShoots: 138,368 tris" "NeedleSprays: 44,616 tris" --hdr PATH_TO_STUDIO_HDR --out out/pines/card-baked/upright/comparison.png --width 2400 --height 1100 --samples 64 --transparent-bounces 64
```

Use the spreading filenames and triangle counts from the table for its comparison.
For a second angle, render each GLB with `tools/render_blender.py`,
`--azimuth 120 --elevation 16 --light-scale 10 --transparent-bounces 64` and a
distinct output filename. The local `comparison-side.blend` scenes instead rotate
all three trees 120 degrees around their own trunk bases, retaining the shared
camera, labels and lighting of each comparison.

## Verification

The Release build passes all 38 CTest cases. Both default-seed trees and wood
studies pass Khronos GLB validation with zero errors. Each has one expected
runtime-generated tangent-space warning for normal-mapped bark. The two additional
seeds, 43 for upright and 74 for spreading, also generate and validate successfully.
Repeated default-seed exports match byte-for-byte on this build; the changed seeds
produce different files. No cross-platform byte identity is claimed.

All 100,510 upright and 81,432 spreading needle/tip placement attempts pass their
support checks and snap to shoot surfaces. Default structural wood unions each
report one watertight component. Native TexUtil maps, the matched-scale comparison,
full-tree previews and second-angle previews are inspected from actual exports.
Reports, renders, GLBs and packed scenes stay under ignored `out/pines/`.

All four CardBake GLBs also pass Khronos validation with zero errors. The finer
upright/spreading bakes have 8/6 runtime tangent-space warnings; the compact bakes
have 6/4. Every warning is for a normal-mapped bark or card material. Every guide
has two uniquely assigned, nonempty cells, all images embed in GLB, all baked
materials use double-sided MASK coverage, and sidecar map paths resolve locally.
Repeated bakes reproduce all GLB and PNG bytes on this build. The 38-test Release
suite passes after the profile additions. Front and second-angle comparisons use
actual exported models with 64 transparent bounces in Blender Cycles.
