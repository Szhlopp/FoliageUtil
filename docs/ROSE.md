# Rose and petal study

Prepare sample assets first with `python3 tools/prepare_samples.py --texutil
../TexUtil/build/texutil`. Source recipes are under `samples/`; executable textured
graphs are under `out/samples/graphs/`. Generated maps and bakes stay under `out/`.
See [sample setup](../samples/README.md).

The [detailed rose](../samples/graphs/rose-detailed.json) is a working example of the
TexUtil-to-FoliageUtil sprite workflow. Forty petals use twelve seeded texture
variants, an alpha silhouette, inward-curved card geometry and saved optical
settings. The example runs with prepared assets; TexUtil is needed only to rebuild
the textures. This is a procedural study inspired by the reference, not a botanical
simulation or a reproduction of the photograph.

## Geometry, stencil and material

| Feature | Implementation |
| --- | --- |
| Broad fan, narrow attachment, irregular rim | TexUtil alpha stencil, with seeded edge warp. |
| Inward cup, lengthwise bend, twist | Subdivided FoliageUtil `card` geometry. |
| Fine veins and fibers | Shared warped fan and cellular fields drive pigment and normal relief. This approximates venation; it is not a branching-path generator. |
| Variation | Twelve synchronized color/normal/roughness sprites; seeded cell choice and local width/height/depth jitter. |
| Tissue backlighting | Saved `translucency:0.16`, separate from alpha coverage. |
| Soft surface response | Saved `subsurface:0.12`, RGB radius `[1,0.45,0.25]`, scale `0.001` meters. |

The inner, middle and outer layers contain 12, 18 and 10 petals, respectively.
Their widths, opening angles and folds vary by layer. Each card has 40 longitudinal
and 16 transverse segments. The full rose has **60,500 triangles and 33,207
vertices**, including stem, calyx, sepals and compound leaves. This is a close-up
example. Reduce segment counts for a game asset; the maps and atlas indices still
work. There is no subdivision modifier, welded petal bond or collision solver.

In radial placement, local +Z faces away from the bloom. Negative `fold` cups the
side edges toward the center; negative `curl` bends the tip inward. Petal bases
cluster around the calyx to avoid visibly detached outer petals. UVs describe the
flat card before deformation, then select one atlas cell per petal.

The [basic rose](../samples/graphs/rose.json) remains a smaller geometry-only example:
11,912 triangles, 38 petals, and 20 length by 8 width segments.

## Regenerate the atlas

```sh
../TexUtil/build/texutil samples/materials/rose/petals.texutil.json --out out/rose-atlas-materials --threads 8 --json
```

[petals.texutil.json](../samples/materials/rose/petals.texutil.json) requests a 4 by 3 grid.
Each cell is 384 pixels including 6 pixels of padding on each side, giving 372
pixels of content and a 1536 by 1152 atlas. Seeds 42 through 53 drive the twelve
variants. [petal-sprite.texutil.json](../samples/materials/rose/petal-sprite.texutil.json)
contains the reusable source graph. Unlike the earlier petal material, its noise
nodes are not pinned to explicit seeds, so variants actually differ.

The atlas channels share cell bounds and seeds. Base color contains sRGB pigment
and linear coverage alpha; normals are 16-bit OpenGL data; packed G is roughness
and B is metallic zero. A separate height atlas is retained for inspection and
future uses, but does not displace the geometry. Color and normal relief use the
same vein field. The outline and its noise are independent of fine venation.

After edits, rerun preparation and inspect `petals.atlas.json` and its entire
`petals.atlas.assets` directory under `out/samples/assets/rose/`. Foliage material
bindings and the instance manifest path must refer to that same package.
The original [petal.texutil.json](../samples/materials/rose/petal.texutil.json) and single-map
PNGs remain available for the earlier texture study. The leaf material still uses
[leaf.texutil.json](../samples/materials/rose/leaf.texutil.json).

## Saved optical controls

`foliageutil materials --json` exposes the material schema. See
[MATERIALS.md](MATERIALS.md). Nonzero tissue settings are written to GLB material
`extras.foliageutil`, with `version:1`. The Blender helper automatically reads them
and builds a thin-surface diffuse transmission mix using textured base color,
vertex tint and tangent normals, plus Burley surface diffusion. Alpha coverage is
applied after that mix, preserving fully transparent stencil holes.

These are FoliageUtil preview semantics, not a standard glTF optical extension.
Other GLB viewers retain core PBR but require an adapter for these extras. OBJ/MTL
and the current solid-color Filament helper do not reproduce the optical effects.
The source recipe, exported GLB, packed Blender scene and render manifest all
retain the settings. No manual Blender material edits are required.

The current scalar transmission and base-color tint approximate thin tissue.
Spatial thickness/transmission maps and a coherent branching-vein generator remain
future controls; see [SURFACES.md](SURFACES.md). A geometric thickness volume is not
added to these open cards.

## Render the actual exports

```sh
./build/foliageutil out/samples/graphs/rose-detailed.json --out out/rose-atlas
blender --background --factory-startup --python-exit-code 1 --python tools/render_blender.py -- --model out/rose-atlas/rose-detailed.glb --out out/rose-atlas/rose.png --hdr ../TexUtil/assets/hdri/studio.hdr --samples 96 --size 1000 --elevation 18
blender --background --factory-startup --python-exit-code 1 --python tools/render_blender.py -- --model out/rose-atlas/rose-detailed.glb --out out/rose-atlas/closeup.png --hdr ../TexUtil/assets/hdri/studio.hdr --samples 128 --size 1000 --focus bloom --elevation 45
```

On this Mac use `/Applications/Blender.app/Contents/MacOS/Blender` as the executable.
The helper runs separately with factory startup and saves packed `.blend`, PNG and
JSON settings. These views use Cycles CPU, eight workers, seed 42, denoising, AgX,
exposure zero, the TexUtil studio HDR and three softboxes. No mesh modifiers or
post-render retouch are applied. `--petal-subsurface` is an optional diagnostic
override; omission uses the recipe, and zero disables saved petal subsurface.

## Inspect a single petal

[petal-study.json](../samples/graphs/petal-study.json) isolates one fixed atlas cell on a
curved card with the rose's saved material. This is a reusable starting point for
editing the surface before assembling a whole plant.

```sh
./build/foliageutil out/samples/graphs/petal-study.json --out out/petal-study
blender --background --factory-startup --python-exit-code 1 --python tools/render_blender.py -- --model out/petal-study/petal-study.glb --out out/petal-study/backlit.png --hdr ../TexUtil/assets/hdri/studio.hdr --samples 64 --size 800 --elevation 0 --azimuth 0 --lighting backlit
```

`--lighting backlit` puts a soft light behind the tissue with weak front fill.
Use `studio` for the usual lighting. Keep camera and lights fixed when comparing
optical settings. This preset is scaled for the meter-based rose study.

The opaque control uses the same geometry, atlas cell, camera and backlighting,
with both tissue settings set to zero. It remains dark; the saved optical settings
allow the backlight to illuminate the petal while stencil holes remain clear.

## Verification and earlier UV finding

Release tests cover saved material bounds, validation of unused materials, GLB
optical metadata and preservation of alpha coverage and texture data. The rose
GLB passes Khronos validation with zero errors and four runtime tangent-generation
warnings. Blender builds the tangent basis from the exported UVs. See
[VALIDATION.md](VALIDATION.md) for the full suite.

The earlier single-texture rose had valid UVs. Its initially faint texture came
from low map contrast and relief, rather than missing UVs. Its checker study showed
expected compression near tapered mesh tips. The current cards avoid that tip
pinching: the fan outline is an alpha stencil inside a rectangular UV domain.
The per-petal UVs select sprite cells; they are not a unique mesh bake atlas.
