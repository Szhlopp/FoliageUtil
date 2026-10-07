# Willow spray atlas for packed cards

This directory tracks source recipes only. Run `python3 tools/prepare_samples.py
--texutil ../TexUtil/build/texutil` from the project root. The prepared files are
written under `out/samples/assets/` with this package's relative folder structure.
Do not copy generated maps or atlas manifests back into this source directory.

`spray.texutil.json` authors one whole leafy shoot with native TexUtil shapes,
explicit stamps, procedural pigment, a connecting twig and a coherent warp.
`sprays.texutil.json` creates six seed variants in a 3 by 2 atlas. Cells are
384 by 1536 pixels including eight-pixel padding. Physical strip coverage is
approximately 0.46 by 2.4 meters; ribbon path lengths intentionally stretch it.

Each cell contains 42 attached leaf silhouettes and a thin central twig. The
fixed placement list accounts for the rectangular physical coverage when choosing
stamp orientation. Seeded color and coherent drift vary the cells while retaining
a visible base at the bottom center. Crossed ribbon planes share the same cell.
These authored sprays approximate the source foliage rather than baking its exact
3D arrangement. They are not alpha masks cut from the user's photo.

```sh
../TexUtil/build/texutil samples/materials/willow/packed/sprays.texutil.json --out out/willow-packed-atlas --threads 8 --json
```

Inspect the generated atlas and actual GLB. Keep `sprays.atlas.json` with its entire
`sprays.atlas.assets` folder under `out/samples/assets/willow/packed/`, preserving
their relative paths. `color.png` is sRGB RGBA;
normal and height are raw linear data; packed roughness uses G and zero metallic B.
The normal is derived after all silhouettes are positioned and warped, so encoded
normal pixels are never rotated by a stamp. Normal relief is modest; the ribbons
supply overall curvature. The base twig is visible through the card's UV boundary.

FoliageUtil's `willow_cards` material binds the color, normal and packed material
maps with `alpha_mode:"MASK"`, double-sided shading and independent translucency.
See [packed-card generation](../../../../docs/PACKED_CARDS.md) for geometry controls,
counts, reproduction commands and the overdraw tradeoff.
