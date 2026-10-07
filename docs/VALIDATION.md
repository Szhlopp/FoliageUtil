# Validation

## Reusable mature CardBake growth, October 1, 2026

The native plugin Release configuration passes **40/40 CTest cases**. The main
Alembic-enabled Release configuration passes **39/39** (without the native plugin
test). The affected native API, reusable CardBake, ordinary CardBake and
developmental growth tests also pass under UndefinedBehaviorSanitizer, with no
diagnostics. The actual .NET 8 PInvoke smoke passes using prepared TexUtil maps.

The new C++ test checks 193 growth samples, independent support-triggered births,
empty/wood-only states, exact mature geometry/UV/normal/color/wind equivalence,
multiple atlas pages and crossed cells, deterministic backward/concurrent sampling,
seed and LOD bindings, immutable package bytes/timestamps, invalid arguments and
generation budgets. Update reports exclude the detailed source canopy. Native
and managed lifetime tests verify that prepared handles outlive their recipes.

Real Unity 6000.4.6f1/macOS ARM64 tests pass in Built-in and URP 17.4.0. Each uses
the full Sakura, checks 33 forward/backward samples, retains the same GPU textures
and materials, compares mature buffers with a direct bake, verifies seed-triggered
cache replacement and disable cleanup, and saves a live scene without generated
buffers. Four growth stages were rendered from two angles using real TexUtil maps;
partial and mature views were inspected. The reference GLB has **zero Khronos
validation errors**, with eight expected runtime tangent-generation warnings.
An additional full Sakura Built-in run verifies that recipe reload retains the
displayed materials until its replacement bake is ready. Its three preparations
are the initial seed, a changed seed and an explicit reload. The macOS universal
package contains ARM64 and Intel code; native API and .NET smoke checks exercise
its ARM64 slice on this host.

Repeated mature worker updates measured about 203..207 ms, with 0.14..0.16 ms
main-thread apply. Preparation remained about 11 seconds and initial baked texture
loading about one second. The retained wood union accounts for about 174 ms of
the remaining mature native work. See [Unity measurements](UNITY.md#reuse-mature-cards-for-live-growth)
and the reproduction command there. Local evidence is under
`out/unity-card-growth-test/Artifacts/` and
`out/unity-card-growth-urp-test/Artifacts/`, with the reload check under
`out/unity-card-growth-final-test/Artifacts/`. This pass does not establish player
frame rates, Windows/Linux execution, Intel execution or IL2CPP compatibility.

## Original baseline

Validated on macOS ARM64 with Apple Clang 17, CMake/Ninja, September 10, 2026.

| Check | Result |
| --- | --- |
| Release build and CTest | 14/14 tests pass |
| Core assertions | 1,871 checks pass |
| UndefinedBehaviorSanitizer build and CTest | 14/14 tests pass, no diagnostics |
| Clean standalone configure/build | Passes with its own pinned JSON download; no TexUtil source dependency |
| Installed executable and asset layout | Installed outward-card sample generates GLB and OBJ with its relative texture |
| Khronos glTF Validator | All 12 example GLBs: 0 errors, 0 warnings |
| TexUtil model import / UV inspection | All 12 GLBs: valid preview UVs, no missing/nonfinite UVs or degenerate geometry |
| Filament visual inspection | Ten plant/example renders and the three-way card-orientation comparison inspected |

The validator emits informational unused-UV notices for untextured material
primitives. This is expected: UVs are supplied for future texturing. Reused foliage
UVs overlap and tube V may tile outside 0..1, so TexUtil correctly reports these
models as unsuitable for unique-atlas baking before unwrapping.

AddressSanitizer was attempted but its runtime stalled before entering the program,
even for `--help`, with and without sandbox restrictions. The startup-only probe
reported `AddressSanitizer: libc interceptors initialized` before timing out. Those
processes were stopped. No AddressSanitizer pass is claimed; UBSan ran successfully.
Windows/Linux builds and engine-specific imports are not validated.

## Reproduce

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 8
ctest --test-dir build --output-on-failure

npm install --prefix build/validation --no-audit --no-fund gltf-validator@2.0.0-dev.3.10
python3 tools/prepare_samples.py --texutil ../TexUtil/build/texutil
./build/foliageutil out/samples/graphs/card-bake.json --card-bake Strand --out out/card-bake-check
node tools/validate_glb.cjs out/card-bake-check/*.glb

python3 tools/gallery.py --texutil ../TexUtil/build/texutil
python3 tools/preview.py out/samples/graphs/card-orientations.json --texutil ../TexUtil/build/texutil --out out/card-orientations-preview --elevation 28 --azimuth 10 --size 960
```

The core tests cover tube winding/volume and seam continuity, leaf/ellipsoid normals,
card atlas regions, structural LOD, growth truncation and identity, pruning, seeded
repeatability and isolation, output sharing, unused-node behavior, graph errors,
bounded replication, OBJ negative indices and polygon import, input asset protection,
GLB structure/materials/attributes, embedded alpha textures and OBJ texture copies.

Orientation tests verify outward and inward face normals, upright horizontal
alignment, fixed directions, bounded seeded yaw variation, stable pivot fallbacks,
and invalid axes/ranges. Surface scattering checks positions against its source
plane and checks the sample mean for approximate area-uniform placement.

The orientation image intentionally uses opaque planes to expose geometry. The
canopy card examples export alpha MASK materials with an embedded PNG; material
structure, image embedding and source-byte preservation are verified. The TexUtil
preview shader does not evaluate alpha cutouts or wind, so its render is not a
visual validation of those engine shader features.

## Rose and Blender update

The inward-fold correction and new `rose-detailed` example pass the expanded
15-test Release CTest suite. Both rose GLBs have zero Khronos validation errors;
the detailed example emits four runtime-tangent-generation warnings, expected for
its normal-mapped primitives. Blender 5.2.1 LTS imported the embedded maps with
sRGB color / Non-Color data settings and rendered both close-up and full-plant
views in Cycles CPU, 96/128 samples respectively, at 1000 × 1000. Meshes and all texture/HDR resources
were saved into packed `.blend` files. See [ROSE.md](ROSE.md). The earlier UBSan
result above covers the original code; this update changes recipes and adds an
optional Blender script, not the C++ generator.

After strengthening the petal maps, both rose example tests still pass and the
detailed GLB still has zero errors and four tangent-generation warnings. Blender
UV inspection found an active UV layer, 0–1 petal coordinates and zero degenerate
UV triangles across 49,920 petal triangles. A checker render confirms the mapping,
including expected compression near tapered petal ends.

## Card geometry and sprite atlas update

The card/atlas milestone passed **17/17 tests**, including **7,092 core checks**.
Tests cover segmented/crossed cards, anchored base and center pivots, deformed
normals/winding, independent size bounds, preserved UVs and seeded repeatability.
Atlas tests cover cycling/fixed/random selection, partial grids, seed isolation,
unchanged size/color streams, PNG dimension checks, invalid manifests, input
protection and the single V flip in exported GLB coordinates.

All **15 example GLBs** pass Khronos validation with **zero errors**. There are
five runtime-tangent warnings: four for the detailed rose and one for the new
normal-mapped spritesheet material. Blender renders verify actual alpha cutouts,
cell selection and aligned maps. Filament renders verify the geometry with the
existing solid-color helper. The earlier UBSan baseline does not cover these new
card/atlas code changes.

TexUtil's expanded **20-test suite** also passes, including native spritesheet
tests for rectangular grids, padding extrusion, synchronized data/color channels,
seed and thread repeatability, float-buffer budgets and source-asset protection.

## Saved petal materials and completed rose atlas

The current Release suite passes **18/18 tests** with **7,114 core checks**. The
additional coverage verifies material bounds (including unused materials), saved
optical metadata, unchanged alpha coverage and texture layout, and re-parsing a
normalized graph. Integer catalog defaults now retain integer JSON types so a
normalized document can be parsed again. The material schema is discoverable via
`foliageutil materials --json` and the library's `materialFields()` API.

All **16 example GLBs** pass Khronos validation with **zero errors** and **six
runtime tangent-generation warnings**: four for the rose, one for the single petal,
and one for the leaf sprite sample. Non-power-of-two atlas dimensions and unused
UVs produce informational messages. These are separate from errors and warnings.

The current detailed rose uses twelve aligned petal sprite variants, alpha stencils
on inward-curved cards, and saved optical settings. The exported model has 60,500
triangles. Blender 5.2.1 LTS renders were inspected at 1000 pixels, 128 samples for
the bloom and 96 samples for the full plant. The single-petal study uses 800 pixels
and 64 samples with studio and backlit presets. See [ROSE.md](ROSE.md).

The optional Blender material regression checks that alpha coverage wraps both
the reflection and diffuse transmission shaders, imported pigment and normal links
remain shared, saved subsurface values are applied, and a zero CLI override disables
them. It passed in Blender 5.2.1 LTS. A matched opaque-control render confirmed
the backlighting response comes from the tissue settings. The regression requires
Blender with Python support and the generated single-petal GLB:

```sh
./build/foliageutil out/samples/graphs/petal-study.json --out out/petal-study
blender --background --factory-startup --python-exit-code 1 --python tests/blender_materials.py -- out/petal-study/petal-study.glb
```

The optical settings use custom GLB extras and the supplied Blender adapter. No
standard glTF optical extension, Filament tissue shader, thickness map or GUI editor
is claimed. The previous sanitizer results remain baseline-only.

## Birch tree example

The current Release suite passes **19/19 tests**, including the seeded birch tree.
Its GLB has **295,300 triangles** and **13,800 leaf cards**. Same-seed regeneration
is byte-identical; seed 43 changes the export. Khronos validation reports zero
errors and three runtime tangent-generation warnings. The new leaf atlas and bark
wrapper were generated with TexUtil and inspected; bark normal conversion is
explicitly OpenGL, matching FoliageUtil's material contract.

Actual Blender renders check the full tree, bark detail and leaf detail, and a
TexUtil Filament preview checks the solid geometry. The Blender helper now accepts
an explicit `--light-scale` for large assets and chooses camera distance from the
model bounds to avoid clipping trees. Existing light-rig scale defaults to one.
See [BIRCH.md](BIRCH.md) for reproduction commands and interpretation limits.

## Wheat and README banner update

Release CTest remains **19/19 passing** after upgrading the wheat graph. Its patch,
single stalk and isolated head exports all have zero Khronos validation errors,
with three, three and two runtime tangent-generation warnings respectively.
Same-seed GLB regeneration is byte-identical; seed 43 changes the mesh. Actual
Blender full-patch and ear close-ups and a Filament geometry preview were inspected.
The preview helper prefers the filename matching the recipe when multiple GLB
outputs exist, keeping the gallery focused on the whole wheat patch.

The README banner is rendered from the actual three GLB examples with exact text
and independent presentation scales. The 1280 by 640 export and its source workflow
are documented in [SOCIAL.md](SOCIAL.md). It does not replace the individual asset
renders or imply equal botanical scale between those models.

## Bamboo, lily pads and proximity scattering

The Release and UndefinedBehaviorSanitizer builds both pass **20/20 tests**,
including **8,897 core checks**. Proximity coverage compares accepted and snapped
points against independent surface-distance calculations for faces, edges,
corners and a hierarchy of separated meshes. It also checks random-stream
preservation, zero-distance surface points, offsets, empty supports, no-retry
behavior, attempted-point budgets, invalid references/types/cycles, unused-node
validation and JSON acceptance/rejection diagnostics.

The final bamboo grove has 548,184 triangles and 365,673 vertices. Its three
prototype scatter nodes accept and snap all 320, 240 and 400 leaf bases with a
2 mm limit. Seven complete culms contain 2,240 leaves. Short leaf sheaths and
thicker branchlets make their support visible in the Blender attachment close-up.
The lily pad patch has 14,896 triangles, including submerged petioles; surface-only
exports preserve the six-variant RGBA stencil atlas.

The three bamboo and four lily pad GLBs have **zero Khronos validation errors**.
Tangent-generation warnings number three on the bamboo grove/complete culm, one
on its bare stem, and one on each lily pad export. The lily pad material was
checked for double-sided MASK coverage, an embedded 1536 by 1024 RGBA atlas,
matching normal/roughness maps and saved translucent tissue metadata. Seed 42
repeats byte-for-byte on this build and seed 43 changes each full plant export.

Actual Blender 5.2.1 LTS renders verify the final foliage, leaf attachments and
pad stencils; Filament solid-material renders check the geometry separately.
The README banner is regenerated at 1280 by 640 from the birch, bamboo, rose and
wheat exports. Its Python source and packed scene reproduce the layout.
The revised layout arranges the plants on the right with their bases cropped at
the bottom edge and the rose bloom low in the foreground. Ground, water and the
lily pad are absent. The title uses small white superscript `Util`. The banner manifest
records placed mesh bounds, title styling and the zero-degree camera elevation.
The updated AGENTS.md explains atlas generation, map binding, internal padding,
UV conversion, seed behavior and proximity checks. TexUtil's existing AGENTS.md
already documents its native spritesheet output and cell-size contract.

## Roots and fading profiles

On September 11, 2026, Release and UndefinedBehaviorSanitizer builds each pass
**21/21 tests**, including **12,051 core checks**. New coverage verifies root
attachment and ground descent, buried tip rings, root replication and budgets,
normals and winding, profile interpolation and malformed/unused profiles,
explicit seed isolation, radial displacement bounds, pinned end rings, seam
continuity and consistent spatial noise at retained straight-path LOD samples.

The initial untextured rooted-tree study had **zero errors and zero warnings** from the Khronos
validator. Their untextured materials produce informational unused-UV notices.
Seed 42 repeats byte-for-byte on this build; seed 43 changes all four outputs.
Actual Filament and Blender renders check the mesh. The Blender ground plane is
presentation geometry, separate from the GLBs. The example uses neutral materials
to show shape and overlapping junctions. See [ROOTS.md](ROOTS.md).


## Solid wood and textured rooted tree

Release and UBSan each pass **22/22 CTest cases** after adding `solidify` and the
curved banner-tree example. The core executable reports **12,648 checks**. Added
coverage verifies closed geometric edge incidence and opposite winding, analytic
union volumes for overlapping/duplicate/nested/disconnected solids, preserved
materials, UVs, colors and wind weights, deterministic output, invalid open inputs,
unused-node validation, and source/shell/global geometry limits. Boolean results
are rounded and simplified in the solid kernel before float serialization; mesh
area validation uses double intermediates to avoid false degeneracy from float
subtraction.

An independent reader checked serialized GLB positions and triangle indices,
joining only exact duplicated positions across property/material seams:

| Export | Triangles | Connected bodies | Open/nonmanifold edges | Degenerate faces | Volume (m³) |
| --- | ---: | ---: | ---: | ---: | ---: |
| Seed-42 solid wood | 138,420 | 1 | 0 | 0 | 19.30825 |
| Seed-42 trunk and roots | 41,446 | 1 | 0 | 0 | 14.57649 |
| Banner wood | 72,606 | 1 | 0 | 0 | 9.20094 |
| Seed-43 solid wood | 139,158 | 1 | 0 | 0 | 18.69895 |

All rooted-tree GLBs repeat byte-for-byte with seed 42 and change with seed 43 on
this build. The whole tree, solid wood, solid base, banner tree and banner wood
have zero Khronos validation errors, each with the existing generated-tangent-space
warning because normal-mapped primitives omit explicit tangents. Untextured slots
also produce informational unused-UV notices.

TexUtil generated the packaged 2048-pixel bark maps and MaterialX from the saved
recipe. The native sheet was inspected, including its 2×2 height repeat. Blender
5.2.1 LTS rendered the actual textured GLBs at 1400 pixels and 96 samples, with
packed scenes and manifests under `out/rooted-tree`. The ground is a separate
presentation plane. Fine relief uses the embedded normal map, not displacement.
UV chart seams can remain at fused junctions.

The Filament helper also renders the final geometry. Its scalar materials now use
triplanar projection: small boolean intersection triangles can fail TexUtil's UV
usability threshold, and this geometry-only preview does not sample textures.
Blender supplies the textured UV check. The social banner uses actual exports,
including the curved tree, at 1280×640 and 128 samples. Source recipes, renderer,
material package, [solid guidance](SOLIDS.md), and [banner instructions](SOCIAL.md)
are retained for reproduction.

## Weeping willow and profiled growth

The Release CTest suite passes **23/23 tests**, including **12,825 core checks**.
New tests cover profiled bend onset, vertical tails, retained segment lengths,
base pivots, children attached after deformation, partial final segments, smooth
opposite-direction turns, parallel directions, original default behavior,
repeatability, point budgets and validation of unused nodes.

Both willow GLBs have **zero Khronos errors**. With dedicated willow bark and young
branch maps, the full tree has three runtime normal-map tangent warnings, and the
structural wood has two. Its 78 input shells
union into one watertight structural component. The full tree adds separate thin
strand tubes and open leaves; it is not itself a single solid.

The seed-42 tree contains 810,482 triangles and 653,292 vertices. All 31,200 leaf
placements pass proximity checks and snap to the strand surface; the lowest leaf
is about 5.5 cm above the recipe's Y=0 plane. This is a checked example, not a
general ground collision guarantee for other parameter values or seeds.

The actual textured GLB was rendered and visually inspected in Blender 5.2.1 LTS,
Cycles CPU, 96 samples at 1200 pixels. The packed scene and render manifest are
saved under `out/weeping-willow/`. Filament's optional preview failed to access a
Metal device inside the sandbox, so no successful Filament render is claimed for
this example. See [WILLOW.md](WILLOW.md).

The dedicated willow material update also passes the 23-test suite. TexUtil's
2048-pixel bark and young-branch maps were rendered, their height/normal/color/
roughness sheet and 2x2 repeat inspected, and the source recipe validated. Matched
Blender full-tree and 1000-pixel, 48-sample trunk close-ups verify the embedded
materials. Mesh counts and leaf placement remain unchanged. See the
[willow material package](../samples/materials/willow/README.md) for regeneration controls.

## Configured LODs, growth, sunflower and packed cards

The Release and undefined-behavior-sanitizer CTest suites each pass **27/27 tests**,
including **22,860 core checks**. New coverage exercises nested seeded instance
and ribbon density, unchanged survivor UV/atlas/color/roll/wind data, card and tube
LOD reductions, named selection, output collision preflight, portable manifests,
growth onset and final-state identity, per-instance growth pivots, empty-step
cleanup, combined LOD/growth labels, normal-angle filtering, custom petal outlines,
edge deformation, ribbon attachment/endpoint/crossed-plane geometry, budgets and
embedded alpha-cutout materials. All nodes and variant overrides validate,
including unused nodes.

The checked set of **32 GLBs has zero Khronos errors**. The 52 warnings are all
`MESH_PRIMITIVE_GENERATED_TANGENT_SPACE`, because tangent-space normal maps rely
on runtime tangent generation. Reports are saved in
`out/lod-growth-sunflower-validation.jsonl`. The set includes sunflower and tree
growth snapshots, their LODs, ordinary willow LODs, packed willow levels and the
small card LOD example. Empty growth outputs deliberately have no invalid GLB.

The mature growth snapshot produces the same GLB bytes as ordinary generation.
The willow's ordinary LODs retain its welded wood and support twigs, reducing the
full 810,482 triangles to 311,282 and 211,442. The packed representation replaces
individual leaves and thin twig tubes with 600 crossed, textured ribbons while
retaining its 37,682-triangle welded wood. Its three levels have **52,082, 44,882
and 40,082 triangles**, respectively. Its current GLB was checked byte-identical
to the mesh used by the inspected renders after the density implementation.

Actual Blender 5.2.1 LTS Cycles CPU renders verify the willow LOD comparisons,
matched-scale tree growth, textured sunflower full/bloom views, packed willow
comparison and front/side coverage. Packed scenes and settings manifests stay
under `out/`. Native TexUtil map sheets and the actual petal/spray atlases were
inspected. The sunflower has nine embedded map images for petals, leaves and
stems; its geometry uses custom taper, cupping, edge ripples and lateral bend.
Its seed filter accepts 3,307 of 6,500 attempts on the facing surface. The optional
MaterialX SDK validator was unavailable (`ModuleNotFoundError: MaterialX`);
no SDK validation is claimed. GLB validation and actual Blender rendering passed.

See [LOD](LOD.md), [growth](GROWTH.md), [sunflower](SUNFLOWER.md), and
[packed-card](PACKED_CARDS.md) instructions for saved configurations, reproduction
commands and limitations. Atlas assets and source recipes are bundled; graph
execution does not require TexUtil or Blender.

## Fitted CardBake export

The 2026-09-11 CardBake pass adds CPU source-to-card projection, saved export
profiles and texture packages. Release and UBSan builds pass all 28 CTest cases.
The focused bake test covers deterministic bytes, color-factor encoding, UV and
normal orientation, source alpha holes/depth, unique cells, multiple pages,
retained materials, crossed/curved cards, memory and geometry limits, unsupported
materials, source protection and output selection.

Three final GLBs were checked with the optional Khronos validator: both willow
bake profiles and the single-strand study. They have zero errors and 15 warnings
about runtime-generated tangent space. The retained untextured cut material also
has unused-UV informational messages. Evidence is in the ignored
`out/card-bake-validation.jsonl` and Release/UBSan test logs.

Actual Blender 5.2.1 LTS Cycles renders compare the original 810,482-triangle tree
against 52,082- and 44,882-triangle CardBake variants. A separate side view and
close-up strand comparison were inspected. The renders use 64 samples and 64
transparent bounces; the latter prevents dark termination through dense alpha
layers in the comparison helper. The exported materials and geometry are used
without replacements. Both willow bakes use five 2048-square pages per map; the
GLBs are approximately 83.2 and 81.8 MiB, including retained wood textures.
See [CardBake](CARD_BAKE.md) for reproducible commands and projection limitations.

## Source-only sample packaging

The September 11 repository cleanup moved all 26 unchanged foliage graphs to
`samples/graphs/` and their TexUtil sources to `samples/materials/`. Birch's shared
source and the generic leaf sprite sources are included locally. The material
manifest prepares a portable graph/asset bundle without external recipe imports.

- Native TexUtil preparation completed all 18 jobs. All 75 former sample PNGs
  regenerated byte-for-byte, with no missing or different maps. A second run
  reused all 18 cached jobs after checking source and output hashes.
- All 26 graphs validated against the real prepared maps. Textured sprite cards
  and the small `Strand` CardBake exported successfully. Khronos validation found
  zero errors in both GLBs, with one runtime-tangent warning each.
- CMake installation included all source recipes and the preparation helper.
  The installed helper reused all 18 material jobs, and the installed executable
  generated textured sprite cards from the prepared bundle.
- Release passes 31/31 CTest cases. UBSan also passes all 31 after correcting a
  test-only macOS temporary-path comparison and rerunning that case. No sanitizer
  diagnostic was reported. CTest's small diagnostic textures are explicitly marked
  and kept separate from native TexUtil appearance checks.
- Repository-tool tests check dependency closure, source/output separation, atlas
  image enumeration and preservation of final/source files during cleanup.
  Successful CTest runs now remove their generated exports; failures retain them.
- The Git index contains no generated sample images, models, atlases or archives.
  Intentional `docs/images/` illustrations remain tracked. Local documentation
  links resolve after the moves. No shared Git history was rewritten.

The cleanup report and native checks are retained locally under `out/`. See
[repository storage](REPOSITORY.md) for size measurements and cleanup scope.

## Sakura and publication graphics

The September 11 sakura pass uses native TexUtil recipes for cherry bark, five-petal
blossoms and a 3-by-2 flowering-sprig atlas. All 21 material jobs prepare successfully
and all 27 sample graphs are included in the bundle. Release passes 32/32 CTest
cases. No new UBSan run is claimed for this pass.

All seven sakura GLBs pass Khronos validation with zero errors; warnings concern
runtime tangent generation. Seed 42 repeats byte-for-byte on this local build,
while seed 43 changes the tree and validates. The structural wood is one connected
watertight component. Its base/LOD2/CardBake counts are 299,464 / 80,976 / 52,192.
Blender 5.2.1 LTS Cycles renders were inspected for the full tree, branch, matched
comparison and a second baked-tree angle. See [sakura](SAKURA.md).

The two README graphics and social image use actual Blender render layers and
native TexUtil maps in editable SVG layouts. All three publication images were checked for
legible copy, plant framing, dimensions and correct triangle labels. The willow
comparison shows its existing 810,482 / 52,082 / 44,882-triangle exports. Publication
source hashes, SVGs and layer render manifests stay under `out/publication/`.


## Sakura growth animation

The September 12 pass adds a separate native staged sakura graph and an optional
continuous-growth public-API adapter for Blender. Release passes 33/33 CTest cases,
including all 32 standard growth snapshots. All 192 continuous animation sample
positions generate successfully. The final GLB matches the mature sakura's geometry,
materials and embedded textures exactly; its node label reflects its new filename.

Five nonempty checkpoints (25%, 50%, 70%, 85%, 100%) pass Khronos validation with
zero errors and the expected runtime-tangent warnings. Their wood remains one
connected watertight component. Zero progress correctly produces no mesh. Invalid
adapter progress values and output selections are rejected without output writes.
No new UBSan run is claimed for this pass.

Representative Blender previews were inspected at trunk, branch, hanging-shoot,
flowering and full-bloom stages. The saved animation uses real generated meshes
with keyed visibility and shared PBR textures, at a fixed camera and world scale.
Continuous growth is evaluated natively for every rendered frame; there is no
optical-flow interpolation or image-generated geometry.

A Metal device was detected, but the separate GPU probe crashed during kernel
compilation. The final animation therefore uses Cycles CPU. Evidence, previews,
frame reports and the movie stay under ignored `out/sakura-growth/`.

The final H.264 movie decodes without errors: 1280 by 960, 24 fps, 252 frames,
10.5 seconds. An encoded blossom-frame image was inspected. Reopening the packed
Blender scene verifies nine timeline checkpoints, correct single-stage visibility,
191 nonempty growth meshes and ten shared packed images. The scene is about
697 MiB because it retains the full geometry sequence; the movie is about 2.5 MiB.
Temporary test GLBs were removed after final verification; rendered frames and
final exports remain available locally.

## Developmental growth and overlapping sakura stages

The September 13 geometry update passes **35/35 Release CTest cases** and
**35/35 UndefinedBehaviorSanitizer cases**, with no sanitizer diagnostics.
The new development tests check independent length/thickness, curved tip advance,
parent-constrained branch radii, attachment-specific births, baby leaf expansion,
point-stage composition, stable atlas/color selection through proximity filtering,
sub-resolution birth boundaries, connected solid wood, mature identity and budgets.
Legacy scale-mode coverage remains in the existing suite.

All **192 continuous sakura samples** generate successfully. Every one of the
191 nonempty wood meshes is a single watertight component; progress zero is empty.
The mature GLB is byte-for-byte identical to the previous sakura-growth endpoint,
including its **299,464 triangles**, materials and embedded textures. The new
timing changes the development sequence, not the mature tree.

Sakura exports at approximately 47%, 77% and 100% pass Khronos validation with
zero errors and three expected runtime-tangent warnings each. A baby-leaf snapshot
from `development-tree`, both at base geometry and LOD1, passes with zero errors
and zero warnings. Real TexUtil
maps are used for the sakura exports and Blender previews. Check reports remain
under ignored `out/sakura-development/`. See [growth controls](GROWTH.md) and
[the sakura animation workflow](SAKURA.md#growth-animation).

The final Blender 5.2.1 LTS Cycles CPU render uses 24 samples and a fixed camera.
Its H.264 movie decodes without errors: 1280 by 960, 24 fps, 252 frames and
10.5 seconds. Representative early, middle and late renders and an encoded
flowering frame were visually inspected. Reopening the packed scene verifies
191 nonempty mesh snapshots with matching exported triangle counts, nine
visibility checkpoints and ten shared packed images. The movie is approximately
2.6 MiB; the editable scene is approximately 584 MiB because it retains every
growth mesh. Both remain outside Git. Duplicate test/preview GLBs are removed
after validation, while final outputs, rendered frames and reports are retained.

## Mineral generation (2026-10-01)

The rock/crystal/Boolean changes passed all 46 release native-build CTests,
including the C ABI, plus the real .NET 8 smoke. The dedicated mineral test
completed 154,759 checks. Six targeted UBSan tests (including fixture setup) passed.
Twenty sample/seed combinations generated successfully. All 16 base/LOD mineral
GLBs passed Khronos validation with zero errors and zero warnings.

Actual Blender 5.2.1 Cycles renders were inspected at azimuths 25 and 145 degrees
for rocks, cliff, rock formation, gems and the crystal cluster. They use exported
geometry and authored material factors, with no diagnostic fixture textures.
Reproduction commands are in [MINERALS.md](MINERALS.md). Local results are under
`out/minerals/`, including GLB reports, `gltf-validation.jsonl`, `seed-check.json`,
and render scenes/manifests.

Unity 6000.4.6f1 on macOS ARM64 passed isolated Built-in cliff and URP 17.4.0
crystal-cluster tests, including 42 → 77 → 42 seed changes, native/managed/upload
geometry equality, normals, indices, material slots, tangents, shader compilation,
single-sided culling and two rendered views. Reports and images are under
`out/unity-minerals-cliff-test/Artifacts/` and
`out/unity-minerals-urp-test/Artifacts/`. One observed cliff rebuild took 185 ms
on the worker and 0.057 ms to apply; the cluster took 25 ms and 0.047 ms. These are
editor generation measurements, not a player FPS benchmark.

The macOS universal package is `out/unity-minerals-package/`. Both ARM64 and
x86_64 binaries compile; only ARM64 editor execution was tested. Windows, Linux
and IL2CPP/player backends were not tested for this change. The Unity foliage
shader uses the basic material fallback: it does not implement gemstone volume
refraction/custom IOR. The GLB optical extensions were checked separately, and the saved Blender scene
confirmed the imported transmission, IOR and ruby volume-absorption shader values.
