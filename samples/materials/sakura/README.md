# Native sakura materials

All maps are procedural TexUtil outputs. The five source recipes are self-contained:

- `petal.texutil.json`: pale fan-shaped tissue, a notched rim and faint veins.
- `blossom.texutil.json`: five petals, pink center, fine filaments and gold anthers.
- `sprig.texutil.json`: six flowers, two buds and a visible supporting twig.
- `sprigs.texutil.json`: native 3-by-2 spritesheet output using the sprig source.
- `bark.texutil.json`: mature and young cherry bark with horizontal lenticels.

The package manifest runs the bark, blossom and spritesheet jobs. Petal and sprig
recipes are imported dependencies, not separate publication jobs. Regenerate with:

```sh
python3 tools/prepare_samples.py --texutil ../TexUtil/build/texutil
```

The sprite cell is 384 by 480 pixels including six pixels of internal padding;
the whole atlas is 1152 by 960. Its manifest and `.atlas.assets` directory must
travel together. Local noise gives the cells slight warp and pigment variation;
imported flower seeds remain pinned. The floral arrangement is shared, not six
independently modelled shoots.

Bind matching RGBA color, OpenGL normal and packed roughness images on the card
material, and use MASK coverage. Atlas selection alone does not bind textures.
The support twig reaches the lower card pivot; transparent margins elsewhere do
not define attachment. See [the sakura example](../../../docs/SAKURA.md) and
[atlas guidance](../../../docs/SPRITESHEETS.md).

Generated maps, atlas manifests and models belong under `out/`, never here.
