# CardBake: fitted cards from generated foliage

Prepare sample assets first with `python3 tools/prepare_samples.py --texutil
../TexUtil/build/texutil`. Source recipes are under `samples/`; executable textured
graphs are under `out/samples/graphs/`. Generated maps and bakes stay under `out/`.
See [sample setup](../samples/README.md).

CardBake captures the actual leaves and twigs of a generated mesh onto long,
curved cards fitted to its strand paths. It uses CPU material sampling,
barycentric projection and padding adapted from TexUtil, with a new fitted-card
projection/export backend in FoliageUtil. No TexUtil installation, GPU or Blender
is needed to bake. Blender is used only to inspect the exported asset.

## Export option

```sh
./build/foliageutil out/samples/graphs/weeping-willow.json --card-bake Strands --out out/willow-card-bake-final --json
./build/foliageutil out/samples/graphs/weeping-willow.json --card-bake StrandsLow --out out/willow-card-bake-final --json
./build/foliageutil card-bakes --json
```

`--card-bake NAME` selects one saved profile and exports its GLB and texture package
instead of ordinary graph outputs. Regular generation does not bake automatically.
The library equivalent is `exportCardBake(graph, name, directory, options)`.
`--seed` and normal geometry limits apply. LOD and growth flags cannot be combined
with this option; bake profiles execute the mature base source. Save distinct bake
profiles for different topology or atlas resolutions.

## Saved profile

Add a named profile under root `card_bakes`:

```json
"card_bakes": {
  "Strands": {
    "source": "bake_canopy_wind",
    "paths": "strands",
    "keep": "bake_structure_wind",
    "output": "willow-tree-card-baked.glb",
    "segments": 6,
    "planes": 2,
    "cell_width": 64,
    "cell_height": 256,
    "atlas_size": 2048,
    "padding": 4,
    "samples": 2,
    "margin": 0.01,
    "max_distance": 0.05,
    "alpha_cutoff": 0.4,
    "translucency": 0.24,
    "memory_mb": 1024
  }
}
```

The executable and generated [schema](card-bakes.json) define defaults and bounds.
There are 1..8 named profiles; names are case-insensitively unique. Every profile
validates even if not selected.

| Control | Purpose |
| --- | --- |
| `source` | Mesh containing the foliage and small twigs being replaced. |
| `paths` | Skeleton with one path per strand, in the same coordinates as the source. |
| `keep` | Optional mesh retained unchanged, usually welded wood. |
| `segments` | 1..64 longitudinal segments per card. Six follows the willow's shoulder closely; three reduces topology further. |
| `planes` | One strip or two crossed strips per occupied path. Each plane has its own bake cell. |
| `cell_width`, `cell_height` | Pixel dimensions including transparent gutters. Increase for close-up leaf details. |
| `atlas_size` | Square page dimensions; overflow creates more pages, up to 64. |
| `padding` | Cell gutter and color/normal dilation distance. Alpha coverage is never dilated. |
| `samples` | 1, 2x2 or 3x3 deterministic samples per pixel for silhouette filtering. |
| `margin` | Extra width and end margins in meters. |
| `max_distance` | Maximum component attachment distance to the closest guide path. Unmatched components fail. |
| `alpha_cutoff` | MASK threshold for the generated card material. |
| `translucency` | Uniform diffuse tissue transmission through the existing Blender adapter. |
| `memory_mb` | Estimated working memory budget for generated meshes, indexes, decoded PNGs, cell raster and one atlas page. |

## How it fits and captures

The source mesh is split into components connected by shared vertex indices.
Each component is assigned to the closest path at its lowest indexed vertex.
FoliageUtil's geometry leaves begin at their attachment pivot, and instances
preserve that ordering. Keep the fine twig geometry in `source` to capture the
visible connection at each card base. A source containing an entire welded tree
is inappropriate: its components no longer correspond to individual strands.
Imported or reordered meshes may need their base vertex/component layout prepared.
The test checks attachment distance, not full component containment.

Each occupied path is resampled into a small number of rows. A principal-axis fit
chooses the first plane's roll from the assigned foliage, and transported frames
follow the strand. The second plane crosses the first. Width and end extents fit
the projected source vertices plus the margin. The bake uses a distinct cell for
every path and plane; nearby strands cannot enter each other's capture.

Source triangles project into the curved strip's coordinates. A depth buffer
keeps the frontmost opaque or alpha-tested surface at every sample. Vertex colors,
material factors and bilinearly sampled UV textures supply color and roughness;
source geometric and texture normals are converted to the card's tangent frame.
Supersamples are resolved in linear space, with straight-alpha output and color
bleed into transparent gutters. No lighting or shadows are baked into the albedo.

This projection flattens a three-dimensional spray onto a curved surface. It is
not a general cage-ray baker: it approximates triangles across sharp bends, loses
depth/parallax, and uses the front capture for both sides. Crossed planes improve
coverage but can duplicate visible details and shadows. Close-up leaves will not
match the original exactly. Increase subdivisions where curves need them and
inspect front, side and back views at the intended game-camera distance.

## Materials, UVs and packaging

The package includes:

```text
willow-tree-card-baked.glb
willow-tree-card-baked.cardbake/
  manifest.json
  0-color.png
  0-normal.png
  0-metallic-roughness.png
  ... further pages ...
```

The GLB embeds all used images. The matching package keeps editable PNGs and a
manifest of source settings, seed, counts, occupied cells, UV rectangles and
physical extents. Map paths resolve beside the manifest; its mesh path resolves
to the neighboring GLB. Source configuration node names refer to the original
recipe, which should be retained for regeneration. Outputs are overwritten;
export is not atomic. Inspect the current manifest when copying a package after
changing its resolution or page count, since unreferenced old output files may
remain in the directory.

- Color is 8-bit sRGB RGBA. RGB includes source material factors and vertex colors;
  the baked material uses white factors to avoid multiplying them twice.
- Normals are 8-bit OpenGL tangent-space RGB. A sampled source normal is oriented
  toward the card front before encoding. The tangent convention follows TexUtil's
  triangle UV basis, not a promise of identical MikkTSpace results in every DCC.
- Packed data uses G=roughness, B=metallic, in linear space. Scalar factors are
  baked and reset to one on the destination material.
- OPAQUE and MASK source materials are supported. MASK holes expose underlying
  source geometry. JPEG source textures, BLEND materials and source subsurface
  scattering fail explicitly. Thin diffuse transmission is a uniform bake setting.
- Preserved wood retains its original mesh, UVs, material textures and wind data.
  Card vertices sample nearby source wind metadata. This is not an animated bake.
- Cards have unique atlas cells, including crossed planes. Do not randomize these
  cells or assign a different atlas afterward. UVs belong to this generated mesh.
  The retained wood still has reusable UVs, so the complete mesh is not a single
  unique bake atlas. Image V is converted exactly once during GLB export.

## Willow results and costs

Both profiles use the same seed-42 willow that originally has 810,482 triangles.

| Version | Retained wood | Foliage | Total | Reduction |
| --- | ---: | ---: | ---: | ---: |
| Original | 37,682 | 772,800 | 810,482 | 0% |
| CardBake, six segments | 37,682 | 14,400 | 52,082 | 93.6% |
| CardBake, three segments | 37,682 | 7,200 | 44,882 | 94.5% |

Each bake has 600 occupied paths and 1,200 cards. The shipped profiles use five
2048-square atlas pages per map, with 64x256 cells. Color, normal and packed data
at RGBA8 take 240 MiB across those pages before mipmaps or engine compression;
retained wood textures are additional. PNG/GLB file size is compressed storage,
not GPU memory. Lower cell dimensions or use a single plane for smaller texture
budgets. Simply enlarging a page does not reduce the occupied texel count.

Geometry savings do not translate directly to frame-rate savings. Large transparent
areas add overdraw and shadow work. The engine should use suitable texture
compression and alpha-coverage mip handling. Reducing the retained wood further
requires a separate wood topology pass.

## Reproduce the visual checks

A small study isolates a single strand:

```sh
./build/foliageutil out/samples/graphs/card-bake.json --out out/card-bake-study --json
./build/foliageutil out/samples/graphs/card-bake.json --card-bake Strand --out out/card-bake-study --json
```

That study replaces 1,240 source triangles with 24 card triangles. It uses larger
cells to inspect the material transfer at close range.

Render the actual GLBs using Blender and the same camera/lighting. The preview
scripts use 64 transparent bounces by default; the lower Cycles default can
terminate deeply layered cutouts as dark patches. This affects preview ray paths,
not the exported material or mesh.

```sh
blender --background --factory-startup --python-exit-code 1 --python tools/render_variants.py -- --models out/willow-lods/weeping-willow.glb out/willow-card-bake-final/willow-tree-card-baked.glb out/willow-card-bake-final/willow-tree-card-baked-low.glb --labels "Original: 810,482 tris" "CardBake: 52,082 tris" "CardBake low: 44,882 tris" --hdr ../TexUtil/assets/hdri/outdoor.hdr --out out/willow-card-bake-final/comparison.png --width 2000 --height 850 --samples 64
blender --background --factory-startup --python-exit-code 1 --python tools/render_blender.py -- --model out/willow-card-bake-final/willow-tree-card-baked.glb --out out/willow-card-bake-final/side.png --hdr ../TexUtil/assets/hdri/outdoor.hdr --samples 64 --size 1000 --light-scale 8 --elevation 8 --azimuth 110 --ground-height 0
```

The comparison's original GLB can be reproduced with `samples/graphs/weeping-willow.json`
and `--no-lods --no-growth --out out/willow-lods`. Automated tests cover deterministic
bakes, source color factors, UV orientation, normal convention, cutout holes,
depth visibility, unique cells, page spill, budgets, source-file protection and
CLI selection. PNG dimensions and resource limits are checked before allocation.

## Reuse a mature bake during growth

`prepareCardGrowth(graph, name, emptyDirectory, options)` bakes the mature plant
once and returns an immutable `CardGrowth` handle. `growCards(*prepared, progress,
limits)` evaluates the saved guides and retained wood, then emits the reached
portions of the fitted cards. It does not evaluate the source canopy, rasterize
textures or write files during growth updates, unless that canopy is also an
explicit dependency of the guides or retained wood. Keep the texture directory
alive while its materials are in use. Preparation fixes the graph, seed, LOD,
profile, fitted planes and atlas cells; create another handle to change them.

This mode requires saved `growth.mode:"developmental"`. It uses each guide's
support-triggered birth, visible length and radius maturation. Mature UVs stay
fixed while the strip extends along its guide; its width follows the guide's
radius fraction, including support maturity. The fitted tip overhang advances
with the tip. Every occupied path and crossed plane keeps its original unique
atlas cell. Progress one matches the ordinary mature bake's geometry and vertex
attributes. Empty and wood-only samples retain stable material slots, including
wood cap materials that may become exposed at intermediate stages. Handles can
be sampled concurrently and scrubbed backward.

This animates a captured mature shoot. It does not replay the source leaves'
individual `scale_profile`, flowering schedule or attachment delays. A moving
reveal can cut through leaf/flower silhouettes, and width growth deforms the
mature silhouette. Use smaller captured shoots or ordinary instanced foliage
when independent leaf/flower growth matters. Retained wood still regenerates,
including any solid unions, so measure update latency and Unity frame costs.

The current bindings are held in memory, not serialized for reloading from an
existing `.cardbake` manifest. The written mature GLB and PNGs remain ordinary
static assets. Unity exposes preparation and repeated growth through
`FoliageRecipe.PrepareCardGrowth` and `FoliageCardGrowth.Generate`; the component's
**Reuse Mature Card Bake** switch manages a private cache for its enabled lifetime.
See [Unity setup and timings](UNITY.md#reuse-mature-cards-for-live-growth).

## Packaged growth exports

See [Growth animation and stage packages](EXPORTS.md) for native Alembic export
and independently generated geometry/CardBake stages. Named profiles select
their own progress samples and optional LOD. Each CardBake stage captures its
currently grown source and visible guide paths; empty foliage can retain wood
alone. These profiles run explicitly with `--export NAME`.
