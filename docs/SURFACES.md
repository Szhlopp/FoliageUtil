# Leaf, petal and stencil surface controls

The target is the single pink petal supplied in the task: a narrow attachment,
broad fan-shaped rim, small edge irregularities, branching veins, pigment that
fades toward the rim, and gentle translucency. This is a development target, not
a claim that the current rose matches it. The reference image is not bundled.

Most fine detail should come from texture fields. Geometry should carry the
silhouette where a cutout cannot, the broad cup, rolled edges, and folds that
change the outline or cast a visible shadow. A thin grid with a cutout stencil
can represent the same surface at a lower geometry budget.

## Available now

- `leaf`: length, width, four fixed profiles, longitudinal curl, crosswise fold,
  twist, and length/width subdivisions. Its present topology closes both ends
  at single vertices, which limits broad petal rims.
- `card`: a rectangular or crossed grid, atlas rectangle, base/center pivot,
  subdivisions, curl, fold and twist. Alpha textures define its visible outline.
- `instance.scale_jitter`: seeded independent local XYZ size multipliers. Values
  are fractional half-ranges, such as `[0.1,0.15,0]` for width +/-10%, height
  +/-15%. Positive scaling preserves winding; inverse scaling corrects normals.
  UVs remain unchanged, so the same image naturally squashes and stretches.
- Existing scatter/radial uniform scale, orientation jitter and vertex tint can
  compose with those controls. Explicit instance seeds isolate its size/color
  stream from root seed changes, provided upstream geometry/placements stay fixed.
- TexUtil `spritesheet` output generates synchronized seeded texture atlases;
  FoliageUtil `instance.atlas` reads their layout for random, cycling or fixed
  selection. See [SPRITESHEETS.md](SPRITESHEETS.md).
- TexUtil builds color, normal, roughness and alpha maps. Its MaterialX catalog
  already includes subsurface, subsurface radius/scale, thin-walled and transmission
  inputs. FoliageUtil now saves `translucency`, `translucency_color`, `subsurface`,
  `subsurface_scale` and `subsurface_radius` in versioned GLB material extras.
  The Blender adapter applies them automatically. See [MATERIALS.md](MATERIALS.md).

Changing dimensions does not introduce a new fold. Card curl/fold/twist currently
shape the prototype once; size variation then changes the proportions of its
copies. Separate seeded deformation ranges per copy remain proposed below.

## Proposed additions for the reference target

These are design proposals, not accepted JSON parameter names.

| Control group | Proposed controls | Purpose / owner |
| --- | --- | --- |
| Outline / stencil | Closed contour or left/right profiles, neck width, widest point, tip roundness/notch, asymmetric lobes, edge irregularity | A shared surface description for FoliageUtil mesh boundaries and TexUtil alpha masks. Existing width-only profiles cannot express a broad irregular rim well. |
| Broad shape | Bend and cup profiles along the surface, separate side rolls, edge ruffle amplitude/wavelength/falloff, crease placement | FoliageUtil geometry; preserve the attachment and rest UV coordinates. |
| Veins | Fan or midrib pattern, branch count/order, divergence, curvature, taper, fine network density | Shared seeded paths rasterized in TexUtil. One coherent vein mask should drive relief, pigment and tissue thickness rather than unrelated noise in each map. |
| Tissue / finish | Base-to-rim pigment gradient, vein tint, fine wrinkles, normal strength, roughness variation, tissue thickness mask | TexUtil maps with shared physical scale. Use geometry only for coarse raised ribs when requested. |
| Light transport | Spatial transmission/thickness maps, explicit thickness for solid geometry, additional engine adapters | Scalar translucency, transmission tint and subsurface weight/radius/scale are implemented in saved materials and the Blender adapter. Keep alpha coverage separate. |
| Reuse / variation | Correlated width/height variation and seeded per-copy bend/fold/twist ranges | Extend the available independent size jitter and atlas selection. Preserve attachment and silhouette while breaking repetition. |
| UVs / output | Flat-rest projection, growth coordinates for field alignment, atlas selection; surface grid, contour mesh or baked card output | Generate the stencil and its mesh in the same coordinate system, then deform together. Preserve a cheap option for distant foliage. |

## Gentle transparency

The intended effect is light diffusely passing through thin tissue, with veins
and thicker regions transmitting less. Alpha coverage remains useful for the
stencil silhouette or holes. Uniformly fading alpha would also weaken the body
and introduce blending/sorting concerns without describing tissue scattering.

The proposed GLB mapping for thin-surface translucency is
[`KHR_materials_diffuse_transmission`](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_diffuse_transmission/README.md).
Its specification explicitly models leaves and other thin diffuse transmitters.
It provides a factor, color and corresponding textures. The specification page
is marked Release Candidate as inspected on September 10, 2026; renderer support
must be verified before promising portable appearance. A texture's channel used
for transmission is distinct from the base-color alpha used for coverage.

The current implementation instead stores versioned `extras.foliageutil` optical
settings. The Blender adapter reads them and combines the imported textured PBR
shader with diffuse thin-tissue transmission and optional Burley surface diffusion.
It applies coverage alpha after this mix, so cutout holes remain transparent.
This makes saved recipes reproducible in the supplied Blender preview, while other
viewers need an adapter. The solid-color Filament helper still needs shader integration.

The detailed rose now uses a procedural fan stencil, twelve synchronized sprite
variants and those saved optics. Its veins use warped stripes, fibers and cellular
fields. They approximate a vein network without a coherent branching tree. Spatial
thickness/transmission maps, a branching path generator and a visual editor remain
future work. See [the rose study](ROSE.md) for the working recipe and actual renders.

## In-app workflow and first milestone

The same saved JSON should drive AI graph generation and visible editing controls:

1. Create one surface, choose its stencil/outline and attachment point.
2. Adjust broad shape while viewing its actual textured geometry.
3. Adjust veins, pigment, normal relief and tissue translucency together.
4. Compare front lighting, backlighting, checker UVs and a silhouette view.
5. Scatter seeded variants into a flower, branch or plant, then export a selected
   geometry budget with its maps and material settings.

Keep TexUtil independent. An optional build/orchestration layer can run its JSON
recipes and bind their outputs into FoliageUtil before export, using explicit
executables and portable relative paths. The generator library need not embed
TexUtil or Blender. A visual editor should serialize these same controls rather
than maintaining hidden renderer-only material state.

The [single-petal study](../samples/graphs/petal-study.json) now provides the segmented
card, procedural fan outline, atlas selection and saved scalar tissue controls.
Next add coherent branching veins and mapped thickness, then contour-mesh output
and independent deformation variation for leaves, sepals, petals and grasses at
different budgets. The same graph fields should drive a future visual editor.
