# Packed leafy cards

A card can show an entire leafy shoot, including the fine twig joining its leaves.
FoliageUtil's `ribbon` node sweeps these textured strips along the existing paths.
This preserves the willow's hanging shape with much less geometry than separate
leaf meshes and twig tubes. Its alpha pixels define the leaf silhouettes.

```sh
./build/foliageutil out/samples/graphs/weeping-willow.json --out out/willow-cards --lod Cards
./build/foliageutil out/samples/graphs/willow-tree-card-packed.json --out out/willow-tree-card-packed --json
./build/foliageutil describe ribbon
```

The first command selects the saved `Cards` generation profile on the original
graph. Its node override routes the final merge to the welded structure and packed
canopy. The dedicated example exports `willow-tree-card-packed.glb`, OBJ/MTL,
and two lower levels with a portable `lods.json` manifest. It is a separate saved
recipe using the same node names, seed, wood and hanging paths.

| Representation | Canopy triangles | Total triangles | Reduction from original |
| --- | ---: | ---: | ---: |
| Individual leaves and twigs | 772,800 | 810,482 | 0% |
| Packed crossed ribbons | 14,400 | 52,082 | 93.6% |
| Packed LOD1 | 7,200 | 44,882 | 94.5% |
| Packed LOD2, one plane | 2,400 | 40,082 | 95.1% |

All packed levels retain the original 37,682-triangle welded wood. Their 600
ribbons each show 42 leaves per plane. They retain canopy coverage by moving
leaf detail into textures. They do not represent the exact same individual leaves
as the geometry model. The dense crossed representation can look fuller and its
shading differs from independently curved leaves.

## Reusable generation controls

```json
"packed_canopy": {
  "op": "ribbon",
  "input": "strands",
  "width": 0.46,
  "planes": 2,
  "stride": 5,
  "rotation_jitter": 180,
  "atlas": "../assets/willow/packed/sprays.atlas.json",
  "atlas_mode": "random",
  "material": "willow_cards"
}
```

- `width` is physical full width; `width_profile` multiplies it along the path.
  Profile values remain positive, including endpoints, to avoid zero-width quads.
- `stride` retains every Nth path sample and both endpoints. Smaller values track
  the hanging shoulders more closely. It does not modify the source path.
- `planes` makes 1..8 crossed strips around the tangent. A single plane is cheaper
  but loses coverage when viewed edge-on. `rotation` and seeded `rotation_jitter`
  control roll. Frames transport along the sampled path.
- `atlas`, `atlas_mode` and `atlas_index` use the same TexUtil manifest convention
  as instances. One selected cell covers the whole path and all its crossed
  planes. U runs across the strip; V runs from zero at its base to one at its tip,
  by normalized retained arc length. Atlas selection never infers physical size.
- `density` keeps a nested seeded subset of paths. Surviving ribbons retain roll,
  color and atlas cell. `color_variation` darkens each retained spray. Node seeds
  use normal graph isolation rules.

LOD `density` also scales ribbon density, while `tube_stride` multiplies ribbon
stride. Use per-node overrides for plane counts and to preserve essential paths.
The packed willow examples keep density at one and reduce subdivisions/planes.
Geometry budgets are charged before allocating each strip's mesh. Source path
budgets still apply. For growth, stage the source paths before the ribbon node;
a stage directly on a ribbon mesh follows ordinary mesh-pivot scaling rules.

Bind a real RGBA atlas on a double-sided `MASK` material. Keep the visible twig
at the card's base center so it touches supporting wood. PNG top-left and mesh
V conversion is handled by the atlas reader and exporter. Normal maps must derive
from the final packed layout rather than rotating already encoded normal pixels.
The [native texture package](../samples/materials/willow/packed/README.md) supplies six
aligned variations, including alpha, normal and packed roughness.

The `ribbon` node uses authored atlases. For textures captured from generated
foliage, use the separate [CardBake export](CARD_BAKE.md). It fits strips to guide
paths and assigns a unique baked cell to every strand and plane. The actual
triangles are rectangular strips under the alpha stencil. Proximity queries see
those triangles, not leaf alpha. Do not use these strips as proof that arbitrary
points touch visible foliage. Keep solid wood queries for physical attachments.

Fewer triangles do not guarantee a proportional frame-rate improvement. Dense
transparent regions add overdraw, fill-rate cost, shadow work and mipmap/alpha
coverage considerations. Check in the intended engine and at its actual viewing
distances. Full geometry remains preferable for close-up individual leaves.

## Reproduce the comparison

```sh
blender --background --factory-startup --python-exit-code 1 --python tools/render_variants.py -- --models out/willow-lods/weeping-willow.glb out/willow-tree-card-packed/willow-tree-card-packed.glb out/willow-tree-card-packed/willow-tree-card-packed-LOD2.glb --labels "Geometry leaves: 810,482 triangles" "Packed cards: 52,082 triangles" "Packed LOD2: 40,082 triangles" --hdr ../TexUtil/assets/hdri/outdoor.hdr --out out/willow-tree-card-packed/comparison.png --width 1800 --height 780 --samples 64
blender --background --factory-startup --python-exit-code 1 --python tools/render_blender.py -- --model out/willow-tree-card-packed/willow-tree-card-packed.glb --out out/willow-tree-card-packed/willow-tree-card-packed.png --hdr ../TexUtil/assets/hdri/outdoor.hdr --samples 64 --size 1200 --light-scale 8 --elevation 8 --azimuth 20 --ground-height 0
```

The comparison uses the original mature geometry export from the LOD study.
Development renders use Blender 5.2.1 LTS and Cycles CPU. Scenes and settings
manifests are saved beside the PNGs; the renderer uses the exported materials
and does not replace the foliage geometry.
