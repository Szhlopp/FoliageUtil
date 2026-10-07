# Sprite variants on foliage

Prepare sample assets first with `python3 tools/prepare_samples.py --texutil
../TexUtil/build/texutil`. Source recipes are under `samples/`; executable textured
graphs are under `out/samples/graphs/`. Generated maps and bakes stay under `out/`.
See [sample setup](../samples/README.md).

TexUtil's native `spritesheet` output generates seeded variants and a shared
manifest. FoliageUtil reads `instance.atlas` and assigns one occupied cell to each
copy of a prototype. All vertices in the copy, including crossed planes, use that
cell. One UV set selects matching color, normal and packed roughness textures.

```json
{
  "op": "instance",
  "input": "leaf_card",
  "points": "leaf_sites",
  "atlas": "../assets/spritesheets/leaves.atlas.json",
  "atlas_mode": "random",
  "scale_jitter": [0.10, 0.15, 0],
  "seed": 42
}
```

Bind the corresponding atlas PNGs in the prototype's material. The complete
[cards-spritesheet example](../samples/graphs/cards-spritesheet.json) includes those
bindings and a comparison. Its prepared atlas works without TexUtil installed.

| Control | Effect |
| --- | --- |
| `atlas` | TexUtil spritesheet JSON path, relative to the foliage recipe. |
| `atlas_mode:"random"` | Seeded selection of an occupied cell per placement. |
| `atlas_mode:"cycle"` | Cell 0, 1, 2, etc., wrapping at the occupied count. |
| `atlas_mode:"fixed"` | Use `atlas_index`, a zero-based occupied cell index. |
| `scale_jitter:[x,y,z]` | Independent local dimension variation after shaping the prototype. |

Atlas selection uses a separate random stream from size and color. Enabling it
does not change geometry or colors; size jitter does not change sprite choices.
Explicit instance seeds isolate selection from the root seed. A root seed change
can still alter upstream geometry. Random selection can repeat cells; use cycle
to enumerate them.

Source UVs must be within 0..1. `card.uv_rect` selects a subregion inside each
chosen cell; leave it at the default to use the whole content rectangle. UVs stay
attached when cards curve, fold, twist, squash or stretch. Cell selection does
not change physical card dimensions; choose their aspect ratio deliberately.

The reader checks manifest version, row-major indices, occupied count, grid,
padding, pixel/UV agreement and actual PNG dimensions. Content bounds exclude
padding. The PNG's top-left origin is converted into internal mesh UVs, and GLB
export performs its existing single V flip. Do not add another flip. Manifest
and image assets are protected from export overwrite.

## Regenerate and inspect

From the FoliageUtil root, prepare the maps with a compatible TexUtil executable.
The optional Blender command below uses an HDR from the neighboring TexUtil checkout;
substitute your own HDR path if that checkout is unavailable.

```sh
python3 tools/prepare_samples.py --texutil ../TexUtil/build/texutil
./build/foliageutil out/samples/graphs/cards-spritesheet.json --out out/cards-spritesheet --json
blender --background --factory-startup --python-exit-code 1 --python tools/render_blender.py -- --model out/cards-spritesheet/cards-spritesheet.glb --out out/cards-spritesheet/blender.png --hdr ../TexUtil/assets/hdri/studio.hdr --size 1400 --samples 96 --elevation 8 --azimuth 12
```

The foliage example reads `out/samples/assets/spritesheets/`. Preparation updates
the manifest and entire `.assets` directory together. To move the prepared bundle,
copy all of `out/samples/`, retaining graph, manifest and material relative paths.

The [detailed rose](../samples/graphs/rose-detailed.json) applies this workflow to twelve
fan-shaped petal stencils, curved cards and saved tissue optics. See [ROSE.md](ROSE.md).
PNG alpha coverage alone does not add tissue transmission or subsurface. The rose's
current vein network is a procedural approximation; coherent branching vein paths
and thickness maps remain further work in [SURFACES.md](SURFACES.md).

## More working atlases

[Bamboo](BAMBOO.md) assigns eight vein/pigment variants to geometry leaves and
their short sheaths. [Lily pads](LILY_PAD.md) assign six notched alpha stencils
to gently cupped cards. Their source recipes and generated manifests use the same contract.

When a leaf looks detached, check its visible base as well as the card pivot.
Transparent sprite margins can leave an apparent gap. [Scatter proximity](SCATTER_PROXIMITY.md)
checks and optionally snaps the pivot to supporting triangles; it does not inspect
alpha or move a silhouette within the card.
