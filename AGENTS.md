# Working with FoliageUtil

FoliageUtil is a C++17 CPU utility that turns named JSON graphs into seeded foliage
meshes. Keep method and function signatures on one line. Avoid em dashes.

## Build and discover

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 8
ctest --test-dir build --output-on-failure
python3 tools/prepare_samples.py --texutil ../TexUtil/build/texutil
./build/foliageutil nodes --json
./build/foliageutil describe orient
./build/foliageutil validate out/samples/graphs/rose.json --json
./build/foliageutil out/samples/graphs/rose.json --out out/rose --seed 42 --json
```

Use the executable catalog as the source of truth for parameter names and bounds.
Read README.md, docs/NODES.md and the relevant example before authoring a recipe.
Do not invent species nodes. Trunks, paths, branches, leaves, petals, grains, cards
and instances compose into arbitrary plants. Root document fields are version,
seed, units, name, description, materials, nodes, outputs, lods, growth, card_bakes and exports. Unknown fields fail.

## Source layout and repository hygiene

Read samples/README.md and docs/REPOSITORY.md. Canonical foliage graphs live in
`samples/graphs/`; all TexUtil recipes and imported dependencies live in
`samples/materials/`, with generation jobs in `samples/manifest.json`. Run the
preparation command above before using textured examples. It needs a TexUtil
executable but no adjacent source checkout. Prepared `out/samples/` bundles are
portable and can generate foliage without TexUtil. Edit source recipes, not their
generated copies. Add missing imports to the sample package, never an absolute
path or an undocumented dependency on another checkout.

Keep generated textures, atlas manifests, GLB/OBJ/FBX models, Blender scenes,
reference photos and archives out of Git. The only publication-image exception
is the three named README/social graphics allowed by `.gitignore` in `docs/images/`.
Keep individual sample renders and close-ups under `out/`; link their reproduction
guides instead of checking in more images. Do not use `git add -f` to bypass
these rules. `out/` and `build*/` stay ignored. Use the cleanup tool's dry run before
removing regenerable artifacts; preserve latest final outputs, unknown archives,
source recipes and images needed by documentation. Do not rewrite Git history
or force-push as routine cleanup.

CTest uses explicitly labeled diagnostic fixtures in `build/test-samples/` and
removes each successful test's output directory. Failures retain artifacts. Those
fixture textures are not TexUtil renders and must not be used for visual QA.
Always use the real prepared sample bundle for renders and final exports.

## Author coherent plants

Use right-handed Y-up meters. Local prototype +Y is its growth direction and +Z
is its leaf/card face normal. Node names form references; object order is irrelevant.
Every node validates, but only output dependencies execute. Explicit node seeds
isolate that node from root seed changes. Renaming a stochastic node changes its
stream. Do not promise cross-platform bitwise equality.

`trunk` and `curve` make sampled paths. `branch` returns children only; connect
successive branch nodes for further generations. `grow` deforms each path from
its own base, so run it before adding children. `merge` only combines like types.
`tube` creates surfaces from paths; `scatter` creates placement frames; `instance`
flattens a prototype onto frames. Repeated plants share prototype shape.

For hanging foliage, use `grow.direction:[0,-1,0]`, `interpolation:"spherical"`
and `strength_profile` to control the shoulder and vertical tail. The profile
multiplies `strength` over the retained arc length; its default `[0,0]` to `[1,1]`
ramp preserves existing recipes. A plateau at one gives full attraction early.
Spherical interpolation handles opposite tangents with a deterministic bend plane.
Segment lengths and the base pivot are retained; ground and collision constraints
are not imposed. Deform boughs before adding sprays, deform sprays before adding
strands, then scatter leaves on those final strands. See docs/WILLOW.md and
samples/graphs/weeping-willow.json. Keep strand tubes visible to connect every leaf.

### Roots, trunk flare and profiles

Use `roots` with a skeleton `input` to generate spreading roots from each parent.
`attachment` selects a normalized arc-length position on that parent; roots start
on its centerline. `length` is horizontal reach, `surface_at` controls how quickly
the root descends, and `ground_height` is a world-Y plane. `bury_depth` places tip
centers below it; allow for the tip radius if the whole cap must be hidden. Roots
output children only. Tube them and merge their mesh with the trunk mesh. This
is flat-ground shaping, not terrain collision or automatic welded junctions.
Transform the completed plant afterward when possible. See docs/ROOTS.md and
samples/graphs/rooted-tree.json.

Profiles are 2..32 `[position, multiplier]` keys, strictly increasing from 0 to 1,
with linear interpolation. `radius_profile` multiplies taper on trunk, branch,
curve and roots. `trunk.flare_length` and `flare_power` control the basal fade.
`tube.ridge_profile` fades lobes, while `radius_noise`, `noise_scale`,
`noise_octaves` and `noise_profile` add coherent radial surface variation. Tube
profiles use normalized swept arc length; trunk/branch profiles use normalized
growth steps, and roots profiles use normalized horizontal reach. A zero surface
noise profile pins displacement at that location. Trunk/branch `noise_profile`
fades new directional perturbations, not the bends already accumulated. Increase
segments and sides when the surface noise or root curvature needs more geometry.

`instance.scale_jitter:[x,y,z]` adds seeded independent fractional size variation
around the prototype origin, before rotation. `card` supports height/width
subdivisions with curl, fold and twist. UVs remain attached during deformation.

### Atlas workflow

Use TexUtil's native **spritesheet output**, not a node named `spritesheet`.
Read its source recipe and TexUtil guidance before editing it. The bamboo and
lily-pad packages are self-contained examples:

```sh
../TexUtil/build/texutil samples/materials/bamboo/leaves.texutil.json --out out/bamboo-leaves --threads 8 --json
../TexUtil/build/texutil samples/materials/lily-pad/pads.texutil.json --out out/lily-pad-materials --threads 8 --json
```

Generate the portable package with `tools/prepare_samples.py`; its graphs live
under `out/samples/graphs/`, and maps under `out/samples/assets/`. Never copy generated
PNGs, `.atlas.json`, `.atlas.assets`, MaterialX or mesh outputs into source samples
or Git. For an independent export package, move its manifest and whole matching
image folder together and preserve relative paths.
The manifest defines occupied cells, image size, grid and internal padding.
Bind its matching color, normal and packed roughness PNGs on the prototype's
material; the `atlas` field alone does not assign textures or materials.

```json
{
  "op": "instance",
  "input": "pad_card",
  "points": "pad_sites",
  "rotation": [-90, 0, 0],
  "atlas": "../assets/lily-pad/pads.atlas.json",
  "atlas_mode": "random",
  "scale_jitter": [0.12, 0.05, 0.10]
}
```

`atlas_mode` supports `random`, `cycle` and `fixed`; `atlas_index` is zero-based
and must identify an occupied cell. One cell is applied to every vertex/material
in each copied prototype, including crossed planes. Keep prototype UVs within
0..1 and bind compatible atlas layouts on all textured parts of that prototype.
Use `card.uv_rect` only for a deliberate subregion inside each cell. Set physical
card width/height yourself; atlas selection does not infer the card's aspect ratio.

Do not manually flip V or duplicate grid/padding values in foliage nodes. The
reader converts TexUtil's top-left image coordinates, and GLB applies its existing
single V conversion. Atlas randomness has an independent stream from size/color;
root and explicit node seeds follow the normal graph rules. These UVs repeat
across copies and are not a unique bake atlas. See docs/SPRITESHEETS.md.

For silhouettes, use an actual RGBA atlas and `alpha_mode:"MASK"` on the material.
Curved cards still have rectangular triangles under that stencil. Keep visible
leaf/card bases at the attachment pivot; transparent sprite margins can make a
correctly placed card look detached. Geometry leaves such as bamboo can use an
opaque atlas. Translucency is separate from stencil transparency.

For flowering canopies, see `samples/graphs/sakura.json` and
`samples/materials/sakura/README.md`. The sprig atlas has six flowers and visible
supporting twigs per cell; these are textured cards, not individual geometry
petals. The `FloweringShoots` CardBake profile captures the final generated canopy
along its twig guides. Preserve its attachment bases and inspect multiple views.

### Placement and attachment

To enforce closeness to actual supporting geometry, set `scatter.proximity_mesh`
to a mesh node and `max_distance` in meters. Candidates farther from its triangle
surface are rejected, with no retries. `snap_to_mesh:true` moves accepted pivots
onto that surface while retaining their frame, scale and phase. Checks happen
after scatter offset; count is the number attempted, not a guaranteed result.
Inspect the JSON node report's `proximity` counts. Distances use the current graph
coordinate system, so check before instancing the complete plant into a grove.

Use branch/twig tubes as the support, not the leaves being generated, which would
create a dependency cycle. The query measures distance to triangles, not nearest
vertices, volume containment, alpha coverage or the final leaf silhouette. A leaf
prototype must have its base at the pivot. Retain visible supporting twigs or a
short sheath; proximity alone cannot fix a transparent margin or disconnected
prototype. See docs/SCATTER_PROXIMITY.md and samples/graphs/bamboo.json.

For mesh-facing scatter, set `normal_direction:[x,y,z]` and
`max_surface_angle` in degrees. Candidates pass when their interpolated surface
normal is within that cone. The direction uses current graph coordinates and must
be nonzero. This selects surfaces before offset, tilt and proximity; it does not
aim geometry. `scatter.angle:0` makes prototype +Y follow the surface normal.
Count is attempts with no retries. See docs/SUNFLOWER.md.

`leaf.width_profile` can override the shape outline with full-width multipliers.
Use at least three keys, zero widths at both ends and positive interior widths.
`lateral_bend` shifts the tip sideways in multiples of length; `edge_wave` and
`edge_frequency` ripple the edge. All retain base pivots and UVs. Use enough
segments to resolve the profile/ripples, and recheck silhouette at each LOD.

For roses and rosettes, use `radial` with `mode:spiral`; control start/end radius,
height, tilt and scale. `leaf` with `shape:petal` supports curl, fold, twist and
width subdivisions. The examples are starting points, not a list of allowed species.

For cards, set `card.planes` for crossed geometry. Apply `orient` to placement
points before `instance` to face outward/inward from a center, face a fixed direction,
or keep the original frame. It aims prototype +Z. `horizontal:true` keeps radial
facing perpendicular to `up`; `rotation_jitter:[x,y,z]` adds seeded +/- local angles
in degrees. Zero jitter means exact facing. Controls apply to meshes beyond cards.

Keep curvature and segment density appropriate to asset scale. Foliage must remain
connected visually. `merge` leaves tube junctions overlapping. Pass merged closed
wood through `solidify` to union trunk, roots and branches into one solid, then
merge leaves/cards afterward. `require_connected:true` rejects detached shells.
`weld_tolerance` is numerical seam tolerance, not gap filling; `crease_angle` only
changes normal smoothing. It preserves surviving UV/material/color/wind data.
Read docs/SOLIDS.md for closed-input requirements, budgets and precision limits.
Use prune for whole-stem selection, not a boolean cutter. Wind is shader metadata,
not an animated simulation. Do not describe a low-poly example as photorealistic.

## Materials and export

Use GLB for self-contained PBR materials, alpha-cutout textures, UVs, vertex colors
and _WIND metadata. OBJ/MTL loses some of these properties. Default materials are
bark, cut and leaf. Declare custom material names. Material numeric RGBA is linear;
base-color textures are sRGB. Normal maps use OpenGL convention. Metallic/roughness
textures pack G=roughness, B=metallic, multiplied by the scalar factors.

Query `foliageutil materials --json` for the material schema. Saved `translucency`
and `subsurface` controls use GLB `extras.foliageutil` and the Blender preview adapter.
Do not promise their appearance in other viewers without an adapter. Alpha coverage
defines stencil holes separately from diffuse transmission through the tissue.

UVs intentionally repeat between leaves and branches. Never call them a unique
bake atlas. Use TexUtil xatlas and bake tools when a unique atlas is requested,
noting that their OBJ conversion drops colors and custom wind attributes. Read
TexUtil's own guidance before changing its graphs or code. It remains independent.

Output files go under out/ unless otherwise requested. Exports overwrite outputs,
but may not overwrite imported assets. Keep texture references portable and preserve
relative paths. PNG/JPEG bytes embed in GLB; cards require an actual alpha texture
for cutout silhouettes. The optional TexUtil preview helper shows solid materials
and therefore does not verify cutout behavior or wind. Inspect exported GLB material
and texture data when testing those properties.

## Packed foliage cards

Use `ribbon` to sweep an entire leafy-spray atlas along skeleton paths. `width`
is meters, `stride` controls sampled path rows, and `planes` creates crossed strips.
Positive `width_profile` values shape strip width. `rotation`/`rotation_jitter`
control roll, `density` keeps nested seeded subsets, and one atlas cell is used
per path across all its planes. Bind real RGBA maps on a double-sided MASK material.
Keep the twig visible at the base center. Stage paths before ribbons for growth.

See docs/PACKED_CARDS.md, samples/materials/willow/packed/README.md and
samples/graphs/willow-tree-card-packed.json. The original willow also offers `--lod Cards`.
This keeps its welded wood and replaces individual leaves/twig tubes with packed
strips. Source atlases are authored in TexUtil, not automatically baked from
arbitrary input meshes. Ribbons are rectangular triangles under an alpha stencil;
proximity does not see that stencil. Profile lower LODs in the target engine:
alpha overdraw and edge-on views remain costs even with far fewer triangles.
LOD density scales ribbon density; tube_stride also scales ribbon stride.

## CardBake export

Use saved root `card_bakes` profiles and `--card-bake NAME` to fit long cards to
skeleton strands and capture their generated foliage. Query `foliageutil card-bakes
--json`; read docs/CARD_BAKE.md and samples/graphs/card-bake.json. The willow offers
`Strands` and `StrandsLow` profiles. `exportCardBake` is the library API. Profiles
validate even when unused; ordinary exports do not bake automatically. Standalone `--card-bake`
selects the mature base graph and cannot combine with LOD/growth CLI flags.
The library API accepts `Options.growth` and selected LOD graphs; named stage
export profiles use that path to bake the currently grown source and visible guides.

```sh
python3 tools/prepare_samples.py --texutil ../TexUtil/build/texutil
./build/foliageutil out/samples/graphs/weeping-willow.json --card-bake Strands --out out/willow-baked --json
./build/foliageutil out/samples/graphs/weeping-willow.json --card-bake StrandsLow --out out/willow-baked --json
```

To change the preset, edit `samples/graphs/weeping-willow.json` and prepare again.
Its generated GLB and `.cardbake` directory belong only under `out/`. Keep the
profile in source JSON; no baked PNG, atlas or model needs to be committed.

`source` is a mesh of leaves and small twigs only, `paths` is the matching skeleton,
and optional `keep` retains the wood mesh unchanged. Use matching coordinates.
Source connected components attach to the closest guide at their lowest indexed
vertex, so prototype vertex order must preserve its attachment base. Unmatched
components fail using `max_distance`; no source foliage is silently dropped.
Whole connected arbitrary canopies need suitable component separation first.

Each occupied path gets one or two fitted planes, with `segments` along its length.
Each plane gets a unique cell, including crossed planes. Fit and bake are seeded
through the source graph; do not assign these cells randomly through `instance`.
Resolution, page size, margin, alpha cutoff, padding and supersampling are controls.
PNG color/alpha, OpenGL normals and packed roughness/metalness are embedded in GLB;
source color factors and vertex colors are baked once. OPAQUE and MASK sources
are supported. JPEG, BLEND and source subsurface materials fail explicitly.
The uniform `translucency` setting uses the existing preview adapter.

This is curved strand projection, not an arbitrary cage-ray baker or a whole-tree
billboard. It retains frontmost projected details, not full 3D depth/back surfaces.
Keep the GLB and matching `.cardbake` package/manifest for inspection. PNG names
resolve beside the manifest; source recipe configuration paths retain their source
context. Atlas memory and alpha overdraw remain costs. For Blender foliage
comparisons use enough `--transparent-bounces` (preview default 64), or deeply
layered transparent card regions can terminate dark. Render at least two angles.

## LOD and growth exports

Saved root `lods` profiles export automatically from the CLI. Discover the schema
with `foliageutil lods --json`; see docs/LOD.md. Use `density`, `topology`,
`tube_stride` and per-node `overrides`. Density selects nested seeded instance
subsets; surviving atlas cells/variation stay stable when inputs are unchanged.
Pin meshes used for scatter/proximity and preserve tube resolution needed for
solid junctions. Override whole-plant density when only leaves should be reduced.
This is graph resampling, not arbitrary mesh decimation or texture resizing.
`screen_height` is manifest metadata, not runtime switching. `--no-lods` skips
levels and `--lod NAME` selects one. `generate`/`write` remain base-only;
`exportGraph` writes configured batches with portable manifests.

Saved root `growth` contains `steps` (2..32), optional `lods` (default false),
and `stages` keyed by actual node names. Each stage has start/end in 0..1 and
linear or smoothstep easing. In default `mode:"scale"`, skeleton stages scale each stem at its own base;
instance stages grow each leaf/pod/cluster at its attachment; ordinary mesh stages
use a configurable pivot. Stages compound along dependencies. Schedule the trunk
before branches, then leaves. Unstaged nodes evaluate normally. See docs/GROWTH.md,
samples/graphs/growth-tree.json and samples/graphs/sunflower.json. Query `foliageutil growth
--json`. The existing `grow` operator controls path shape separately.

CLI growth snapshots export automatically. `--no-growth` keeps mature assets;
`--growth-step N` selects one zero-based step. `Options.growth` supplies continuous
progress to the library. Empty step outputs are manifest entries with no mesh
file; exact stale generated mesh/MTL paths are removed on overwrite. Progress one
matches mature geometry. These are independent meshes, not morph targets or
botanical simulation. Check intermediate attachments, root/ground positions and
solid unions. At most 128 variants export sequentially with per-variant budgets.

Use `growth.mode:"developmental"` for overlapping extension and thickening.
Skeleton stages take separate monotone `length_profile` and `radius_profile`
channels over local age, plus `tip_length`/`tip_radius` for young tip taper.
Branches and roots wait for the parent to reach their attachment. Their radius
fractions are capped by support maturity rather than multiplied through every
generation. Full mature guide paths stay in `Stem.samples`; tube/ribbon outputs
reveal the reached prefix. Stage the creation path, then deform its guide with
`grow` before attaching children. The default `mode:"scale"` retains old behavior.

Instance `scale_profile` supports baby leaves, pods and flowering sprigs at each
attachment. `attachment_delay` is a fraction of the time remaining to `end`, not
a distance or an absolute delay. Birth is the later of stage start and support
arrival, followed by that delay. Give the parent length channel enough time to
reach all child attachments before their end times. Unstaged offspring inherit
automatic development; explicit point and instance stages compose deliberately.
Skeleton scatter with proximity keeps mature candidate membership and gates
visibility against current support, preserving atlas/variation streams. The
cached mature support query counts against the same generation budgets.

For the sakura animation, use `samples/graphs/sakura-growth.json` and read
`docs/SAKURA.md#growth-animation`. Its stages overlap; trunk radius continues to
increase while lower branches and small flowering sprigs emerge. See
`samples/graphs/development-tree.json` for baby leaf instances. Build the optional
`foliage_growth_frame` target and run `foliage_growth_frame RECIPE --check-sequence 192`
before rendering to check every continuously sampled mesh without repeated GLBs.
`tools/render_growth_animation.py` shares textures and saves keyed visibility in
Blender. Resume checks the recipe, binary, source scene and renderer-script hashes.
Check changed timing and frame counts, especially near birth/profile boundaries;
retain the same fully grown geometry. Keep all animation files under out/.

## Named animation and stage exports

Read docs/EXPORTS.md and query `foliageutil exports --json`. Root `exports`
profiles are opt-in through `--export NAME`, with `type:"alembic"` or
`type:"stages"`, a relative package `output` directory and mesh-node `source`.
Profiles require saved `growth` and validate even when unused. Do not combine
`--export` with the old LOD/growth/CardBake CLI selection flags. Library callers
use `exportProfile` on a base graph without setting `Options.growth`.

Alembic profiles take `frames`, `fps`, `start`, `end` and optional `lod`. The
native writer needs no Blender; build with CMake 3.29+ and `FOLIAGE_ALEMBIC=ON`
(default). OFF retains stage exports. Packages include animation.abc, material
bindings, portable texture files and a manifest. Keep these together. The cache
has variable topology and zero-based sample times; it is not a morph animation.
Use tools/import_growth_alembic.py for Blender materials and reliable empty-frame
visibility during backward scrubbing. Verify archive attributes and actual
Blender imports before claiming success; do not imply Unity has been tested
without running it. Unity shaders need material remapping and tissue optics.

Stage profiles take `steps`, default `geometry:"mesh"` or `"card_bake"`, and a
saved `card_bake` name. `stage_overrides` maps zero-based indices to representation,
bake profile or LOD overrides. Empty steps have no mesh; early CardBake stages
can export retained wood alone. Later stages fit/bake currently grown foliage,
with an independent atlas per stage. Inspect intermediate attachments and at
least two CardBake viewing angles. Samples growth-exports.json and
sakura-growth.json include both export types; Sakura also offers MixedStages.

Keep renders and Blender scenes beside, not inside, managed export packages.
Re-export replaces an owned package atomically; added/missing files cause refusal
except disposable Finder `.DS_Store` metadata.
Failed new exports preserve the previous completed package. All ABC, GLB, PNG,
video and scene outputs remain ignored. `max_samples_mb` is a cumulative mesh
record estimate, not a RAM cap; `max_output_mb` checks package bytes during export
and before publication. Respect per-frame geometry and per-bake memory budgets.

## Unity and native hosts

Read docs/UNITY.md. Build the shared C ABI with `FOLIAGE_NATIVE_PLUGIN=ON`;
`tools/build_unity_package.py` assembles the UPM package and pinned static PNG/zlib
dependencies under out/. Use macOS `--arch universal`, or build `--arch x64` on
Windows/Linux. One source/wrapper needs separate OS binaries. Do not commit
native plugins, Unity caches or generated recipe bundles. Never relabel an
architecture or claim Windows/Linux/IL2CPP verification from a macOS editor test.

Use `tools/package_unity_recipe.py` on a prepared graph to copy the actual texture,
atlas and imported-mesh dependencies into a portable StreamingAssets bundle.
The runtime native API is include/foliage/c_api.h. Keep buffer layouts, material
slots, UTF-8 ownership and exception handling in sync with FoliageRecipe.cs.
Native views are borrowed until mesh destruction; managed snapshots own copies.
Keep the C# mirror-X/winding conversion in one place. Test actual Unity culling.

FoliageGenerator evaluates one request at a time on a worker and coalesces changes.
Tangents and bounds are also computed there; Unity mesh/material operations stay
on the main thread. Preserve the separate worker/apply/initial-load timings. Do
not convert worker milliseconds into an FPS prediction. Profile actual player
frames and initial texture loading separately from repeated geometry updates.

A nonempty CardBakeProfile selects fitted cards at the requested growth and LOD.
`FoliageRecipe.BakeCards` and C++ `bakeCards` return mesh/material data while writing
the profile's texture package. The C ABI requires an empty caller-owned output.
The component uses private temporary cache directories and removes old ones after
their textures load. Never clean user directories as part of its lifecycle.
Save current mesh as prefab persists mesh/materials and imported textures under a
new Assets folder, with no generator on the prefab. Preserve existing folders;
keep raw normal maps linear and use MASK coverage mip settings.
Generated live meshes, materials and textures must remain `HideFlags.DontSave`;
prefab export clears those flags on persistent clones. Verify that saving a live
scene does not embed generated mesh or texture data. The demo scripts under
`unity/demo/` add bamboo beside an existing Sakura and provide shared or individual
growth playback. Preserve existing scene placements/settings when updating it.
The bamboo recipe uses developmental stages for each `*_culm`, `*_branches`,
`*_branchlets` and `*_leaves`; select `SourceNode="wind"` for the complete grove,
since its first alphabetically sorted output is an individual culm study.
Reuse finished bakes; shared packed-ribbon atlases are preferable when regenerating
geometry frequently. Runtime PNG loading has no platform texture compression or
alpha-coverage mip guarantee. Preserve bake alpha/material slot bindings.

For cached fitted-card growth, enable `ReuseMatureCardBake` on a developmental
recipe. `PrepareCardGrowth` bakes mature textures once; `FoliageCardGrowth.Generate`
updates guides, retained wood and card prefixes with fixed atlas bindings. Keep
preparation timing separate from repeated build/apply timings. The cache is owned
by the enabled component and invalidated by recipe, seed, LOD, profile or budget
changes. It approximates shoot growth and does not replay individual source leaf
stages. Bindings are currently in memory, not reloadable from a baked manifest.
Test texture identity, backward/empty growth and mature equality with
`tools/test_unity.py --card-bake PROFILE --reuse-mature-bake` in an isolated project.

Run CTest with native_api, the real .NET managed smoke, and tests/unity through a
licensed editor using tools/test_unity.py. Use separate projects under out/;
never modify an unrelated Unity project for verification. Render prepared TexUtil
maps in Built-in/URP and test partial/empty growth, culling and CardBake. The
included shader has thin translucency; HDRP, volumetric SSS and animated wind
require further adapters. A missing player backend is a limitation to report,
not a passing test. Keep Unity source under unity/ and test scripts under tests/.

## Maintain and verify

The library API is include/foliage/foliage.hpp. Keep catalog validation, type
inference and evaluator behavior in agreement. Add meaningful tests for seeded
behavior, curve attachment, normals/winding, degenerate geometry, UVs, bounded
replication and export contracts when changing those areas. Geometry budgets must
be charged before bulk allocation. Validate all nodes, including unused ones.

After catalog changes run `python3 tools/update_docs.py`. Keep generated reference
docs in sync. Run the CTest suite and test affected GLBs with the optional Khronos
validator (see docs/VALIDATION.md). For visible shape changes, render actual exports
with tools/preview.py or tools/gallery.py through the existing TexUtil Filament
build. Use the local GPU as needed; do not substitute an invented image for proof
of a geometry change. Do not claim a test passed when its runtime was unavailable.

## Publication graphics

Read docs/SOCIAL.md before refreshing README or social images. The editable
layouts are tools/build_publication_graphics.cjs; tools/publication_layers.json
lists the actual packed Blender scenes used by tools/render_publication_cutouts.py.
Keep real exported geometry and native TexUtil maps in feature illustrations.
Recompute triangle labels if their source graphs change. For this publication set,
only the two README PNGs and social JPEG belong in docs/images; layers, SVGs with
embedded images and packed scenes stay in out/. Preserve the small white Util, low rose, and ground-free
willow/right-side-plant social composition unless the user changes that direction.

## Rocks, crystals and carving

Read docs/MINERALS.md and query `describe rock`, `describe crystal` and
`describe boolean`. Rock size is nominal full XYZ extent about the origin before
weathering. Crystal profiles create polygon rings from Y=0 to height; only endpoint
radii may be zero. Use seeds for independent primitives, and instance for shared
shape with placement/size variation. LOD overrides control rock subdivisions and
crystal sides explicitly. Use solidify on raw overlapping shells, then boolean
for union/difference/intersection of solids, including cavities. Cut faces retain
tool materials; Boolean output normals must agree with final winding.

GLB transmission/IOR/volume controls are separate from foliage translucency and
alpha coverage. The Unity foliage shader does not render gemstone refraction;
retain metadata for a compatible adapter. CardBake rejects refractive sources.
Keep example geometry stylized unless actual appearance verification supports more.
