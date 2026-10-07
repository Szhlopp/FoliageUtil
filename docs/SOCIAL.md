# README graphics and GitHub social image

The publication set follows TexUtil's dark navy, colored-panel style and uses
actual exported FoliageUtil meshes rendered in Blender. The two README graphics
are 1536 by 1024 pixels; the GitHub social image is 1280 by 640.

| Image | Content |
| --- | --- |
| [Workflow](images/foliageutil-workflow.png) | Graph structure, growth stages, TexUtil atlases and measured CardBake reduction |
| [Samples](images/foliageutil-samples.png) | Sakura, willow, rose, sunflower, birch, bamboo and wheat |
| [Social preview](images/foliageutil-social-preview.jpg) | Willow on the left; birch, bamboo, wheat and a low rose on the right |

The title uses white `Foliage` with small white `Util` at 30 percent size, modestly
raised and separated from the main word. The social composition crops the plants
at the bottom and edges. It contains no ground, water or lily pad. Presentation
scale varies between species. The willow triangle comparison retains the shared
scale of its original comparison scene.

## Editable sources

[tools/build_publication_graphics.cjs](../tools/build_publication_graphics.cjs)
contains the layouts, exact copy, dimensions, palette and typography. It emits
editable SVG layouts, raster PNGs, a social JPEG and a source-hash manifest under
`out/publication/`. It embeds the real render layers and native atlas without
repainting the plant imagery. Font selection is Arial, Helvetica, then sans-serif;
inspect text spacing on another system. No fonts are checked in.

[tools/render_publication_cutouts.py](../tools/render_publication_cutouts.py)
opens the verified packed preview scenes listed in
[tools/publication_layers.json](../tools/publication_layers.json). It keeps their
cameras, meshes, materials and lights, hides presentation ground/text, and renders
transparent layers. Original scene files are not saved or overwritten. The layers
are 48-sample Cycles CPU renders; the flower close-ups use 64 samples. PNG alpha
bounds control framing in the layouts, without geometry or color modification.

With the corresponding sample scenes already rendered:

```sh
blender --background --factory-startup --python-exit-code 1 --python tools/render_publication_cutouts.py -- --manifest tools/publication_layers.json --samples 48 --threads 8
blender --background --factory-startup --python-exit-code 1 --python tools/render_publication_cutouts.py -- --manifest tools/publication_layers.json --only rose-detail sunflower-detail --samples 64 --threads 8
node tools/build_publication_graphics.cjs
```

The Node helper requires `sharp` (the checked export used 0.35.4). Use an existing
installation through `NODE_PATH`, or install it into ignored `out/publication-tools`
with `npm install --prefix out/publication-tools sharp@0.35.4` and run with
`NODE_PATH=out/publication-tools/node_modules`. Blender and Node/sharp are optional
publication tools, not generation dependencies. The helper's optional positional
argument changes its output directory.

The scene manifest identifies every local render dependency. Recreate missing
scenes from the [sakura](SAKURA.md), [willow](WILLOW.md), [rose](ROSE.md),
[sunflower](SUNFLOWER.md), [birch](BIRCH.md), [bamboo](BAMBOO.md),
[wheat](WHEAT.md), [growth](GROWTH.md) and [CardBake](CARD_BAKE.md) guides with the
existing Blender helpers. The render output name determines the packed `.blend`
name; use the paths in the manifest. Prepare native TexUtil sample assets first.

After visual inspection, copy only the two README PNGs and social JPEG into `docs/images/`.
Generated layers, embedded-image SVGs, maps, GLBs and packed scenes remain under
ignored `out/`. The source scripts and small JSON scene manifest stay in Git.

The earlier all-in-Blender social composition remains reproducible through
[tools/render_social_banner.py](../tools/render_social_banner.py). The new layout
keeps its composition and updates the typography and export-feature copy.
GitHub's repository social-preview setting is separate from README Markdown;
upload the social JPEG there when desired. These changes do not modify that setting.
