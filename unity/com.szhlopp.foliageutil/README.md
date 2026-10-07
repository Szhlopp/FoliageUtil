# FoliageUtil for Unity

Generate seeded foliage with the same native C++ engine as FoliageUtil's CLI.
Supports live developmental growth, saved LODs, packed cards and fitted CardBake.

1. Install this generated package through **Add package from disk** in Unity.
2. Bundle a prepared graph with `tools/package_unity_recipe.py` from the source
   repository and copy the entire bundle under `Assets/StreamingAssets/`.
3. Add **Foliage Generator** to an empty GameObject. Set Recipe Path relative to
   StreamingAssets, then adjust Growth, Seed and Lod. Use a Linear color space.
4. Leave Card Bake Profile empty for live geometry, or enter a saved profile name
   to fit/bake cards on the worker. For developmental growth, enable **Reuse Mature
   Card Bake** to bake once and animate the fitted cards with the same textures.
   The inspector separates preparation, worker and main-thread apply timings.
5. Use **Save current mesh as prefab** with a new Assets folder to save a reusable
   asset with imported textures and no runtime generation or baking.

Use **Reload recipe and textures** after editing source files. Runtime meshes,
materials and the component's private bake cache are released on disable. A
custom caller can use `FoliageRecipe.Generate` and `FoliageRecipe.BakeCards` directly
from worker tasks, then apply their owned snapshots on Unity's main thread.
`FoliageRecipe.PrepareCardGrowth` returns an independent disposable owner whose
`Generate(progress)` method updates growing cards without baking again. This
approximates shoot growth rather than individual leaf/flower stages. Bindings live
in memory; disabling or changing recipe, seed, LOD or profile requires preparation
again. Retained wood still regenerates, so measure actual update costs.

The included shader supports Built-in Forward and URP. HDRP needs a material
adapter. Thin translucency is supported; volumetric subsurface scattering and
animated wind are not implemented by this shader. New baked maps loaded at
runtime are uncompressed, so prepare texture assets through Unity's importer for
shipping game assets where possible.

The native binary must match the player's OS and CPU. Separate macOS, Windows and
Linux builds use the same C# code and recipe bundle. This source directory needs
`tools/build_unity_package.py` to include binaries and dependency licenses.

See `Documentation~/UNITY.md` in a generated package, or the
[full Unity guide](https://github.com/Szhlopp/FoliageUtil/blob/main/docs/UNITY.md).
