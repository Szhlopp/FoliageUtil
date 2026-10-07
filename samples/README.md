# Reproducible sample sources

This directory contains the authored JSON graphs and every TexUtil recipe needed
to build their textures. Generated PNGs, atlas manifests, MaterialX files, Blender
scenes and mesh exports are deliberately not checked in.

```text
samples/
  graphs/       FoliageUtil plant, card, LOD, growth and CardBake recipes
  materials/    TexUtil source recipes, including shared/imported dependencies
  manifest.json Ordered material-generation jobs
```

From the repository root, prepare the complete bundle once:

```sh
python3 tools/prepare_samples.py --texutil ../TexUtil/build/texutil
./build/foliageutil out/samples/graphs/weeping-willow.json --card-bake Strands --out out/willow-baked
```

`--texutil` can point to any compatible TexUtil executable. The neighboring source
checkout is not required: the birch source and generic leaf sprite recipes are
included here. The helper checks that all recipe imports and sprite sources stay
inside this package. Native source dimensions/seeds are retained.

The output defaults to ignored `out/samples/`. It contains `graphs/` and `assets/`
with generated maps, atlas manifests and the corresponding TexUtil sources. Copy
that whole directory to move the prepared bundle. Graphs resolve `../assets/`
relative to `graphs/`, and each atlas keeps its manifest and matching `.assets`
directory together. The prepared bundle runs without TexUtil; only regeneration
requires TexUtil. Rendered meshes go into another directory under `out/`.

Edit the checked-in files here, then rerun preparation. Do not edit generated
copies in `out/samples/` and expect changes to survive preparation. Cached jobs
are reused only when the TexUtil binary, complete source dependencies and generated
file hashes match. Use `--force` to rebuild all jobs, or `--out` to create another
portable bundle. Outputs must be an empty directory or an existing marked sample
bundle; the helper refuses to overwrite sample source directories.

The same source layout is installed under `share/foliageutil/`. The installed
`tools/prepare_samples.py` works against those installed sources.

For examples that do not reference textures, the graph in `samples/graphs/` can
also be run directly. For consistent commands, use prepared graphs in this guide.

The [two pine studies](../docs/PINES.md) provide an upright whorled tree and a
wide spreading tree, with paired geometry needles, native TexUtil pine bark,
and detailed or compact CardBake profiles for both crowns.

The [meadow and common bush collection](../docs/GREENERY.md) adds six ground-level
fillers and four bush shapes. Every graph includes `Cards` and `CardsLow` profiles
with compact, single-page atlases and portable native TexUtil material sources.

The [Asiatic lily](../docs/ASIATIC_LILY.md) uses dark plum petals with orange tips,
speckled golden throats, separate stamens and ribbed buds. The graph exports a full
plant and bloom study, plus two optional CardBake profiles for the complete plant
that retain the floral organs in 3D.

The [mineral collection](../docs/MINERALS.md) adds seeded rocks, carved cliffs,
stacked formations, diamonds, rubies, emeralds and amethyst clusters. These samples
use procedural geometry and material factors, so they also run directly from source.

No reference photographs, arbitrary user models or generated atlases belong here.
Only the two README graphics and social JPEG belong in `docs/images/`; see
[repository storage policy](../docs/REPOSITORY.md).

CTest generates its own small **diagnostic** textures under `build/test-samples/`.
They exercise material/atlas/export contracts and are not visual examples. Use
TexUtil-prepared `out/samples/` for all appearance checks and final renders.

The [sakura growth example](../docs/SAKURA.md#growth-animation) includes a native
developmental recipe and a Blender animation workflow with continuous API sampling.
[development-tree.json](graphs/development-tree.json) demonstrates overlapping
trunk/branch growth and baby leaves, using the same attachment-triggered channels.

The `growth-exports.json` strand and `sakura-growth.json` tree save native Alembic
and geometry/CardBake stage export profiles. See [the export guide](../docs/EXPORTS.md).
Export packages and their texture files belong under `out/`, never in this source bundle.
