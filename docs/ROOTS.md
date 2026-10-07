# Rooted trunks and fading profiles

Prepare sample assets first with `python3 tools/prepare_samples.py --texutil
../TexUtil/build/texutil`. Source recipes are under `samples/`; executable textured
graphs are under `out/samples/graphs/`. Generated maps and bakes stay under `out/`.
See [sample setup](../samples/README.md).

The broad structure in a mature tree comes from a flared base, irregular trunk
lobes, large spreading limbs and roots that enter the soil. These are geometry
controls. Bark fissures, lichen and fine surface detail can come from TexUtil maps.

[The rooted-tree recipe](../samples/graphs/rooted-tree.json) combines a 5.8-meter trunk,
eight large buttress roots, thirteen smaller surface roots, three generations of
branches and 4,200 geometry leaves. The complete tree reaches about 12 meters.
Its woody structure is fused by `solidify`, with seeded TexUtil bark maps on the
surviving UVs. It is a procedural example, not a reconstruction of a particular tree.
See [SOLIDS.md](SOLIDS.md) for closed-mesh requirements and
[the material package](../samples/materials/rooted-tree/README.md) for bark generation.

## Root controls

`roots` accepts a skeleton and produces roots for each input stem. Its output
contains children only. Pass it to `tube`, then merge that mesh with the trunk.
Existing `branch`, `grow`, `prune`, `transform` and other skeleton operations also
accept the resulting paths.

| Control | Effect |
| --- | --- |
| `count`, `angle`, `angle_jitter` | Root count and seeded radial arrangement around world +Y |
| `attachment` | Attachment along the parent's normalized arc length; the base starts on its centerline |
| `length`, `length_variation` | Horizontal reach in meters and seeded size variation |
| `ground_height`, `surface_at` | World-Y soil plane and the fraction of reach by which the centerline descends to it |
| `bury_depth` | Tip centerline depth below that plane; allow for tip thickness to hide the entire cap |
| `radius_scale`, `radius_variation`, `taper` | Root thickness relative to the parent, variation, and tip thinning |
| `radius_profile` | Further radius shaping along the root |
| `noise`, `noise_scale`, `noise_profile` | Coherent sideways meander in meters, frequency and fading; the attachment stays fixed |
| `segments` | Geometry samples along each root before tubing |

```json
{
  "op": "roots",
  "input": "trunk",
  "count": 8,
  "attachment": 0.29,
  "length": 4.4,
  "radius_scale": 0.72,
  "ground_height": 0,
  "surface_at": 0.6,
  "bury_depth": 0.32,
  "noise": 0.16,
  "noise_profile": [[0, 0], [0.2, 0.15], [0.6, 1], [1, 0.3]],
  "segments": 64
}
```

The centerline descends quadratically toward the plane during `surface_at`, with
burial increasing toward the tip. Roots are radial in world XZ even if the parent
leans. The ground height is explicit; there is no terrain mesh sampling or collision
avoidance. Applying `grow`, `branch` or a transform afterward can change the ground
relationship, so inspect the final plant. Start underground or near the soil for
smaller roots, and higher on the trunk for tall buttresses.

## Profiles and noise fading

A profile contains 2..32 `[position, multiplier]` keys. Positions must increase
strictly from 0 to 1. Values interpolate linearly. Noise and ridge multipliers
are within 0..1; radius multipliers are within 0.001..10. Zero radius is excluded
to preserve valid tube ends. The default `[[0,1],[1,1]]` keeps existing behavior.

`trunk.flare` is an additional fractional base radius. `flare_length` controls
how far the flare extends and `flare_power` controls how concentrated it is.
`radius_profile` multiplies the radius after taper and flare. It is also available
on `branch`, `curve` and `roots` for bulges and narrower sections.

On `tube`, use a low ridge count for lobes, then fade their depth up the trunk.
Radial surface noise gives those shapes less regular outlines:

```json
{
  "op": "tube",
  "input": "trunk",
  "sides": 64,
  "ridges": 8,
  "ridge_depth": 0.27,
  "ridge_profile": [[0, 1], [0.18, 0.9], [0.45, 0.18], [1, 0.04]],
  "radius_noise": 0.18,
  "noise_scale": 1.4,
  "noise_octaves": 3,
  "noise_profile": [[0, 0], [0.2, 1], [0.7, 0.5], [1, 0]]
}
```

This displaces the surface while preserving both end rings. It does not change
the skeleton or downstream skeleton attachment positions. Use the existing
scatter proximity controls when a placement must match the displaced surface.

The seeded surface noise is coherent value noise sampled in world space. Each
octave doubles frequency and halves amplitude; their sum is normalized within
the requested fractional radius bound. UV seams share displacement and normals.
It is sampled on existing vertices, so higher frequencies require enough path
segments and tube sides. `stride` remains available for structural LOD.

Tube profiles use normalized swept arc length; curve profiles use normalized
polyline arc length. Trunk and branch profiles use normalized growth steps.
Root profiles use normalized horizontal reach. A directional `noise_profile` on
trunk or branch scales each new random turn: fading it out does not straighten
turns already accumulated. Root noise changes sideways displacement only, so
fading it does not disturb the prescribed vertical descent.

All randomness follows root/node seed rules. An explicit seed isolates that node's
random stream, but upstream geometry can still change its inputs. Surface noise
defaults to zero, and profiles default to one. Existing recipes keep their shape.

## Generate and inspect

```sh
./build/foliageutil describe roots
./build/foliageutil out/samples/graphs/rooted-tree.json --out out/rooted-tree --seed 42 --json
python3 tools/preview.py out/samples/graphs/rooted-tree.json --texutil ../TexUtil/build/texutil --out out/rooted-tree-filament --elevation 16 --azimuth 25 --size 1100
blender --background --factory-startup --python-exit-code 1 --python tools/render_blender.py -- --model out/rooted-tree/structure.glb --out out/rooted-tree/rooted-tree-base.png --hdr ../TexUtil/assets/hdri/outdoor.hdr --size 1400 --samples 96 --light-scale 7 --ground-height 0 --elevation 16 --azimuth 24 --target 0 0 2.35 --ortho-scale 8.8
blender --background --factory-startup --python-exit-code 1 --python tools/render_blender.py -- --model out/rooted-tree/rooted-tree.glb --out out/rooted-tree/rooted-tree.png --hdr ../TexUtil/assets/hdri/outdoor.hdr --size 1400 --samples 96 --light-scale 12 --ground-height 0 --elevation 13 --azimuth 24
```

Use the installed Blender executable, on this Mac
`/Applications/Blender.app/Contents/MacOS/Blender`. Its optional `--ground-height`
adds a presentation plane to test contact shadows and burial. It does not add
ground to the GLB. The packed Blender scene and render manifest remain in `out/`.

| Export | Triangles | Purpose |
| --- | ---: | --- |
| `rooted-tree.glb` / `.obj` | 340,020 | Complete tree |
| `structure.glb` / `solid-wood.glb` / `.obj` | 138,420 | Trunk, roots and all branches |
| `trunk-roots.glb` | 41,446 | Isolated trunk and roots |
| `roots.glb` | 40,248 | Roots only |

The recipe unions wood before adding leaves. Root-only output intentionally keeps
the separate root shells for inspection. Fusion removes internal faces, but source
UV charts and intersection creases can remain visible. Root count does not imply
spacing avoidance, erosion, soil simulation or botanical growth.
