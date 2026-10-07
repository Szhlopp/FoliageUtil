# Rocks, cliffs and crystals

`rock`, `crystal` and `boolean` extend the same seeded evaluator used for foliage.
They work through the CLI, C++ library, native C API and Unity wrapper. Existing
placement, instancing and material nodes compose them into formations. Query
`foliageutil describe rock`, `describe crystal` and `describe boolean` for bounds.

## Examples

These recipes use authored material factors and procedural vertex colors, with no
texture dependencies. They run directly from source and travel in the prepared
sample bundle.

| Recipe | Outputs and construction |
| --- | --- |
| [rocks](../samples/graphs/rocks.json) | Granite boulder, layered sandstone and slate shard, with a `Low` LOD |
| [cliff](../samples/graphs/cliff.json) | Fused ledge, subtracted weathered arch, intersected flat ground boundary |
| [rock-formation](../samples/graphs/rock-formation.json) | Four independent stones unioned into one closed outcrop |
| [gems](../samples/graphs/gems.json) | Separate diamond, ruby and emerald studies, plus a combined display |
| [crystal-cluster](../samples/graphs/crystal-cluster.json) | Amethyst prism and tilted cluster unioned into a stone matrix |

```sh
./build/foliageutil samples/graphs/rocks.json --out out/minerals/rocks --seed 42 --json
./build/foliageutil samples/graphs/cliff.json --out out/minerals/cliff --json
./build/foliageutil samples/graphs/rock-formation.json --out out/minerals/rock-formation --json
./build/foliageutil samples/graphs/gems.json --out out/minerals/gems --json
./build/foliageutil samples/graphs/crystal-cluster.json --out out/minerals/crystal-cluster --json
```

Change the root seed to vary rocks, crystal corner jitter and placements. Regular
gem profiles with zero jitter deliberately do not change with seed. Explicit node
seeds isolate that node from root changes. Names still identify independent random
streams. An `instance` repeats one prototype shape, while size, rotation and color
can vary. Use separately named primitive nodes for independent shapes in a formation.

## Rock shape

`size` is the nominal full XYZ extent, centered at the origin before weathering.
`roundness:1` begins with an ellipsoid; smaller values make rounded blocks.
`noise`, `noise_scale` and `noise_octaves` apply coherent radial weathering in
normalized coordinates, so resizing retains the pattern. `strata` and
`strata_frequency` add horizontal layering. Actual bounds vary with seed. This is
stylized geometric shaping, not geological erosion simulation.

`subdivisions` refines an icosphere: 20, 80, 320, 1,280, 5,120 or 20,480 triangles.
Use enough detail to resolve noise and layers. `shading` selects flat facets or
smooth normals, retaining creases where smoothing would invert shading.
`color_variation` darkens vertices independently of geometry. Bind a stone material
for color and roughness.

UVs use spherical projection, split at the seam. Some U values exceed one to keep
wrapping continuous within seam triangles. They suit repeating textures, not a
unique bake or `instance.atlas`. Vertices split per triangle for UVs/normals;
their geometric positions still form a watertight surface.

## Crystal profiles

Crystals stand on Y=0 and extend along +Y to `height`. `radius` is the polygon's
circumradius and `sides` controls facets. `radius_profile` has 2..32
`[height_fraction, radius_multiplier]` keys. Each key creates a ring. Zero is
allowed only at endpoints, creating an apex. Positive endpoints receive flat caps.
Interior radii must be positive; nonzero multipliers must be at least 0.0001,
and at least one key must be positive. Features that
collapse at float precision fail.

```json
{
  "op": "crystal",
  "sides": 8,
  "radius": 0.64,
  "height": 0.84,
  "radius_profile": [[0,0],[0.56,1],[0.61,1],[0.82,0.79],[1,0.48]],
  "material": "diamond"
}
```

This forms a lower point, girdle, crown and table. Other profiles create prisms,
double points and stepped gems. Flat normals preserve facets, and UVs stay in
0..1. `radial_jitter` varies polygon corners using one multiplier per corner shared
by all rings, retaining planar prism faces. These are editable stylized cuts, not
calibrated jewelry designs.

Embed cluster bases slightly inside the rock. Use `radial.tilt`, `radial.scale`
and `instance.scale_jitter` for outward sprays, or mesh scatter with
`normal_direction:[0,1,0]`, a surface-angle limit and `angle:0` to align +Y with
surface normals. Scatter count is attempted placements. Merge overlapping crystals
with their matrix and use `solidify` for one closed asset. `crease_angle:0` retains
flat facets.

## Boolean construction

```json
{"op":"boolean","input":"cliff_body","tool":"cave_cutter","operation":"difference","crease_angle":35}
```

`union` combines volume, `difference` removes the tool from the input, and
`intersection` retains their common volume. Operands must be oriented, closed,
non-self-intersecting solids in matching coordinates. First pass raw overlapping
shell assemblies through `solidify`. Open leaves/cards are not cutters. This is
not mesh repair or gap filling.

Both operands contribute material, UV, vertex-color and wind properties. New cuts
inherit the cutter's material. Coplanar coincident boundaries can inherit either
operand's properties. Normals are recalculated and oriented against final winding.
UVs are inherited/interpolated, not newly unwrapped across the seam.

Cavities survive subsequent `boolean` operations. Use `boolean` to combine carved
solids; `solidify` expects outward shells and cannot treat an inward cavity boundary
as an independent union operand. `require_connected:true` requires one positive
body; an enclosed cavity is not a second body. False allows disconnected pieces.

An empty intersection or complete subtraction can feed another Boolean. An empty
final output fails ordinary export under the existing graph contract. Growth and
native hosts may accept empty meshes. The `solid` report includes the operation,
input shells, positive bodies, boundary shells, volume and geometry counts.
Input triangle/shell limits cover both operands. Global geometry budgets are
charged before output allocation; kernel scratch memory is additional. Features
below the coordinate-dependent float tolerance may disappear. See [SOLIDS.md](SOLIDS.md).

The global LOD topology factor leaves these primitive shapes unchanged. Use node
overrides for `rock.subdivisions` and `crystal.sides`. Changing sides changes a gem's
cut; reducing Boolean operands can change intersections. Recheck each LOD's shape
and connectivity.

## Gem materials and engine support

GLB exports `transmission`, `ior`, `thickness`, `attenuation_color` and optional
`attenuation_distance` through standard Khronos
[transmission](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_transmission),
[IOR](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_ior) and
[volume](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_volume)
extensions. Keep gemstone alpha at one, `alpha_mode:"OPAQUE"` and `metallic:0`.
Transmission controls light entering the surface; alpha controls its coverage.
Thin foliage `translucency` is separate.

Positive thickness requires transmission and a closed mesh. Rasterizers approximate
volume thickness; ray tracers use actual geometry. Attenuation distance uses world
meters, so large and jewelry-sized gems need different settings. Omitting distance
disables volume absorption. Spectral dispersion is not implemented. OBJ/MTL and
viewers lacking these extensions use the basic material fallback.

Blender imports the GLB optical extensions directly. The supplied Unity foliage
shader renders geometry, color and basic PBR, but does not implement volume
refraction or custom IOR. Native material JSON retains optical fields for custom
adapters. CardBake rejects transmissive/custom-IOR sources because cards would
discard those optics.

## Verification and reproduction

`foliage_mineral_tests` checks seeds, profile endpoints, analytic frustum/cube
volumes, exact two-face edge incidence, winding and cut normals, materials, cavities,
chained/empty operations, invalid unused nodes, repeatability and budgets. Core
export tests inspect actual GLB optics; native and managed smoke cover all samples.

Render actual exports from two angles:

```sh
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python tools/render_blender.py -- --model out/minerals/crystal-cluster/crystal-cluster.glb --out out/minerals/renders/crystal-cluster-25.png --samples 48 --size 700 --azimuth 25 --elevation 26 --ground-height 0
# Repeat at --azimuth 145. Use --light-scale 5 for the cliff.
node tools/validate_glb.cjs out/minerals/gems/gems.glb out/minerals/cliff/cliff.glb
```

Unity's mineral smoke checks seed changes/backward regeneration, mesh upload,
material slots, tangents, shader compilation, culling and two views in an isolated
project. It explicitly records `refractive_shader:false`. Add `--urp-version` for
the matching editor package.

```sh
python3 tools/build_unity_package.py --arch universal --out out/unity-minerals-package
python3 tools/test_unity.py --editor /Applications/Unity/Hub/Editor/6000.4.6f1/Unity.app/Contents/MacOS/Unity --package out/unity-minerals-package --recipe samples/graphs/crystal-cluster.json --project out/unity-minerals-test --name crystal-cluster --minerals
```

Keep generated GLBs, renders, scenes, logs and plugins under `out/`.
