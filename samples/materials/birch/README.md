# Birch tree assets

This directory tracks source recipes only. Run `python3 tools/prepare_samples.py
--texutil ../TexUtil/build/texutil` from the project root. The prepared files are
written under `out/samples/assets/` with this package's relative folder structure.
Do not copy generated maps or atlas manifests back into this source directory.

The bark wrapper imports the packaged `birch-source.texutil.json`. The original log material remains unchanged. The wrapper exports its
color/roughness, creates OpenGL normals from its height fields, retains cut-end
maps, and adds a young-wood color. These prepared PNGs let FoliageUtil generate the
tree without TexUtil; only regeneration requires a compatible TexUtil executable.
All source imports are included here, so a neighboring checkout is not required.

The leaf and atlas recipes are new procedural graphs: pointed toothed blades,
short petioles, central and angled veins, pigment and surface variation. The
4 by 3 atlas has twelve 256-pixel cells, four-pixel padding, seeds 91 through 102,
and synchronized color/alpha, normal and packed roughness maps. Keep
`leaves.atlas.json` beside its `leaves.atlas.assets` directory.

All maps are procedural, not photo cutouts. See [the birch study](../../../docs/BIRCH.md)
for controls, regeneration, material conventions and actual exported-tree renders.
