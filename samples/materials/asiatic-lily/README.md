# Asiatic lily materials

`lily.texutil.json` is a self-contained native TexUtil recipe for the supplied
dark-plum and orange lily references. It generates 1024-square petal, leaf and bud
maps without importing photographs or depending on another recipe checkout.

The petal material combines a dark central pigment field, an irregular red border,
orange tips and edges, a pale golden throat, and dark speckles. Lengthwise vein
relief is subtle, with a comparatively matte petal finish. The leaf maps use dark
green pigment and parallel relief. The bud color has six longitudinal green bands,
matched by shallow geometric ridges in the foliage graph.

```sh
python3 tools/prepare_samples.py --texutil ../TexUtil/build/texutil
```

Prepared maps and the native inspection sheet appear under
`out/samples/assets/asiatic-lily/`. Color PNGs are sRGB. OpenGL normal maps and
packed maps are linear; packed G is roughness and B is zero metallic.
The source surfaces use opaque maps. CardBake captures their geometry into actual
RGBA silhouettes with double-sided MASK materials.

Useful controls in the source material:

- `dark_length` and `outer_length`: dark pigment and red border along the petal.
- `interior` and `red_interior`: width of the orange/red edge.
- `brush_amplitude`: irregularity of the pigment boundary.
- `warm_color`, `red_color`, `dark_color`: the three pigment palettes.
- `speck_zone` and `specks`: location, count and size of throat markings.
- `normal.strength` and `roughness`: vein relief and surface finish.

The `t` gradient is reversed so the native image's bottom is the attachment base.
Do not add another UV flip in the foliage graph or exported material.
Edit source recipes, prepare, and then regenerate the models and bakes. See the
[lily guide](../../../docs/ASIATIC_LILY.md) for geometry controls and reproduction.
