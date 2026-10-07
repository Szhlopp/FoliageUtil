# Lily pad atlas

This directory tracks source recipes only. Run `python3 tools/prepare_samples.py
--texutil ../TexUtil/build/texutil` from the project root. The prepared files are
written under `out/samples/assets/` with this package's relative folder structure.
Do not copy generated maps or atlas manifests back into this source directory.

`pad.texutil.json` defines a round, notched alpha stencil with subtle rim
irregularity, radial veins, fine cellular detail and pigment variation.
`pads.texutil.json` renders six seeded variants into a 3 by 2 atlas with shared
color, OpenGL normal and packed roughness layouts.

The manifest carries cell bounds and padding. Copy it with the complete
`pads.atlas.assets` directory. Color is sRGB RGBA, normal maps are 16-bit linear
data, and roughness is in G with metallic zero in B. Texture alpha cuts the visible
outline from a curved card; it does not cut the underlying triangles.

See [LILY_PAD.md](../../../docs/LILY_PAD.md) for the geometry controls and reproducible
TexUtil and Blender commands.

