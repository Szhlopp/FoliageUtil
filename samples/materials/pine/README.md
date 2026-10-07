# Pine bark

`bark.texutil.json` makes tileable brown pine bark with short irregular plates,
dry roughness and OpenGL normals. It imports the packaged
`../rooted-tree/bark.texutil.json` structure and compresses it vertically into four
repeats. Height and pigment are transformed before color and normal generation;
the normal image itself is not stretched.

The pine trunk uses 1.5 circumference repeats and 0.7 repeats per meter vertically.
The upright trunk's unflared base therefore gives approximately 0.96 m horizontally
by 1.43 m vertically per UV tile. Circumference coverage narrows with taper.
Relief is shading-only, with no texture displacement added to the mesh.

```sh
../TexUtil/build/texutil samples/materials/pine/bark.texutil.json --out out/pine-materials --threads 8 --json
python3 tools/prepare_samples.py --texutil ../TexUtil/build/texutil
```

The native exports include 2048-square color, height, normal, roughness and packed
roughness/metalness maps, a labeled sheet with a 2x2 repeat check, and MaterialX.
The preparation manifest puts these under `out/samples/assets/pine/` and retains
the source import closure. Base color is sRGB; height, normals and roughness are
linear data. The needle and young-twig materials are scalar colors in the foliage
graphs, so no leaf stencil or atlas is required.

See [the pine guide](../../../docs/PINES.md) for both trees and actual GLB previews.
All generated maps, material files and renders stay under `out/`.
