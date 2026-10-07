# Asiatic lily: dark plum and orange

[asiatic-lily.json](../samples/graphs/asiatic-lily.json) builds a reference-inspired
flowering plant with three open six-tepal blooms, two elongated ribbed buds, and
36 full lanceolate leaves. Each flower has cream filaments, six rust-brown anthers, a green
central style and a three-lobed stigma. The petals have dark plum centers, irregular
red-to-orange margins, orange tips, a pale gold throat and dark throat speckles.

This is an authored environment asset with reusable primitive geometry and native
TexUtil materials. The supplied photographs guide its appearance; they are not
texture inputs or repository dependencies. It does not identify a cultivar.

## Models and proportions

| Export | Triangles | Representation |
| --- | ---: | --- |
| `asiatic-lily.glb` | 28,914 | Full plant with curved geometry petals and leaves |
| `asiatic-lily-bloom.glb` | 6,644 | One separate bloom, local floral axis +Y |
| `asiatic-lily-cards.glb` | 9,762 | 54 fitted strips, detailed retained stems and floral organs |
| `asiatic-lily-cards-low.glb` | 2,364 | 54 coarser strips and lower-resolution retained geometry |

The whole plant is approximately 0.568 m wide, 1.161 m high and 0.461 m deep.
The large bloom is approximately 0.295 m across. All exports use right-handed
Y-up meters. The planting origin is at the main stem's ground-level base; tiny
stem radii and card margins can extend below Y=0. The isolated bloom's origin is
its receptacle, so it is a flower prototype rather than another complete plant.

## Generate

```sh
cmake --build build --parallel 8
python3 tools/prepare_samples.py --texutil ../TexUtil/build/texutil
./build/foliageutil validate out/samples/graphs/asiatic-lily.json --json
./build/foliageutil out/samples/graphs/asiatic-lily.json --out out/asiatic-lily --json
./build/foliageutil out/samples/graphs/asiatic-lily.json --card-bake Cards --out out/asiatic-lily --json
./build/foliageutil out/samples/graphs/asiatic-lily.json --card-bake CardsLow --out out/asiatic-lily --json
```

Ordinary generation writes the plant and isolated bloom. CardBake profiles run
only when selected. Each baked GLB embeds its maps and has a matching `.cardbake/`
directory with editable PNGs and a manifest. Keep those packages beside their GLBs.
All generated models, textures, scenes, previews and archives stay under `out/`.

The material source is [lily.texutil.json](../samples/materials/asiatic-lily/lily.texutil.json).
See [its guide](../samples/materials/asiatic-lily/README.md) for palette and pigment
controls. Prepared graphs and maps are portable without TexUtil after preparation.

## Authoring controls

`petal_shape_0` through `petal_shape_5` alternate wider and narrower surfaces.
Their width profiles open quickly at the throat, broaden through the middle and
taper to pointed tips. Negative curl and the radial placement give a cupped base
with gently recurved outer portions. Small twists, folds and edge waves keep the
six petals from being identical flat planes. Their attachment bases are distinct,
and each matching centerline is transformed with its petal for baking.

`flower_petals_0` through `flower_petals_2` place the flower surfaces on three
axes at different heights, orientations and scales. The corresponding
`flower_detail_*` transforms place the retained filaments, anthers and style.
Change each set together. `flower_stalk_*` curves connect the side flowers to the
main stem. The main flower sits at the main stem's tip.

`leaf` controls a 19 cm long, 5.5 cm wide prototype. Its width profile fills the
middle of the blade while retaining tapered bases and pointed tips. Shallow folds,
arched tips and upright attachment angles give the green leaves more body; upper
leaves retain more of their size for coverage along the stem. Explicit `leaf_*`
placements have matching `leaf_guide_*` paths. Six additional leaves are interleaved
between the original placements to fill the stem more evenly. Update the guides when changing
length, curl, twist or placement so CardBake follows the leaf centers.

`bud_axis` supplies the tapered closed bud envelope;
`bud_stalk_*` curves end at each bud's base. Geometry tubes overlap at branching
junctions. The plant is not a boolean-unioned solid, and its petals/leaves are
open double-sided surfaces.

The saved layout is explicitly authored. Changing the root seed does not rearrange
the flower axes or leaf transforms. Edit those controls for another plant layout.
The root seed still follows ordinary evaluator behavior where randomness is used;
no botanical growth simulation is provided by this recipe.

## CardBake profiles

Both profiles capture the 18 petals and 36 leaves individually. Their 54 matching
guides each receive one curved fitted strip and one unique atlas cell. Stems,
buds, filaments, anthers, style and stigma remain geometry so the central flower
details retain depth. The uniform baked translucency is 0.12; the source petals
and leaves use 0.10 and 0.16 respectively through the Blender preview adapter.

| Profile | Card triangles | Retained triangles | Segments per strip | Cell | Atlas page |
| --- | ---: | ---: | ---: | --- | --- |
| `Cards` | 864 | 8,898 | 8 | 128 x 128 | 1024 x 1024 |
| `CardsLow` | 432 | 1,932 | 4 | 64 x 64 | 512 x 512 |

Each profile uses one page per map, 3x3 supersampling, MASK cutoff 0.35, a 1.5 mm
fit margin and a 5 mm component-to-guide assignment limit. Padding is four pixels
for `Cards` and three for `CardsLow`. The lower profile reduces the retained tube
sampling and the anther/stigma/bud topology as well as the card subdivisions.

The three baked maps occupy approximately 12 MiB (`Cards`) or 3 MiB (`CardsLow`)
when loaded as RGBA8, before mipmaps or engine compression. Both also retain a
1024-square bud color map, another 4 MiB under that assumption. File compression
does not determine GPU memory use. Card capture loses some transverse petal cup,
fold detail and depth; use full geometry for close inspection.

Import cards with double-sided alpha testing. Color is sRGB, normals are OpenGL
tangent space, and packed G/B are linear roughness/metallic. Preserve alpha
coverage in mipmaps and profile overdraw at the intended game distance. The
full plant and both card variants carry `_WIND` metadata; the isolated bloom study
does not add wind. Metadata requires an engine shader/adapter to animate it.
LOD switching is also an engine setup step, not configured by these bake profiles.
Unity/player appearance and performance have not been tested for this asset.

## Render actual exports

Use the existing Blender preview adapter and an HDR environment. These local
renders use TexUtil's optional `assets/hdri/studio.hdr`, which is not a recipe
dependency. An exposure of -2 stops preserves the dark pigment in this studio rig.

```sh
blender --background --factory-startup --python-exit-code 1 --python tools/render_blender.py -- --model out/asiatic-lily/asiatic-lily.glb --out out/asiatic-lily/asiatic-lily.png --hdr PATH_TO_HDR --size 1500 --samples 64 --azimuth 15 --elevation 12 --exposure -2
blender --background --factory-startup --python-exit-code 1 --python tools/render_blender.py -- --model out/asiatic-lily/asiatic-lily-bloom.glb --out out/asiatic-lily/bloom-closeup.png --hdr PATH_TO_HDR --size 1500 --samples 64 --azimuth 12 --elevation 72 --exposure -2 --light-scale .3 --target 0 0 .014 --ortho-scale .34
blender --background --factory-startup --python-exit-code 1 --python tools/render_variants.py -- --models out/asiatic-lily/asiatic-lily.glb out/asiatic-lily/asiatic-lily-cards.glb out/asiatic-lily/asiatic-lily-cards-low.glb --labels "Geometry: 28,914 triangles" "Cards: 9,762 triangles" "CardsLow: 2,364 triangles" --hdr PATH_TO_HDR --out out/asiatic-lily/comparison.png --width 1900 --height 1400 --samples 64 --exposure -2
```

For a second view, render each baked model with `--azimuth 128 --elevation 18`
and a distinct output filename. The preview tools use 64 transparent bounces.
Packed Blender scenes and JSON settings are saved beside the renders.

## Verification

The source graph validates. Both ordinary exports and both card variants pass
Khronos validation with zero errors at the saved seed and at seed 682. The full
plant has two generated-tangent warnings; the isolated bloom and each card model
have one. These are normal-map tangent-generation warnings, not missing textures.

Repeated source exports and bakes match GLB and PNG bytes on the local build.
Both bakes have 54 distinct nonempty cells, embedded images, double-sided MASK
materials and wind data. The Release CTest suite passes all 38 cases. Actual
native TexUtil textures, the bloom close-up, the full plant, and front/side views
of both bakes were inspected in Blender Cycles. No cross-platform byte identity
or target-engine frame rate is claimed.

The fuller-leaf revision preserves the isolated bloom GLB and all source texture
bytes. Every non-leaf primitive in the full plant retains its vertex positions,
normals, UVs, colors and wind values. Materials and flower placements are unchanged.
