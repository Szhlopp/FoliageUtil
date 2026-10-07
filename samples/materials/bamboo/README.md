# Bamboo materials

This directory tracks source recipes only. Run `python3 tools/prepare_samples.py
--texutil ../TexUtil/build/texutil` from the project root. The prepared files are
written under `out/samples/assets/` with this package's relative folder structure.
Do not copy generated maps or atlas manifests back into this source directory.

`culm.texutil.json` generates longitudinal fibers, pigment variation, pale joint
bands, a seam, OpenGL normal relief and packed roughness. `branch-color.png` uses
the same base material without the bands. Tube longitudinal UV repeat must equal
`1 / node_spacing` to align the bands with the raised rings.

`leaves.texutil.json` renders eight seeded variants of `leaf.texutil.json` into
matching atlas maps and a manifest. Leaf outlines are geometry, so no stencil is
needed. Colors are sRGB, normals are 16-bit linear data, and roughness is packed
in G with metallic zero in B. No lighting is baked into these maps.

The prepared files make the FoliageUtil recipe independent of TexUtil at generation
time. See [BAMBOO.md](../../../docs/BAMBOO.md) for controls and regeneration commands.

