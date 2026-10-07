# Live foliage in Unity

The Unity package calls the existing C++17 engine through a versioned C ABI.
It uses the same seeded graphs, developmental growth, LODs, packed ribbons and
CardBake profiles as the CLI. Geometry does not need a separate C# implementation.

## Install and try a recipe

Build a native package on the target host:

```sh
# macOS, both Apple Silicon and Intel in one library
python3 tools/build_unity_package.py --arch universal
# Windows x64 or Linux x64, run separately on each host
python3 tools/build_unity_package.py --arch x64
```

In Unity's Package Manager, choose **Add package from disk** and select the
produced `out/unity-package/package.json`. The source directory
`unity/com.szhlopp.foliageutil` alone has no native binary. Keep the generated
package outside Assets, or embed its entire directory under Packages.

Prepare actual textures, then bundle one graph:

```sh
python3 tools/prepare_samples.py --texutil ../TexUtil/build/texutil
python3 tools/package_unity_recipe.py out/samples/graphs/sakura-growth.json --out out/unity-recipes/sakura
```

Copy the **whole** `sakura` bundle to
`Assets/StreamingAssets/FoliageUtil/sakura/`. Add **Foliage Generator** to an empty
GameObject and set **Recipe Path** to `FoliageUtil/sakura/graph.json`. Use a Linear
color-space project. The Growth slider, seed and saved LOD name regenerate the mesh
in edit mode and play mode. An empty Source Node selects the first saved output.
**Reload recipe and textures** rereads edited files; **Regenerate** reuses the
parsed recipe. Disabling the component releases its owned mesh and materials.
Generated preview meshes, materials and textures are marked temporary so saving a
scene stores the generator settings without embedding those buffers. Use **Save
current mesh as prefab** when a persistent static asset is wanted; its cloned mesh
and materials have normal asset flags.

The local Sakura demo can also include bamboo beside the tree. The source controls
and editor setup helper live in [unity/demo](../unity/demo/README.md). Its **Both**,
**Sakura** and **Bamboo** buttons select which plants the growth slider and playback
affect. Bamboo uses the developmental stages in `samples/graphs/bamboo.json` and
selects `SourceNode = "wind"` for the complete seven-culm grove.

The packager copies referenced maps, imported OBJ files and atlas manifests,
including LOD overrides. It rewrites paths into a portable, deduplicated bundle.
TexUtil and the source checkout are not needed on the player's machine. Existing
nonempty bundle directories are preserved; use a new directory for revisions.

## Platform builds

One C++ source tree and one C# wrapper serve all desktop targets. Native machine
code is built separately for each OS and architecture:

| Platform | Native library | Packaged CPU targets |
| --- | --- | --- |
| macOS 12+ | `libfoliage_native.dylib` | ARM64, x86_64, or universal |
| Windows | `foliage_native.dll` | x86_64 |
| Linux | `libfoliage_native.so` | x86_64 |

The package importer enables each library only for its OS/CPU. To assemble a
multiplatform package, copy the distinct `Plugins/macOS`, `Plugins/Windows` and
`Plugins/Linux` directories from builds of the **same revision** into one package.
Keep the metadata beside the files. Do not include a macOS universal binary and
an overlapping ARM64/x64 copy together. Each build includes dependency licenses.
The GitHub workflow template `tools/ci/native-unity.yml` builds and tests all
three host targets and uploads generated packages as artifacts, keeping binaries
out of Git. To activate it, copy it to `.github/workflows/native-unity.yml` using
a GitHub login with workflow permission. The current repository login lacks that
permission, so the template is included without installing an active workflow.

CMake, Ninja and a C++17 toolchain are required. On Windows, run from an x64 Visual
Studio developer prompt. macOS bundles PNG/zlib and uses only system dynamic
libraries; Windows uses the static MSVC runtime. Linux retains its system C/C++
runtime dependency, so build on an appropriate oldest supported Linux baseline.
The template uses Ubuntu 22.04. macOS universal packaging verifies both binary
architectures and signs the local plugin ad hoc; application distribution signing
and notarization remain part of the game's build process.

This initial package targets Unity 2022.3+ desktop. WebGL, mobile and consoles need
additional platform integration. Desktop runtime asset loading uses filesystem
StreamingAssets paths. Cross-platform seed results are not promised bitwise equal.

## Live generation, timing and threads

```csharp
// FoliageGenerator API, call on Unity's main thread.
generator.SetGrowth(0.65f);
generator.Lod = "LOD1";
generator.RequestRebuild();
```

Each component runs at most one worker job. While it is building, new settings
coalesce into the latest request. A completed mesh is shown before the next job
starts, so continuously changing growth does not starve visible updates. This
rebuilds evaluated geometry; it is not a GPU vertex morph or a botanical simulation.

The worker handles generation, native-to-managed copying, handedness conversion,
tangents and bounds. The main thread applies buffers and material changes.
Textures and the parsed graph are retained between ordinary geometry updates.
Tangents use an accumulated UV basis, matching the CardBake convention; this is
not a claim of MikkTSpace identity. Disabling Calculate Tangents disables normal
mapping. Wind values survive in UV channel 1, but the supplied shader does not
animate wind.

The inspector exposes measured `LastBuildMilliseconds` and
`LastUploadMilliseconds`; the component also exposes `LastLoadMilliseconds`,
`LastStatisticsJson`, `LastError` and `IsBuilding`. Worker time includes baking
when selected. Main-thread apply time includes loading new baked textures.
Initial recipe/texture load is measured separately. Times are wall-clock timings,
not predicted FPS or GPU profiling results.

A local Unity 6000.4.6f1/macOS test of the 299,464-triangle Sakura measured about
441 ms on the worker and 2 ms applying its mature mesh. Initial texture/recipe
loading was about 402 ms. Moving tangent/bounds work to the worker reduced the
measured apply time from about 19 ms. These are individual editor measurements,
not hardware-independent performance guarantees. Background work primarily limits
update latency, but CPU contention, allocations and main-thread uploads can still
affect frames. Multiple components share the system thread pool; this is not a
global foliage job scheduler or a combined memory budget. A 60 FPS frame has
16.7 ms for the whole game. Load initial materials
during a loading phase and profile the player's actual CPU/GPU frames.

## CardBake in Unity

Set **Card Bake Profile** to a saved root `card_bakes` name, such as `Strand` on
`growth-exports.json`, `FloweringShoots` on Sakura, or `StrandsLow` on the willow.
Empty means ordinary graph geometry. The worker evaluates the selected growth
progress/LOD, fits cards and bakes textures, then returns the card mesh directly.
A GLB importer is not needed for this path. Source Node is ignored when a bake
profile chooses its source, guides and retained wood.

Baked material slots are rebuilt from the resulting mesh, including its actual
atlas pages, alpha cutoff, normals, roughness and uniform translucency. Empty and
wood-only early stages work. Existing results remain visible while the next bake
runs. The component keeps only the current generated bake package under Unity's
temporary cache; `LastBakeDirectory` locates it. Replacement/disable cleans its
owned cache. Do not store personal files in those cache directories.

For a reusable game asset, enter a new **Prefab Folder** inside Assets and click
**Save current mesh as prefab** after generation finishes. This saves the displayed
geometry, persistent materials and imported copies of its textures. The prefab
has no FoliageGenerator component and performs no runtime generation or baking.
Color maps import as sRGB; data/normal RGB maps remain linear. MASK color maps
enable alpha-coverage mip preservation at the material's cutoff, and the texture
importer requests high-quality platform compression. Review texture resolution,
compression and coverage settings for the target platform. Existing folders are
preserved. This works for both ordinary geometry and CardBake results.

CardBake is useful for authoring and occasional updates, not for rebaking texture
atlases every frame. The small strand test reduced 1,240 triangles to 24, but baking
it took about 219 ms and loading/applying the new maps took about 16 ms. For games,
bake during preparation or loading and reuse the finished mesh/textures. Existing
CLI stage exports can prepare separate baked growth states. Switching independent
stage meshes is discrete, not interpolated geometry. Packed `ribbon` graphs offer
another option: shared TexUtil spray atlases with cheap geometry regeneration,
without rebaking each growth state.

The full Sakura CardBake test produced 52,192 triangles from 299,464, with about
11 seconds of worker baking and 968 ms of main-thread apply/new-texture loading.
Save a prefab or prepare assets during loading to keep this work out of gameplay.

The source-to-card limitations in [CARD_BAKE.md](CARD_BAKE.md) still apply.
Lower triangle counts do not remove alpha overdraw, texture memory or shadow cost.
Runtime `LoadImage` creates uncompressed GPU textures with mipmaps; it does not
produce platform texture compression or alpha-coverage-preserving mip chains.
For shipped assets, import baked textures through Unity's asset pipeline and tune
compression and coverage settings. Full volumetric subsurface scattering is not
implemented by the included shader.

### Reuse mature cards for live growth

Set **Card Bake Profile** and enable **Reuse Mature Card Bake** on a recipe with
`growth.mode:"developmental"`, such as Sakura's `FloweringShoots` profile or the
`Strand` profile in `growth-exports.json`. The first request prepares a mature bake
even if Growth is zero. Further Growth changes reuse its textures, material
objects, fitted planes and atlas cells. The worker generates only the guides,
retained wood and growing card mesh. The finished mature geometry matches a
normal CardBake. The existing per-update bake mode remains available with the
switch off.

Cards appear when their guide's support arrives, extend along the reached guide,
and widen with its radius maturation. They contain mature leaf/flower images;
individual source foliage stages are not replayed. A revealing tip may cut through
those silhouettes. This is an approximation for growing shoots, with independent
leaf/flower growth available through ordinary geometry or smaller source cards.

`LastPreparationMilliseconds` measures the last one-time bake separately from
`LastBuildMilliseconds`, which measures repeated geometry/copy/tangent/bounds
work. `CardBakePreparationCount` counts successfully applied preparations.
`LastUploadMilliseconds` still includes the first baked texture load; subsequent
updates retain those GPU textures and upload only the mesh. Initial recipe/source
material loading remains in `LastLoadMilliseconds`. Profile actual player frames
as well as worker latency. Retained wood unions can still be expensive.

Changing the recipe (including **Reload recipe and textures**), seed, LOD, bake
profile or budgets prepares another cache. Growth, tangent and shader changes
reuse the bake; changing the shader recreates its material objects. The old result
remains visible until its replacement is ready. Disabling releases the native
bindings and cleans only the component's private temporary directories. Reenabling
prepares again. Bindings currently live in memory; a saved static prefab does not
retain this growth behavior, and existing baked manifests cannot yet be loaded
as prepared growth bindings.

```csharp
using (var recipe = new FoliageRecipe(json, assetDirectory))
using (var cards = await Task.Run(() => recipe.PrepareCardGrowth("FloweringShoots", emptyOutputDirectory, seed: 42)))
{
    MeshSnapshot young = await Task.Run(() => cards.Generate(0.4));
    MeshSnapshot mature = await Task.Run(() => cards.Generate(1));
    // Apply buffers on the main thread; retain the same materials and textures.
}
```

The prepared handle owns its graph independently of the recipe. Dispose it when
finished; callers of this direct API own the texture directory's lifetime. The
additive C ABI entry points are `fu_card_growth_create`, `fu_card_growth_generate`
and `fu_card_growth_destroy`. Rebuild the native plugin and managed package
together to obtain them. All existing vertex layouts and generation calls remain
unchanged.

Run the reusable-bake test in a fresh disposable project. It checks mature mesh
equality, forward/backward and empty growth, texture object reuse, seed/reload invalidation,
temporary object ownership and two viewing angles at four growth stages. Timings,
real Unity renders and the reference GLB go under that project's `Artifacts/`.

```sh
python3 tools/test_unity.py --editor /path/to/Unity --package out/unity-package --recipe out/samples/graphs/sakura-growth.json --project out/unity-card-growth-test --name sakura --card-bake FloweringShoots --reuse-mature-bake
# Add --urp-version matching the installed editor to exercise URP as well.
```

On October 1, 2026, the full seed-42 Sakura passed this test in Unity
6000.4.6f1/macOS ARM64, Built-in and URP 17.4.0. Both kept the same materials and
GPU texture objects through 33 forward/backward samples, matched the direct bake
at maturity (52,192 triangles), prepared again when the seed changed, and cleaned
the owned cache on disable. Actual prepared TexUtil maps were used.

| Growth | Built-in worker | URP worker | Main-thread apply, both |
| --- | ---: | ---: | ---: |
| 35% | 35.7 ms | 36.0 ms | 0.04..0.05 ms |
| 55% | 78.2 ms | 77.3 ms | 0.07..0.08 ms |
| 75% | 161.5 ms | 159.4 ms | 0.11..0.14 ms |
| 100% | 207.2 ms | 202.7 ms | 0.14..0.16 ms |

Preparation remained about 11.0..11.2 seconds, with about 1.0 second for the first
baked texture upload and 0.4 seconds for initial recipe/source texture loading.
These are separate one-time costs for that cache. The mature native node report
spent about 174 ms in the retained wood's `solidify` node. Reusing cards removes
repeated atlas work; welded wood is now the main remaining update cost. These
editor wall-clock measurements establish update latency, not game FPS or player
CPU/GPU frame performance. The images approximate shoot growth rather than each
flower's original animation.

## Use the managed/native API directly

`FoliageRecipe` has no UnityEngine dependency. It owns a parsed graph and supports
concurrent reads. Native handles are reference-counted across active managed calls;
disposal cannot free a recipe still in use by a worker. Returned snapshots copy
native buffers and own their managed arrays.

```csharp
using (var recipe = new FoliageRecipe(json, assetDirectory))
{
    MeshSnapshot mesh = await Task.Run(() => recipe.Generate(0.7, seed: 42, lod: "base"));
    float[] tangents = await Task.Run(() => mesh.CalculateTangents());
    // Apply mesh/materials on Unity's main thread, or use FoliageGenerator.
    MeshSnapshot cards = await Task.Run(() => recipe.BakeCards("StrandsLow", emptyOutputDirectory, 1, seed: 42));
}
```

`BakeCards` writes to an empty caller-owned directory and preserves existing files.
The caller manages that directory's lifetime. `MeshSnapshot.InfoJson` carries the
matching material array and asset base; `StatisticsJson` includes the bake report.
The profile's GLB and texture package is available there for offline use. Ordinary
`Generate` does no export-file writing. `progress:-1` selects mature geometry;
otherwise progress is 0..1. Budget overrides limit generated vertices, triangles
and placement points; they are not whole-process RAM limits.

The header `include/foliage/c_api.h` is the native contract. Strings are UTF-8,
calling convention is Cdecl, and no C++ exception crosses the ABI. Native views
have 14 floats per vertex: position XYZ, normal XYZ, linear color RGBA, UV XY,
wind XY. Submesh records are three uint32 values. Borrowed data lives until mesh
destruction. Error text is thread-local and must be read before another fallible
call. The C# bridge mirrors X and reverses triangle winding exactly once for Unity;
UVs keep their native bottom-left convention. Ordinary mesh slots retain every
recipe material, including empty slots. CardBake slots contain its used materials.

## Rendering and verification

The included `FoliageUtil/Foliage` shader supports Built-in Forward and URP 14+,
with color/vertex factors, OpenGL normal maps, packed roughness/metallic data,
MASK/BLEND modes, two-sided foliage, shadow casting and thin-tissue backlighting.
Built-in lighting uses the main light and SH ambient. URP includes additional
lights. HDRP requires a compatible shader/material adapter. Subsurface and wind
metadata are preserved by the graph but are not fully rendered by this adapter.

Run native and managed tests without a Unity license:

```sh
ctest --test-dir build-unity-macos-universal --output-on-failure
DYLD_LIBRARY_PATH="$PWD/build-unity-macos-universal" dotnet run --project tests/managed/ManagedSmoke.csproj -- out/samples/graphs/growth-exports.json
```

On Linux use `LD_LIBRARY_PATH`; on Windows add the build directory to `PATH`.
The managed smoke test needs .NET 8. It calls the actual library, including
concurrent generation, disposal, error propagation, tangent frames and CardBake.

With a licensed Unity editor, run a real isolated project test:

```sh
python3 tools/test_unity.py --editor /path/to/Unity --package out/unity-package --recipe out/samples/graphs/growth-exports.json
# Choose a fresh project directory for each configuration.
python3 tools/test_unity.py --editor /path/to/Unity --package out/unity-package --recipe out/samples/graphs/growth-exports.json --project out/unity-card-test --card-bake Strand
# For a Unity 6000.4 editor:
python3 tools/test_unity.py --editor /path/to/Unity --package out/unity-package --recipe out/samples/graphs/sakura-growth.json --project out/unity-urp-test --name sakura --urp-version 17.4.0
```

The tests run Unity in batch mode, check changing meshes and buffer layouts,
render single-sided front/back faces, compile the shader and save real camera
renders under `out/unity-preview/`. They use actual prepared TexUtil textures.
They do not open or change another Unity project. Native CTest fixtures remain
diagnostic data and are not acceptable substitutes for visual checks.

Verified locally on macOS ARM64 with Unity 6000.4.6f1: native CTest (Release and
UBSan), actual .NET PInvoke, Built-in and URP 17.4 rendering, developmental growth
and CardBake, plus native generation/CardBake in a standalone Mono player. The universal library contains both ARM64 and x86_64 code; only its
ARM64 slice was exercised here. Windows and Linux have a build/test workflow template
but still need execution on those hosts. The installed editor lacks the macOS IL2CPP
player module, so IL2CPP is not yet verified.

Optional standalone tests are `tests/unity/FoliagePlayerSmoke.cs` (copy to the
disposable project's Assets) and `FoliagePlayerBuild.cs` (copy to Assets/Editor).
Run the editor with `-executeMethod FoliagePlayerBuild.Run`, or add `-foliageMono`
for the Mono backend. Run the produced test app with `-batchmode -nographics`
and check for `FOLIAGE_PLAYER_SMOKE_PASSED`. This verifies player PInvoke, worker
generation and CardBake, independently of editor assembly loading.

## Rocks and crystals

The `rock`, `crystal` and `boolean` nodes use the existing native generation API.
See [MINERALS.md](MINERALS.md) for portable samples and `tools/test_unity.py --minerals`.
The provided foliage shader supports their geometry and basic PBR, but does not
implement gemstone volume refraction or custom IOR. Optical fields remain in
native material JSON for custom shader adapters; GLB exports standard Khronos
transmission, IOR and volume extensions for compatible viewers.
