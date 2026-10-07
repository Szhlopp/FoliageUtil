# Solid wood from overlapping tubes

`solidify` boolean-unions the closed shells in a mesh. Use it after merging trunk,
root and branch tubes, before adding foliage. Intersections become connected
surfaces and buried internal faces are removed. [rooted-tree.json](../samples/graphs/rooted-tree.json)
exports both the closed wood (`solid-wood.glb` / `.obj`) and the tree with leaves.

```json
{
  "wood": {"op":"merge", "inputs":["trunk_mesh", "root_mesh", "branch_mesh"]},
  "solid_wood": {"op":"solidify", "input":"wood", "require_connected":true},
  "tree": {"op":"merge", "inputs":["solid_wood", "foliage"]}
}
```

This is a `nodes` excerpt; references must name your actual mesh nodes. Inputs
must be outward-oriented closed manifold shells with positive volume. Capped
`tube` and `ellipsoid` meshes work. Open leaves, cards, uncapped tubes and arbitrary
self-intersecting surfaces are not valid solid inputs. This is not a general mesh
repair or gap-filling operation. Keep branches and roots intersecting their parent.

| Parameter | Meaning |
| --- | --- |
| `require_connected` | Defaults to true. Rejects a result with multiple bodies; false allows disconnected closed solids without inventing connecting geometry. |
| `weld_tolerance` | Seam welding and numerical simplification tolerance in meters. Zero selects a float-precision floor based on coordinate magnitude. A supplied value can increase that floor. It is not a radius for bridging gaps. |
| `crease_angle` | Normal smoothing threshold in degrees, default 180. Lower values preserve sharp shading. Does not bevel or move the surface. |
| `max_input_triangles` | Default 250,000, checked before preparing boolean input. |
| `max_shells` | Default 1,024, checked before the union. |

The Manifold kernel computes the union with double precision. Positions are rounded
to FoliageUtil's float precision while topology is still available, then numerical
degeneracies are simplified before export. Work near the origin and transform
complete assets afterward to preserve detail. The output's geometric and property
vertex counts differ because UV, material and normal seams need separate values.
Duplicated property vertices do not mean that the surface is open.

Surviving faces retain source materials, UVs, colors and wind weights. Newly cut
vertices interpolate those attributes; normals are recalculated. UVs remain the
source's tiled charts, not a unique baking atlas. A texture seam can still be visible
at a fused junction. Optional normal smoothing does not create an organic rounded
crotch or a continuous bark unwrap. GLB retains PBR textures and custom wind data;
OBJ has its existing material/export limitations.

The node report includes `solid.input_shells`, `components`, `watertight`, `volume`
in cubic meters, `tolerance`, `geometric_vertices` and `property_vertices`. Source
vertices are checked against the vertex limit; output vertices and triangles charge
the usual accumulated graph budget before allocating export arrays. Boolean scratch
space is additional. The source/shell bounds are not a total process memory cap.

The seed-42 rooted tree combines 281 wood shells into one body: 138,420 triangles,
69,208 geometric vertices and about 19.30825 cubic meters. Its trunk-and-roots export
also forms one body. Leaves are separate thin surfaces in the complete tree, so use
`solid-wood.glb` when a closed volume is required.

```sh
./build/foliageutil describe solidify
./build/foliageutil out/samples/graphs/rooted-tree.json --out out/rooted-tree --seed 42 --json
node tools/validate_glb.cjs out/rooted-tree/solid-wood.glb
```

The regression suite checks analytic union volumes for overlapping, duplicate,
nested and disconnected cubes, exact geometric edge incidence, opposite winding,
attribute preservation, repeatability, invalid open inputs, and geometry limits.
The example export is also checked independently after GLB serialization. The glTF
validator checks the file format, not whether the geometry is watertight.

## Carving and intersecting solids

Use `boolean` with mesh `input`, mesh `tool` and `operation` set to `union`,
`difference` or `intersection`. It uses the same budgets and attribute-preserving
kernel, accepts already carved solids with cavities, and allows empty intermediate
results. Cut faces inherit tool materials. First union raw overlapping assemblies
with `solidify`. See [rocks, cliffs and crystals](MINERALS.md) for examples and limits.
