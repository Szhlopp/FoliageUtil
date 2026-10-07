# Wheat materials

This directory tracks source recipes only. Run `python3 tools/prepare_samples.py
--texutil ../TexUtil/build/texutil` from the project root. The prepared files are
written under `out/samples/assets/` with this package's relative folder structure.
Do not copy generated maps or atlas manifests back into this source directory.

`wheat.texutil.json` generates 1024-square tiled husk, straw and blade colors,
longitudinal normal relief and packed roughness. The source uses procedural fields;
no reference photograph is embedded. Color maps are sRGB, `normal.png` is 16-bit
OpenGL data, and `metallic-roughness.png` uses G for roughness and B for metallic zero.

The prepared maps make the FoliageUtil example independent of TexUtil at generation
time. Rerun sample preparation after edits and inspect the maps under
`out/samples/assets/wheat/`. See [WHEAT.md](../../../docs/WHEAT.md) for the complete
geometry and render workflow.
