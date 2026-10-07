# Birch tree

Prepare sample assets first with `python3 tools/prepare_samples.py --texutil
../TexUtil/build/texutil`. Source recipes are under `samples/`; executable textured
graphs are under `out/samples/graphs/`. Generated maps and bakes stay under `out/`.
See [sample setup](../samples/README.md).

[samples/graphs/birch.json](../samples/graphs/birch.json) builds a seeded silver-birch study
using the existing TexUtil birch log material. A pale tapered trunk and boughs
support brown branchlets, drooping twigs and pointed, toothed leaf cards. Its
silhouette follows the light crown and hanging outer growth described by the
[RHS](https://www.rhs.org.uk/plants/2261/betula-pendula/details), with triangular
leaves informed by the [Woodland Trust](https://www.woodlandtrust.org.uk/trees-woods-and-wildlife/british-trees/a-z-of-british-trees/silver-birch/).
This is a composed procedural example, not a botanical growth simulation.

## Generate

```sh
./build/foliageutil out/samples/graphs/birch.json --out out/birch --seed 42 --json
./build/foliageutil out/samples/graphs/birch.json --out out/birch/seed-43 --seed 43 --json
```

Bundled PNGs and the leaf atlas manifest make mesh generation independent of a
TexUtil installation. GLB embeds all textures, core PBR materials, atlas UVs,
vertex tint and `_WIND` metadata. The leaf's saved `translucency:0.2` is carried
in material `extras.foliageutil` and applied by the Blender preview adapter.
Other engines need to implement those extras and wind themselves.

Seed 42 produces **295,300 triangles**, **262,082 vertices** and a height of about
**8.72 meters**, including the crown. Its 7.6-meter trunk has 23 primary boughs,
230 branchlets and 1,380 fine twigs. Ten leaf placements per twig give **13,800
individual cards**, with twelve seeded sprite choices. Each card has 3 height
segments and 2 width segments, plus curl, fold, twist, angular jitter, independent
size jitter and vertex tint. Attachment stays at the card base.

This is a detailed tree example. For a cheaper target, lower `leaf_sites.count`,
reduce card subdivisions (and remove deformation controls that need them), and
increase `tube.stride` on fine wood. Reducing branching counts also changes the
canopy structure. No automatic LOD chain, welded branch junctions or wind animation
is generated. Twig tubes overlap parents at their attachments.

## Reused bark and new leaves

[samples/materials/birch/bark.texutil.json](../samples/materials/birch/bark.texutil.json) imports the
packaged `birch-source.texutil.json`. The pale color and healed marks
come from that source, preserving the existing log work. The wrapper builds
OpenGL normals directly from its height fields, since the original log material
exports DirectX normals. It packs roughness into G and metallic zero into B,
retains the cut-end material, and derives a brown young-wood palette for twigs.
The bark maps are 2048 square. Trunk UVs wrap once around the circumference and
repeat 0.72 times per meter along the stem. Boughs use 0.9 repeats per meter.

[leaf.texutil.json](../samples/materials/birch/leaf.texutil.json) defines the pointed blade,
toothed margin, short petiole, central vein, angled secondary veins, pigment,
normal relief and roughness as reusable fields. Small edge warps and pigment
noise change with each sprite seed. This is an approximate leaf pattern rather
than a vein-growth solver or a photograph projected onto a card.

[leaves.texutil.json](../samples/materials/birch/leaves.texutil.json) requests a 4 by 3 atlas
with seeds 91 through 102. Each 256-pixel cell includes four pixels of padding on
every side, leaving 248 pixels of content. Color/alpha, 16-bit OpenGL normal and
packed roughness maps share the 1024 by 768 layout. All channels use the same cell
for each leaf. Texture generation seeds are independent of the foliage graph seed,
which controls the tree structure, placements and sprite selection.

```sh
../TexUtil/build/texutil samples/materials/birch/bark.texutil.json --out out/birch-materials --threads 8 --json
../TexUtil/build/texutil samples/materials/birch/leaves.texutil.json --out out/birch-materials --threads 8 --json
```

After recipe edits, rerun sample preparation and inspect the updated maps under
`out/samples/assets/birch/`. Keep the atlas manifest and image directory together.
The bark wrapper imports the packaged `birch-source.texutil.json`; regeneration
requires a compatible TexUtil executable, with no neighboring source dependency.

## Render and verify

```sh
blender --background --factory-startup --python-exit-code 1 --python tools/render_blender.py -- --model out/birch/birch.glb --out out/birch/birch.png --hdr ../TexUtil/assets/hdri/outdoor.hdr --samples 96 --size 1400 --elevation 8 --azimuth 30 --light-scale 10
blender --background --factory-startup --python-exit-code 1 --python tools/render_blender.py -- --model out/birch/birch.glb --out out/birch/bark-detail.png --hdr ../TexUtil/assets/hdri/outdoor.hdr --samples 64 --size 1000 --target 0.02 0 1.5 --ortho-scale 0.85 --elevation 5 --azimuth 30 --light-scale 10
blender --background --factory-startup --python-exit-code 1 --python tools/render_blender.py -- --model out/birch/birch.glb --out out/birch/leaves-detail.png --hdr ../TexUtil/assets/hdri/outdoor.hdr --samples 64 --size 1000 --target 0.957 -1.398 4.975 --ortho-scale 1.25 --elevation 8 --azimuth 30 --light-scale 10
```

On this Mac use `/Applications/Blender.app/Contents/MacOS/Blender`. The separate
background renderer imports the actual GLB, applies saved leaf optics, and saves
packed `.blend`, PNG and JSON settings. `--light-scale 10` enlarges the original
rose light rig tenfold and scales its power quadratically. The camera automatically
moves far enough back to avoid clipping a tree-sized asset. Explicit targets are
Blender Z-up coordinates. No ground, extra tree geometry, subdivision or retouch
is added by the preview.

The Release suite includes this example. Seed 42 regenerates a byte-identical GLB;
seed 43 produces a different export. Khronos validation reports zero errors and
three runtime tangent-generation warnings for the bark, cut and leaf materials.
Blender 5.2.1 LTS / Cycles CPU renders verify the texture bindings, alpha silhouette
and visible placement. An additional TexUtil Filament render verified solid-card
geometry; that helper does not preview texture alpha or tissue transmission.
