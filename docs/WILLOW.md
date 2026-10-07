# Weeping willow

Prepare sample assets first with `python3 tools/prepare_samples.py --texutil
../TexUtil/build/texutil`. Source recipes are under `samples/`; executable textured
graphs are under `out/samples/graphs/`. Generated maps and bakes stay under `out/`.
See [sample setup](../samples/README.md).

[The recipe](../samples/graphs/weeping-willow.json) builds a spreading rooted trunk,
ten arching boughs, sixty side sprays, and six hundred hanging stems carrying
31,200 narrow leaves. It uses generic nodes, so the same construction can make
other pendant foliage, vines or trailing plants. Seed 42 is the illustrated tree.

```sh
./build/foliageutil out/samples/graphs/weeping-willow.json --out out/weeping-willow --seed 42 --json
./build/foliageutil describe grow
```

The full tree is exported as `weeping-willow.glb` and `weeping-willow.obj`.
`willow-structure.glb` contains the fused trunk, roots, boughs and side sprays for
inspecting the scaffold. The fine strands overlap their supports; they are retained
as separate closed tubes. Leaves are open surfaces. The complete tree is therefore
not a single watertight solid, while the exported structural wood is.

## Shape the hanging stems

`branch` creates attached paths with independent seeded lengths and directions.
`grow` then turns each segment toward a direction while retaining its length and
keeping the stem base fixed. This example introduces two reusable `grow` controls:

| Control | Meaning |
| --- | --- |
| `strength_profile` | 2..32 `[position, multiplier]` keys over the retained arc length, from position 0 to 1. Multiplies `strength`. Zero retains the source tangent; one applies full strength. |
| `interpolation:"spherical"` | Turns through the angle between the source tangent and target direction. Opposite directions use a deterministic perpendicular plane. |

The defaults remain `interpolation:"linear"` and `strength_profile:[[0,0],[1,1]]`,
preserving the previous linear ramp. The profile is evaluated at each segment's
endpoint. Increase source path `segments` to sample short transitions smoothly.
Spherical interpolation does not resample, add vertices, simulate gravity or solve
collisions. Exactly opposite directions have no unique physical bending plane.

```json
{
  "op": "grow",
  "input": "strand_growth",
  "direction": [0, -1, 0],
  "strength": 1,
  "interpolation": "spherical",
  "strength_profile": [[0, 0], [0.12, 0.38], [0.33, 0.92], [0.65, 0.995], [1, 1]]
}
```

An early rise toward one makes a long vertical tail. Delaying that rise creates a
wider shoulder. Lowering `strength` leaves more of the original branch direction.
`branch.length`, `length_variation`, `angle` and `angle_jitter` change the drape
and spacing. There is no ground clamp: review clearance when changing lengths,
crown height or seed. Root burial follows the separate flat-ground root controls.

The graph deforms boughs before attaching side sprays, deforms sprays before
attaching strands, and finally scatters leaves onto those strands. This order
keeps attachment positions valid. Do not deform an already assembled tree with
`grow`; it does not retain parent-child constraints.

## Leaves and materials

The leaves use `leaf.shape:"lanceolate"`, six length segments, two width segments,
small curl/fold/twist, and independent instance size variation. Their bases stay at
the placement pivot. Scatter checks the final strand mesh and snaps accepted
pivots onto its triangles. Seed 42 accepts and snaps all 31,200 candidates.

The example uses dedicated TexUtil [willow bark materials](../samples/materials/willow/README.md):
dark gray-brown mature bark with irregular longitudinal furrows and fibrous relief,
plus smoother brown young branch bark on the side sprays. The finest strands use
a brown scalar material. The leaves reuse eight opaque vein/pigment variants from the
[bamboo leaf atlas](../samples/materials/bamboo/README.md), with a willow leaf color factor.
The willow's narrow silhouettes come from geometry, not rectangular alpha cards.
Each leaf receives a seeded atlas cell with matching color, normal and roughness
maps. No TexUtil build is needed to generate the prepared example.

Saved `translucency:0.24` is applied by the Blender material adapter. Other viewers
need an adapter for these optical extras. The normal maps provide small surface
relief; the trunk flare and major bark lobes are actual mesh geometry.

Regenerate the bark and young branch maps with:

```sh
../TexUtil/build/texutil samples/materials/willow/bark.texutil.json --out out/willow-material/maps --threads 8 --json
```

Rerun preparation after recipe edits and inspect the material sheet with its
matching maps under `out/samples/assets/willow/`, then regenerate exports and previews.

## Render and density

```sh
blender --background --factory-startup --python-exit-code 1 --python tools/render_blender.py -- --model out/weeping-willow/weeping-willow.glb --out out/weeping-willow/weeping-willow.png --samples 96 --size 1200 --threads 8 --light-scale 8 --elevation 8 --azimuth 20 --ground-height 0
```

The preview is an actual GLB import rendered in Blender Cycles, with the saved
materials and a separate presentation ground. The helper also saves a packed
`.blend` scene and JSON render manifest alongside the image.

This is a detailed example with about 810,000 triangles, not a game-ready LOD.
Reduce `strand_growth.count`, `leaf_sites.count`, leaf `segments`, and tube `stride`
for cheaper geometry. A card LOD needs a suitable strand stencil atlas; the opaque
leaf atlas used here cannot mask an entire rectangular branch card.
