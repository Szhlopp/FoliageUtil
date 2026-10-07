# Configured LOD exports

Add a root `lods` object to export named versions beside the mature asset:

```json
"lods": {
  "LOD1": {"density": 0.5, "topology": 0.5, "tube_stride": 2, "screen_height": 0.3},
  "LOD2": {
    "density": 0.2, "topology": 0.25, "tube_stride": 4,
    "overrides": {"support_twigs": {"sides": 4, "stride": 1}},
    "outputs": ["tree.glb"]
  }
}
```

`support_twigs` must be an actual node name in your graph. Discover the schema
with `foliageutil lods --json` or [lods.json](lods.json). Examples:
[lod-cards](../samples/graphs/lod-cards.json),
[willow](../samples/graphs/weeping-willow.json), and
[sunflower](../samples/graphs/sunflower.json).

- `density` multiplies each `instance.density` and `ribbon.density`. Each instance retains exactly
  `ceil(density * placement_count)` copies, selected using a separate seeded rank.
  Lower densities are nested subsets. For unchanged upstream points/prototypes,
  surviving copies retain their positions, orientation, size, color, wind phase
  and atlas cell, including cycle mode. Zero removes every copy.
- `topology` reduces tube sides, leaf/card subdivisions and ellipsoid rings/sides.
  Values round down and clamp to valid geometry/deformation minima. Curvature
  parameters, crossed-card plane counts and source paths remain unchanged.
- `tube_stride` multiplies tube and ribbon stride, retaining the original path and endpoints.
  This lowers longitudinal surface resolution without resampling branch positions.
- `overrides` patches named nodes after the automatic reductions. Keep essential
  plants at `density:1`, preserve support geometry, or change `planes` explicitly.
  The node operation cannot change. All resulting nodes validate, even unused ones.
- `outputs` optionally restricts a level to a subset of root output filenames.
- `screen_height` is optional manifest metadata in 0..1. Your engine supplies LOD
  switching; no engine-specific extension or runtime transition is exported.

```sh
./build/foliageutil out/samples/graphs/lod-cards.json --out out/lod-cards --json
./build/foliageutil out/samples/graphs/lod-cards.json --out out/base --no-lods
./build/foliageutil out/samples/graphs/lod-cards.json --out out/low --lod LOD2
```

The first command writes the base plus `lod-cards-LOD1.glb` and
`lod-cards-LOD2.glb` (and their configured OBJ/MTL equivalents). `lods.json` records
portable relative paths, counts, bounds, material counts, seed and optional
thresholds. Filenames keep any original subdirectory. Names allow letters,
digits, `_` and `-`, up to 32 characters, with 1..8 case-insensitively unique
profiles. `base` is reserved.

The API's existing `generate`/`write` calls remain base-only. Use `selectLod` for
one derived graph, or `exportGraph` for configured batch export. Geometry budgets
apply separately to each sequentially evaluated variant. Source geometry and
candidate placements still consume their normal budgets before instance reduction.

## Preserve the important geometry

LOD generation resamples graph geometry; it is not arbitrary imported-mesh
simplification, texture resizing, atlas rebaking or impostor generation. Repeated
instances at multiple hierarchy levels each receive the density multiplier, so
use overrides when reducing leaves should not also thin the grove.

Reducing a mesh used for proximity or surface scattering changes its triangles,
which can change placement. Pin that mesh's parameters when exact survivor
positions matter. Keep enough tube sides and stride at solid junctions for shells
to continue intersecting. Lowering resolution can disconnect a union, and
`require_connected` still enforces its contract.

The willow pins all tube inputs to its welded wood and its supporting hanging
strands. It reduces leaves while retaining the connected structure and exact
leaf attachment surfaces:

| Level | Leaves | Triangles | Reduction |
| --- | ---: | ---: | ---: |
| Base | 31,200 | 810,482 | 0% |
| LOD1 | 15,600 | 311,282 | 61.6% |
| LOD2 | 6,240 | 211,442 | 73.9% |

All output paths, sidecars and manifests are checked for collisions and input
asset overwrites before writing begins. Export remains non-atomic: a later
geometry or I/O failure may leave earlier successful files.

For much larger reductions using whole leafy-spray textures, see the willow
`Cards` profile and [packed ribbons](PACKED_CARDS.md). Its mature variant has
52,082 triangles with the original welded wood retained.
