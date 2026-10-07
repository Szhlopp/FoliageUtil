# Procedural rose materials

This directory tracks source recipes only. Run `python3 tools/prepare_samples.py
--texutil ../TexUtil/build/texutil` from the project root. The prepared files are
written under `out/samples/assets/` with this package's relative folder structure.
Do not copy generated maps or atlas manifests back into this source directory.

`petal-sprite.texutil.json` generates a fan-shaped alpha stencil and aligned PBR
maps. `petals.texutil.json` asks TexUtil for twelve seeded variants. The generated
`petals.atlas.json` and `petals.atlas.assets/` are one inseparable package consumed
by FoliageUtil's detailed rose and single-petal examples. Their source and generated
content were authored for this project; the user-supplied photograph is a visual
reference and is not embedded in these maps.

The original `petal.texutil.json` and `leaf.texutil.json` still produce individual
PNG/MaterialX outputs from the first material study. The detailed rose uses the petal
atlas plus the original leaf maps. The extra atlas height map is not a displacement
binding. See ../../../docs/ROSE.md for controls, regeneration, rendering and limitations.
