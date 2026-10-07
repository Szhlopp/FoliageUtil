# Sakura and bamboo growth demo

These source scripts extend the local Sakura demo project. All Unity project
settings, generated scenes, native binaries, recipe bundles and previews stay
under `out/`; they are not committed here.

The prepared local project is `out/unity-demo`, with scene
`Assets/Scenes/Sakura.unity`. Open it in Unity 6000.4.6f1. It uses the native macOS
universal package and real prepared TexUtil maps.

Press Play, select **Both**, **Sakura** or **Bamboo**, and use **Grow again**, the
growth slider, **Pause** or **Fully grown**. Each selected plant runs generation on
its own worker. Selecting Both schedules the same progress on each generator;
their completed meshes can appear on different frames. No existing extra Sakura
copies are added to the playback group automatically. Selecting a plant in the
Hierarchy exposes its independent Foliage Generator settings.

Bamboo's Source Node is `wind`, selecting the full grove instead of the separate
culm study. Its recipe has overlapping culm extension/thickening, attached branches
and branchlets, and baby leaf expansion. Growth one is the original mature mesh.
The original fine-detail grove has 548,184 triangles; it has no saved CardBake or
LOD profile. Do not enter Sakura's `FloweringShoots` profile on the bamboo.

To add bamboo to an existing copy of the Sakura demo:

1. Prepare the samples and run `tools/package_unity_recipe.py` on
   `out/samples/graphs/bamboo.json`, targeting a new bundle directory.
2. Copy the complete bundle into
   `Assets/StreamingAssets/FoliageUtil/bamboo/`.
3. Copy `SakuraDemo.cs` into `Assets/Scripts/`, preserving the existing `.meta`
   when replacing that script. Copy `Editor/AddBambooToDemo.cs` into `Assets/Editor/`.
4. Choose **FoliageUtil > Add Bamboo Beside Sakura**. It uses the existing
   `Assets/Scenes/Sakura.unity`, preserves the Sakura objects and their settings,
   adds the grove eight meters to the right of the primary Sakura, connects the
   controls, frames the camera and saves the scene. Repeating the command reuses
   the existing Bamboo object and preserves its position/settings.

The live plugin keeps generated assets out of scene serialization. Save current
mesh as prefab creates persistent imported assets separately. The original local
scene and scripts were backed up under `out/unity-demo-backups/` before the bamboo
update.

`tests/unity/FoliageGardenSmoke.cs` verifies scene reload, preserved Sakura
settings, control references, empty/partial/mature bamboo and camera renders when
explicitly run in batch mode. `tests/unity/FoliageSmoke.cs` also checks that saving
a live scene avoids embedded generated assets and saved prefabs retain normal
persistent asset flags.
