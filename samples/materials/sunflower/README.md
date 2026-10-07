# Sunflower material package

This directory tracks source recipes only. Run `python3 tools/prepare_samples.py
--texutil ../TexUtil/build/texutil` from the project root. The prepared files are
written under `out/samples/assets/` with this package's relative folder structure.
Do not copy generated maps or atlas manifests back into this source directory.

Native TexUtil graphs generate all textures in this package. The reference photo
in the conversation is used for shape/material direction, not copied into maps.
Petal coverage is approximately 4.2 cm across by 17.5 cm long, with intentional
size variation per instance. Leaves are approximately 22 by 32 cm. Stem U wraps
around the culm and V repeats 2.5 times per meter. These maps add shading relief;
there is no texture displacement. Fine fibers, veins, roughness and pigment are
coordinated with restrained height. All surfaces are dielectrics.

```sh
../TexUtil/build/texutil samples/materials/sunflower/petal.texutil.json --out out/sunflower-material --threads 8 --json
../TexUtil/build/texutil samples/materials/sunflower/petals.texutil.json --out out/sunflower-atlas --threads 8 --json
../TexUtil/build/texutil samples/materials/sunflower/leaf.texutil.json --out out/sunflower-material --threads 8 --json
../TexUtil/build/texutil samples/materials/sunflower/stem.texutil.json --out out/sunflower-material --threads 8 --json
```

The atlas uses a 3 by 2 grid of 384 by 768 cells with eight-pixel padding. Its
six source variants inherit seeds 710..715 and contain no pinned stochastic seeds.
Keep generated `petals.atlas.json` and its complete `.atlas.assets` folder together
in `out/samples/assets/sunflower/`. The reader owns UV origin conversion and padding. MaterialX and
inspection sheets are skipped by the atlas exporter; the standalone source
exports them for inspection. Preparation puts leaf/stem maps beside the generated
recipe copies under `out/samples/assets/sunflower/`.

The geometry petals use 0..1 UVs. `instance.atlas` selects one cell; both petal
materials bind the same color/normal/packed roughness atlas and use white scalar
base-color factors. Maps contain no lighting, cast shadows or alpha stencil.
Color PNGs are sRGB. Normals are raw OpenGL, height and roughness raw scalar,
and packed material maps use G=roughness/B=metallic. Scalar roughness factors are
one in the foliage recipe. Tissue transmission and subtle subsurface settings
travel separately through the saved FoliageUtil material metadata.

The native map sheets and actual textured Blender GLBs were inspected. Render
settings, full and bloom views are documented in [SUNFLOWER.md](../../../docs/SUNFLOWER.md).
The procedural result retains a simplified seed surface and broad leaves; it
is not a scanned asset.
