# Meadow fillers and common bushes

Ten small environment assets use shared native TexUtil greenery materials. Each
source graph exports detailed geometry and saves two optional fitted CardBake
profiles, `Cards` and `CardsLow`. The collection contains green foliage without
flowers, fruit or seasonal decorations. Names describe useful environment shapes,
not botanical reconstructions.

## Meadow and field

Triangle counts include any retained stem geometry. Dimensions are source mesh
bounds in meters, shown as width / height / depth. All assets use right-handed
Y-up coordinates with their planting origin at ground level near the base center.
Thin stems and fitted-card margins can extend slightly below Y=0.

| Recipe | Source triangles | Cards | CardsLow | Dimensions, m |
| --- | ---: | ---: | ---: | --- |
| [Short grass](../samples/graphs/filler-short-grass.json) | 1,680 | 224 | 168 | 0.48 / 0.32 / 0.44 |
| [Tall grass](../samples/graphs/filler-tall-grass.json) | 1,680 | 224 | 168 | 0.74 / 0.92 / 0.63 |
| [Clover patch](../samples/graphs/filler-clover.json) | 2,646 | 432 | 324 | 0.36 / 0.15 / 0.36 |
| [Broadleaf rosette](../samples/graphs/filler-rosette.json) | 2,280 | 100 | 60 | 0.43 / 0.25 / 0.41 |
| [Upright field weeds](../samples/graphs/filler-weeds.json) | 11,400 | 80 | 40 | 0.43 / 0.63 / 0.39 |
| [Low bramble](../samples/graphs/filler-bramble.json) | 21,518 | 112 | 56 | 0.82 / 0.45 / 0.78 |

Grass uses 28 individual blade guides. Each guide receives one fitted strip, with
four segments for `Cards` and three for `CardsLow`. The extra low-profile segment
is necessary to preserve these narrow curved blades during projection. Grouping
whole fans onto a few broad cards or reducing each blade to two segments lost too
much visible coverage during visual checks.

Clover has nine stalks, each with three separate leaflets and connecting petioles.
It retains its thin stalks as geometry and bakes one strip per leaflet. The lower
preset also reduces stem sampling. Rosette cards follow each of ten broad leaves.
Weeds and bramble instead bake their leaves and fine stems together onto crossed
strips along five upright stems or seven arching canes.

## Common bushes

| Recipe | Source triangles | Cards | CardsLow | Dimensions, m |
| --- | ---: | ---: | ---: | --- |
| [Rounded small-leaf bush](../samples/graphs/bush-rounded.json) | 82,080 | 992 | 512 | 1.02 / 0.72 / 0.92 |
| [Wide spreading bush](../samples/graphs/bush-spreading.json) | 55,456 | 992 | 512 | 1.28 / 0.81 / 1.47 |
| [Upright leafy bush](../samples/graphs/bush-upright.json) | 58,140 | 840 | 432 | 1.02 / 1.70 / 0.87 |
| [Loose hedgerow bush](../samples/graphs/bush-hedgerow.json) | 45,612 | 868 | 448 | 1.48 / 1.18 / 1.52 |

The bushes retain six to eight woody stems. Their leafy side shoots become crossed
cards with four segments per plane, or two in `CardsLow`. The lower preset also
reduces retained wood from five to four radial sides and increases path stride.
These are natural multi-stem silhouettes with open branch gaps, not clipped hedge
blocks. Wood is a set of overlapping tubes, not a boolean-unioned solid.

`main_*` curves control height, spread and lean. The `guides` branch node controls
leafy shoot placement; `sites` and `leaves` control leaf population and variation.
The source leaves snap to their supporting twig mesh before baking. Source twigs
remain visible in the capture so the leaf clusters connect to retained branches.

## Generate and bake

Run from the repository root after building FoliageUtil:

```sh
python3 tools/prepare_samples.py --texutil ../TexUtil/build/texutil
for name in filler-short-grass filler-tall-grass filler-clover filler-rosette filler-weeds filler-bramble; do
  ./build/foliageutil "out/samples/graphs/$name.json" --out "out/filler-foliage/$name" --json
  ./build/foliageutil "out/samples/graphs/$name.json" --card-bake Cards --out "out/filler-foliage/$name" --json
  ./build/foliageutil "out/samples/graphs/$name.json" --card-bake CardsLow --out "out/filler-foliage/$name" --json
done
for name in bush-rounded bush-spreading bush-upright bush-hedgerow; do
  ./build/foliageutil "out/samples/graphs/$name.json" --out "out/bushes/$name" --json
  ./build/foliageutil "out/samples/graphs/$name.json" --card-bake Cards --out "out/bushes/$name" --json
  ./build/foliageutil "out/samples/graphs/$name.json" --card-bake CardsLow --out "out/bushes/$name" --json
done
```

Each recipe produces `NAME.glb`, `NAME-cards.glb`, `NAME-cards-low.glb` and the
two matching `.cardbake/` packages. Ordinary exports do not bake automatically.
Keep each baked GLB beside its complete matching directory. The GLB embeds its
textures; the directory retains editable PNGs and the cell manifest. See
[CardBake](CARD_BAKE.md) for projection details.

Edit canonical graphs in `samples/graphs/`, then prepare again. Base layouts for
grass, clover and rosettes are explicitly authored; changing their root seed does
not randomize those layouts. Weed, bramble and bush leaf populations, and bush
shoots, use seeded variation. Change the authored transforms or stem curves for
new layout variants. Source meshes can be regenerated from the prepared package
without TexUtil. Shared material sources are documented [here](../samples/materials/filler/README.md).

## Atlas and game-import costs

Every profile fits onto one page per map at the saved seed. Each page has color
with alpha, OpenGL normals and packed roughness/metallic data.

| Set | Cards page size | CardsLow page size | Three maps, RGBA8, Cards / CardsLow |
| --- | ---: | ---: | ---: |
| Each meadow filler | 512 x 512 | 256 x 256 | 3 / 0.75 MiB |
| Each bush | 1024 x 1024 | 512 x 512 | 12 / 3 MiB |

These estimates exclude mipmaps and engine texture compression. PNG file size is
not GPU memory. Clover and bush stems use scalar materials, so their retained
geometry adds no texture maps. All profiles use 3x3 supersampling, MASK cutoff
0.3, thin translucency 0.16 and unique cells. Near profiles have three-pixel
padding, while lower profiles have two. No atlas cells are randomized afterward.

Use a double-sided alpha-test material, color as sRGB, and normals/packed data as
linear. Packed G is roughness and B is metallic. Preserve alpha coverage when
generating mipmaps and select target-platform texture compression in the engine.
These small atlases favor repeated background props; profile overdraw, mip behavior
and edge-on views at the intended camera distance. Bush cards visibly flatten
leaf depth and can expose more branch gaps than the full source mesh.

`CardsLow` is an independently baked asset. Assign the two representations to the
engine's LOD system explicitly; these recipes do not configure runtime switching.
The files retain `_WIND` metadata, but an engine shader/adapter must animate it.
The supplied Blender adapter displays thin translucency. No Unity/player visual
or performance verification is claimed for this collection.

## Render and inspect

`tools/render_asset_grid.py` creates individually framed catalog cells from actual
GLBs, with triangle labels. It records display scales, camera angles and settings
in JSON and saves a packed Blender scene. Cells are not at a shared physical scale.
Supply an existing HDR environment; this local run uses TexUtil's optional
`assets/hdri/outdoor.hdr`, which is not a recipe dependency.

```sh
blender --background --factory-startup --python-exit-code 1 --python tools/render_asset_grid.py -- --models out/bushes/bush-rounded/bush-rounded-cards.glb out/bushes/bush-spreading/bush-spreading-cards.glb out/bushes/bush-upright/bush-upright-cards.glb out/bushes/bush-hedgerow/bush-hedgerow-cards.glb --labels "Rounded small-leaf" "Wide spreading" "Upright leafy" "Loose hedgerow" --title "COMMON BUSHES / CARDS" --columns 2 --hdr PATH_TO_HDR --out out/bushes/bushes-cards.png
```

Repeat with `--azimuth 120` and a different output name for the second view.
Substitute source or `-cards-low.glb` filenames for the other representations.
The renderer uses 64 transparent bounces, avoiding dark termination through layered
cutouts. All final renders and packed scenes remain under ignored `out/`.

## Verification

All ten source graphs validate. Their 30 default-seed GLBs and 30 additional-seed
GLBs pass Khronos validation with zero errors. Normal-mapped materials produce
runtime-generated tangent-space warnings; unused declared materials produce
informational messages. All 20 final bakes have one page, nonempty uniquely assigned
cells, embedded images, double-sided MASK cards and retained wind attributes.
Repeated default-seed bakes reproduce every GLB and PNG byte on this build.
Cross-platform byte identity is not promised.

The Release build passes all 38 CTest cases. Real TexUtil textures, full source
galleries, and both viewing angles of each card preset were inspected in Blender
Cycles. Reports and additional-seed exports are in `out/filler-foliage/verification/`;
final assets and renders are under `out/filler-foliage/` and `out/bushes/`.
