# Meadow and bush greenery

`greenery.texutil.json` supplies two shared material sets for the six meadow
fillers and four common bushes. It imports the packaged sunflower leaf recipe
for vein, grain and channel-packing primitives. No external checkout or imported
image is required.

- Grass: a lengthwise green gradient, subtle parallel fibers, gentle normals and
  roughness variation.
- Broadleaf: restrained green pigment variation, a central vein and side veins,
  subtle OpenGL normals and packed roughness.

Prepare all source dependencies and graphs from the repository root:

```sh
python3 tools/prepare_samples.py --texutil ../TexUtil/build/texutil
```

The six 512-square material maps appear in `out/samples/assets/filler/` along with
a material inspection sheet. Color is sRGB; normals and packed data are linear.
Packed maps use G for roughness and B for metallic, which is zero for these plants.
Source leaves are geometry with opaque textures. The saved CardBake profiles
capture that geometry into actual RGBA silhouettes and double-sided MASK materials.

Edit the source recipe here, then prepare and rebake. Generated PNGs, fitted atlases
and models remain under `out/`. See [the asset guide](../../../docs/GREENERY.md)
for all ten graphs, dimensions, triangle counts and game import notes.
