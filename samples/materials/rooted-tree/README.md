# Hefty bark material

This directory tracks source recipes only. Run `python3 tools/prepare_samples.py
--texutil ../TexUtil/build/texutil` from the project root. The prepared files are
written under `out/samples/assets/` with this package's relative folder structure.
Do not copy generated maps or atlas manifests back into this source directory.

Original seeded TexUtil recipe producing 2048×2048 maps for the rooted-tree
and curved banner-tree examples. FoliageUtil uses `bark-color.png` (sRGB),
`bark-normal.png` (OpenGL, linear), and `bark-metallic-roughness.png` (linear,
G=roughness, B=metallic zero). Its material factors are white and roughness one.

The graph warps elongated Voronoi plates and smaller fissures together, adds fine
surface grain, and derives normal and dry roughness maps from the final height.
Pigment varies between plates and over a separate broad noise field. The sheet
includes a 2×2 height repeat for inspecting texture continuity.

```sh
../TexUtil/build/texutil samples/materials/rooted-tree/bark.texutil.json --out out/rooted-tree-bark --threads 8 --json
```

Rerun sample preparation after edits and inspect the generated `bark-sheet.png`
and texture on the exported tree. The generated MaterialX file references the same color,
roughness and normal maps. The two 16-bit height maps are available for other
workflows, but the example GLB and Blender renders use normal relief only. Large
lobes and buttresses come from FoliageUtil geometry; no renderer displacement or
post-render painting is applied.
