# Japanese sakura study

This original weeping cherry example uses the same generic nodes as the willow,
with native TexUtil blossoms, sprig atlases and cherry bark. It is inspired by the
broad, cascading canopy in [Miharu's Takizakura photographs](https://miharukoma.com/blog/information/15539)
and the pale, five-petalled flowers in [Minato AQULS' blossom photographs](https://minatoaquls.com/news_release/2024/0417000311.html).
These are visual references, not texture inputs or an exact reconstruction of a
particular tree. No reference photograph is copied into the sample package.

The [source graph](../samples/graphs/sakura.json) and [native material sources](../samples/materials/sakura/README.md)
are reproducible. Prepare the portable assets before running the graph:

```sh
python3 tools/prepare_samples.py --texutil ../TexUtil/build/texutil
./build/foliageutil out/samples/graphs/sakura.json --out out/sakura --seed 42 --json
./build/foliageutil out/samples/graphs/sakura.json --out out/sakura --seed 42 --card-bake FloweringShoots --json
blender --background --factory-startup --python-exit-code 1 --python tools/render_blender.py -- --model out/sakura/sakura.glb --out out/sakura/sakura.png --hdr ../TexUtil/assets/hdri/outdoor.hdr --size 1600 --samples 96 --threads 6 --light-scale 8 --elevation 8 --azimuth 22 --ground-height 0 --exposure -0.4
```

The HDR and Blender executable are optional local preview dependencies. Substitute
your own HDR path. Foliage generation and CardBake use the prepared textures and
do not require either renderer or TexUtil at runtime.

## Construction

The tree has six spreading roots, twelve broad arms, seven sprays per arm and
eight flowering twigs per spray. Direction profiles turn the shoulders outward
before the smaller branches droop. The trunk, roots, arms and sprays are unioned
into one connected, watertight wood component. Flowers and fine twigs remain
separate surfaces, so the complete tree is not a watertight solid.

A 3-by-2 native spritesheet contains six softly warped/tinted sprig variations.
Each sprig has six pale flowers, two buds and a visible connecting twig extending
to its attachment. Crossed, curved cards receive random cells, independent size
jitter and orientation variation. Seed 42 accepts 8,735 of 8,736 attempted sites
within 0.014 meters of the supporting twig mesh and snaps them to its surface.
This checks card pivots, not alpha silhouettes or collision between flowers.

The canopy uses actual RGBA color, OpenGL normals and packed roughness maps.
`alpha_mode:MASK` cuts the stencil; saved `translucency:0.2` controls thin-tissue
backlighting in the Blender adapter. Mature and young wood use separate bark
materials. The single-blossom study additionally has subtle subsurface scattering.

These are textured flower cards, not individually modelled stamens and petals.
They are intended for a tree canopy; the isolated branch/blossom exports make the
simplification visible at close range. For a macro flower, use geometry petals
and stamens as in the rose and sunflower recipes.

## Geometry and export choices

| Export, seed 42 | Triangles | Purpose |
| --- | ---: | --- |
| `sakura.glb` | 299,464 | Detailed sprig-card canopy |
| `sakura-LOD1.glb` | 123,528 | Lower card density and topology |
| `sakura-LOD2.glb` | 80,976 | Sparse, simple cards |
| `sakura-card-baked.glb` | 52,192 | Fitted cards with baked flowering shoots |
| `sakura-wood.glb` | 41,440 | Connected structural wood |
| `sakura-branch.glb` | 234 | Attachment and material study |
| `sakura-blossom.glb` | 72 | Single curved flower card |

`FloweringShoots` fits two four-segment planes to each of 672 twig guides. It
captures the generated canopy into 1,344 unique cells on six atlas pages and
retains the structural wood and wind metadata. Its 52,192 triangles are 82.6%
fewer than the base export. The baked GLB is larger on disk because of its new
textures: approximately 87 MiB versus 62 MiB for the base. Geometry reduction
is not texture-memory reduction.

Compared with LOD2, the fitted bake keeps more canopy coverage at the inspected
angles. It still projects depth onto cards, so lighting, side views and overlap
can differ. Profile alpha overdraw and texture memory in the destination engine.
See [CardBake](CARD_BAKE.md) for projection and material limits.

## Verification

The Release suite passes 32/32 cases, including the new sample. All seven exports
pass Khronos glTF validation with zero errors; warnings request generated tangent
space for normal maps. Repeated seed-42 exports are byte-identical in the tested
local build, and seed 43 changes the tree and passes validation.

Actual Blender 5.2.1 LTS Cycles renders were inspected for the full tree, branch,
matched-scale base/LOD2/CardBake comparison and a second CardBake angle. Final
renders, packed scenes and JSON evidence remain under ignored `out/sakura/` and
`out/sakura-work/`. The sakura also appears in the [publication showcase](images/foliageutil-samples.png).


## Growth animation

The separate [sakura-growth recipe](../samples/graphs/sakura-growth.json) uses
native `growth.mode:"developmental"` and shares the mature sakura's meshes, seed and textures.
The original recipe keeps its mature/LOD/CardBake export behavior. The growth
recipe exports only the tree, with 32 standard CLI snapshots, and omits the
standalone flower/branch studies and CardBake profiles.

| Node | Earliest start | End | Length complete at local age |
| --- | ---: | ---: | ---: |
| `trunk` | 0% | 100% | 72% |
| `roots` | 0% | 100% | 55% |
| `arms_raw` | 12% | 100% | 48% |
| `sprays_raw` | 20% | 100% | 50% |
| `twigs_raw` | 28% | 100% | 52% |
| `flowers` | 30% | 100% | Uses a baby-to-mature size channel |

Each branch actually starts when its parent reaches its attachment, after the
earliest start above and a small attachment delay. Lower shoots develop while
the trunk and higher shoots are still extending. The trunk continues gaining
radius after reaching full height. Branches thicken from their mature reference
radii, capped by support maturity, so parent and child growth no longer multiply
their radii toward zero. Young tips narrow and gradually fill out.

Flowering sprigs first appear small on reached twig sections, then expand through
their `scale_profile`. The new schedule passes all 192 sampled frames with
connected solid wood and keeps the same 299,464-triangle mature tree. Other frame
counts or edited timing need fresh checks, particularly near birth and profile
boundaries. The sprites already depict open blossoms: this is staged sprig growth,
not individually modelled bud opening. For actual baby leaf instances, see
[development-tree.json](../samples/graphs/development-tree.json).

For ordinary saved steps:

```sh
python3 tools/prepare_samples.py --texutil ../TexUtil/build/texutil
./build/foliageutil out/samples/graphs/sakura-growth.json --out out/sakura-steps --json
```

For the smooth Blender animation, the small optional public-API adapter samples
`Options.growth` continuously. It calls the same native stage evaluator as CLI
snapshots. It writes one selected GLB into a reusable current-frame directory,
avoiding 192 copies of the embedded textures on disk. The renderer imports each
mesh and reuses the prepared materials from the reference scene.

```sh
cmake --build build --target foliage_growth_frame --parallel 8
./build/foliage_growth_frame out/samples/graphs/sakura-growth.json --check-sequence 192 > out/sakura-sequence-check.json
blender --background --factory-startup --python-exit-code 1 --python tools/render_growth_animation.py -- --recipe out/samples/graphs/sakura-growth.json --generator build/foliage_growth_frame --output-name sakura-growth.glb --scene out/sakura/sakura.blend --out out/sakura-development/final --frames 192 --fps 24 --width 1280 --height 960 --samples 24 --threads 16
```

First generate and render the mature scene with the command at the top of this
page. It supplies the fixed camera, lighting, real texture bindings, thin-tissue
material adapter and presentation ground. Blender, FFmpeg on PATH and the
optional `foliage_growth_frame` executable are preview dependencies only.
Use the installed Blender path. This command was checked with Blender 5.2.1 LTS
and Cycles CPU. Choose a suitable worker count for your machine.

`--preview 48 101 140 168 191` renders only those zero-based frames into a separate
preview `--out` directory. It does not save a movie or animation scene. Inspect
preview frames before rendering a changed schedule. Add `--resume` to reuse completed
PNGs after an interruption; it verifies the recipe, generator binary, source scene,
renderer script and render settings before rebuilding the Blender timeline. Its
camera framing and overlapping phase captions are authored for this sakura study.

The final output directory contains `sakura-growth.mp4`, `growth-animation.blend`,
`animation.json`, the PNG frame sequence and one current-frame GLB. The 24-fps movie
has eight seconds of growth, a half-second opening hold and two seconds at full
bloom. The packed Blender scene stores the same mesh sequence with keyed visibility
and stage labels, so its 252-frame timeline can be scrubbed without running the
generator again. These are independent meshes switched each frame, not skeletal
animation or morph targets. Textures are shared within the scene. All generated
movies, frames, models and scenes stay under ignored `out/`.

## Packaged growth exports

See [Growth animation and stage packages](EXPORTS.md) for native Alembic export
and independently generated geometry/CardBake stages. Named profiles select
their own progress samples and optional LOD. Each CardBake stage captures its
currently grown source and visible guide paths; empty foliage can retain wood
alone. These profiles run explicitly with `--export NAME`.
