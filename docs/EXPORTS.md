# Growth animation and stage packages

Named root `exports` profiles package the same saved growth evaluator for two uses:
continuous sampled Alembic caches and independent GLB stages. They are opt-in;
ordinary generation still follows the existing `growth` and `lods` batch settings.
Query `foliageutil exports --json` for the authoritative schema, also saved in
[exports.json](exports.json).

```json
"exports": {
  "Animation": {
    "type": "alembic", "output": "tree-animation", "source": "wind",
    "frames": 192, "fps": 24
  },
  "GameStages": {
    "type": "stages", "output": "tree-stages", "source": "wind",
    "steps": 8, "geometry": "card_bake", "card_bake": "FloweringShoots",
    "stage_overrides": {
      "0": {"geometry": "mesh"},
      "1": {"geometry": "mesh"},
      "2": {"geometry": "mesh"}
    }
  }
}
```

`source` names a mesh node. `lod` defaults to `base`; select a saved LOD when
needed. Both forms sample `start` through `end` inclusively (defaults 0 and 1).
The root `growth` definition supplies the stage timing and development controls.
No additional species logic or interpolation is introduced. The fully grown
geometry agrees with ordinary generation at the selected LOD.

```sh
python3 tools/prepare_samples.py --texutil ../TexUtil/build/texutil
./build/foliageutil out/samples/graphs/growth-exports.json --export Animation --out out/growth-exports --json
./build/foliageutil out/samples/graphs/growth-exports.json --export CardStages --out out/growth-exports --json
./build/foliageutil out/samples/graphs/sakura-growth.json --export Animation --out out/sakura-exports --json
./build/foliageutil out/samples/graphs/sakura-growth.json --export GeometryStages --out out/sakura-exports --json
./build/foliageutil out/samples/graphs/sakura-growth.json --export CardStages --out out/sakura-exports --json
```

The Sakura also saves `MixedStages`: its first four of eight stages use geometry,
then its grown canopy uses independently fitted CardBake cards. Edit source
recipes in `samples/graphs/` and prepare again. CLI `--export` selects one named
profile and cannot combine with `--card-bake`, `--lod`, `--no-lods`,
`--growth-step` or `--no-growth`. Seed and generation budget flags still apply.

## Alembic

Native Ogawa export is enabled by default. It requires CMake 3.29 or newer and
fetches pinned Alembic 1.8.12 and Imath 3.2.1 source releases. Their licenses are
installed with the utility. `-DFOLIAGE_ALEMBIC=OFF` removes this dependency and
retains normal geometry, CardBake and stage exports. Profiles still validate in
that build, but selecting an Alembic profile reports that support is disabled.
Blender is needed only for preview/import verification, not for writing `.abc`.

The package contains:

- `animation.abc`: one stable mesh track per used material, including explicit
  empty samples before birth. Positions, normals, UVs, `Color` RGBA vertex colors,
  `_WIND` weights and phase, material face sets, and visibility are sampled.
- `materials.json`: FoliageUtil PBR material settings and face-set/object bindings.
- `textures/`: copied PNG/JPEG maps, with relative paths and duplicate sources shared.
- `manifest.json`: frame times, growth progress, geometry statistics and file list.

The cache uses right-handed Y-up meters and Alembic's clockwise face convention.
Texture UVs retain their native bottom-left origin. Normals use the same convention
as ordinary geometry. Source vertex colors remain linear; multiply them by the
base-color factor and sRGB base-color map. Roughness is packed in G, metalness in B;
normal maps use OpenGL convention. UVs use indexed face-corner samples for DCC
compatibility. Tissue translucency requires a suitable shader.
Alembic does not embed the renderer's material graphs.

Frames are 2..2000, fps is 1..120, and sample zero is at time zero. Duration is
`(frames - 1) / fps`, so 192 samples at 24 fps end at 7.958333 seconds. This cache
has changing topology, not fixed-topology morph targets. Treat samples as discrete
when vertex correspondence changes. Higher fps costs more storage. `_WIND` is
metadata for a compatible shader, not a second baked wind simulation.

### Blender

Use the included adapter to reconstruct PBR materials and make empty-frame
visibility reliable when scrubbing backward in Blender. Blender's mesh cache
reader can retain its last mesh on a zero-face sample; the adapter keys visibility
from the package manifest. The archive itself contains true empty samples.

```sh
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup \
  --python tools/import_growth_alembic.py -- \
  --package out/sakura-exports/sakura-animation \
  --out out/sakura-exports/sakura-animation.blend
```

The scene ends on the mature frame. Set the timeline to zero to inspect birth.
Keep the `.blend` and external package in their relative locations when moving
it. Blender quantizes cache vertex colors to bytes; the adapter corrects their color
interpretation in the shader. It creates materials from the exported maps and applies FoliageUtil's
existing translucency/subsurface preview controls. A plain import may require
manual material assignments and empty-frame visibility handling.

### Unity

Install Unity's [Alembic package](https://docs.unity3d.com/Packages/com.unity.formats.alembic@2.4/manual/index.html),
import `animation.abc`, and place its imported asset in the scene. Enable UVs,
normals and colors; confirm the importer's handedness conversion for Y-up assets.
Assign materials using the exported face-set names, following Unity's
[material remapping workflow](https://docs.unity3d.com/Packages/com.unity.formats.alembic@2.4/manual/materials.html).
Use [Timeline](https://docs.unity3d.com/Packages/com.unity.formats.alembic@2.4/manual/timeline.html)
or its stream player to drive time. Disable sample interpolation when topology
correspondence is not stable. Copy the matching textures and recreate the PBR
settings in the shader used by your render pipeline; materials are separate from
Alembic. Current Unity Alembic support targets desktop platforms; check its package
support list before choosing it for other build targets.

Native archive readback and Blender are verification targets here. Unity import
and player performance need testing in the actual project and render pipeline.
A runtime plugin calling `foliage_core` would be a separate integration: a C ABI,
platform native libraries, a C# wrapper and background mesh generation. The CLI
export profiles do not install a Unity runtime plugin.

## Geometry and CardBake stages

`steps` is 2..128, independently of `growth.steps`. Choose `geometry:"mesh"` or
`geometry:"card_bake"`; the latter also requires a saved `card_bake` profile.
`stage_overrides` maps canonical zero-based indices to `geometry`, `card_bake`
and/or `lod` overrides. Missing fields inherit the main profile.

Each snapshot lives under `G000`, `G001`, etc. A nonempty stage contains
`model.glb`, with embedded PBR textures. A CardBake stage also contains its
`model.cardbake/` inspection package. Empty stages have a manifest entry and no
mesh file. Keep the top-level `manifest.json` with the package for the progress
values, representation choices, counts and paths.

CardBake evaluates the **currently grown** source, retained wood and visible guide
prefixes. When foliage has not emerged it exports only any available retained
wood. Each later stage fits and bakes its own cards and atlas; cells are not
randomly reassigned and the mature atlas is not stretched over young growth.
The usual [CardBake](CARD_BAKE.md) source/component/material restrictions apply.
Very small newly emerged details may need higher bake resolution or a geometry
stage to preserve visible coverage. Bakes approximate depth and can look different
edge-on. Inspect multiple angles and budget alpha overdraw as well as triangles.

These are independent meshes, with independent baked texture layouts. In Unity,
use a GLB importer compatible with your render pipeline or convert the stages to
an engine-supported mesh format. Switch stage objects at the saved progress
thresholds. Optional shader crossfades belong to the game integration. These
stages are not automatically Unity prefabs, LODGroups or morph targets.

## Budgets, overwrite behavior and API

Generation limits apply to every sample. `max_samples_mb` (default 2048) bounds
the cumulative uncompressed vertex/triangle record estimate; it is not a process
memory limit. `max_output_mb` (default 2048) bounds package bytes. File sizes are
checked during export and before publication. A single generated file can exceed
the threshold temporarily before the check aborts and removes that new package.
CardBake also applies its existing per-bake `memory_mb` estimate.

Packages are built in a temporary sibling directory and published only when
complete. Re-export replaces an owned package and removes its obsolete stages.
An existing nonempty directory must contain a recognized manifest whose file
list still matches. Disposable Finder `.DS_Store` metadata is ignored by that
ownership check. Other added or missing files cause a refusal, preserving personal
files. Choose another output directory if you have edited the package layout.
Imported source assets may not be overwritten. Keep renders and `.blend` scenes
outside the managed package directory. These generated outputs stay out of Git.

```cpp
Json report = foliage::exportProfile(graph, "Animation", outputDirectory, options);
bool supported = foliage::alembicAvailable();
```

`exportProfile` accepts a base graph and selects growth/LOD from its profile.
Do not set `Options.growth` for this API. The lower-level `exportCardBake` now
accepts a selected LOD and continuous `Options.growth` when integrating your own
pipeline. The existing standalone `--card-bake` CLI remains a mature export;
use named stage profiles for growth batches.

## Verification example

The 192-frame Sakura export was read back through Alembic and imported in Blender
5.2.1 LTS. The mature imported corners were compared with the ordinary GLB,
including positions, UVs and vertex colors. Blender's custom-normal conversion
differed by at most 0.54 degrees; native archive normals read back unchanged. Empty-frame backward
scrubbing was checked with the adapter. Reproduce the optional Blender check:

```sh
/Applications/Blender.app/Contents/MacOS/Blender --background --python-exit-code 1 \
  --python tests/blender_exports.py -- \
  --package out/sakura-exports/sakura-animation \
  --reference out/sakura-exports/sakura-geometry-stages/G007/model.glb
```

At seed 42 the mature geometry stage contains **299,464 triangles**. The
FloweringShoots CardBake stage contains **52,192 triangles**, including retained
wood, and was inspected in Blender from two angles. Lower triangle count does
not imply a smaller texture package: each CardBake stage carries its own atlas.
Khronos validation reports no GLB errors; the existing runtime-generated tangent
space warnings remain. Native export tests also cover invalid profiles, imported
asset protection, output/sample budgets and preservation of completed packages
on failure. Release, UBSan and Alembic-disabled builds run the CTest suite.
