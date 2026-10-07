# TexUtil integration

FoliageUtil generates geometry and TexUtil provides the existing material pipeline.
Use a compatible TexUtil executable without copying its renderer or model dependencies.
The integration scripts use explicit executable paths and argument lists, with no
shell interpolation or modification of TexUtil sources.

## Preview a generated mesh

Prepare the source samples first. The examples use a neighboring executable path;
`--texutil` can point to another compatible build.

```sh
python3 tools/prepare_samples.py --texutil ../TexUtil/build/texutil
python3 tools/preview.py out/samples/graphs/rose.json --texutil ../TexUtil/build/texutil --out out/rose-preview
python3 tools/gallery.py --texutil ../TexUtil/build/texutil
```

The helper exports the recipe, creates one MaterialX material per used slot, writes
`texutil-preview.json` beside the mesh and invokes the existing Filament renderer.
Change `--elevation`, `--azimuth` and `--size` on `preview.py` to inspect other views.
The helper needs a recipe that exports at least one GLB. These previews use solid
material colors and double-sided rendering. TexUtil's current preview shader does
not display alpha cutouts, per-vertex variation, or wind, and this helper does not
connect imported textures. Cards therefore appear as rectangles in the gallery;
the GLB exports still contain their alpha-cutout texture and MASK material.

## Inspect UVs and prepare an atlas

```sh
./build/foliageutil out/samples/graphs/log.json --out out/log
../TexUtil/build/texutil model check-uvs out/log/log.glb --size 256 --json
../TexUtil/build/texutil model uv out/log/log.glb --out out/log/log-atlas.obj --size 1024 --json
../TexUtil/build/texutil model bake out/log/log-atlas.obj --out out/log/maps --size 512 --samples 32 --maps curvature,ao,materialids,coverage
```

`check-uvs` exits 1 when overlapping or tiled UVs prevent whole-model baking, even
when `usable_for_preview` is true. Repeated foliage UVs are intentional. xatlas
produces a new OBJ with unique atlas UVs; use that new mesh for subsequent bakes.
That conversion does not retain FoliageUtil's GLB colors or custom wind attribute.
Baking thickness is inappropriate for open leaves and overlapping branch junctions.

## Apply TexUtil materials

1. Generate bark, cut-end, leaf or petal maps with TexUtil recipes. Keep tiled bark
   separate from leaf atlas textures and use alpha PNGs for card silhouettes.
2. Reference maps in FoliageUtil's document `materials`. Paths are relative to the
   foliage recipe, not the working directory. GLB embeds PNG/JPEG bytes.
3. Use linear RGBA material multipliers, sRGB base-color textures and raw linear
   data textures. Generate OpenGL normals or convert DirectX green before binding
   `normal_texture`.
4. Pack glTF roughness into G and metallic into B before referencing
   `metallic_roughness_texture`. There is no automatic channel packing in FoliageUtil.
5. Generate the GLB again to embed the new maps. OBJ+MTL supports a smaller subset
   of material properties; use GLB for alpha modes, color variation and wind data.

The original [leaf texture recipe](../samples/materials/leaf-texture.texutil.json) creates the
white alpha silhouette used by card samples. Tint comes from the leaf
material. Regenerate it with:

```sh
python3 tools/prepare_samples.py --texutil ../TexUtil/build/texutil
```

For per-material UV or triplanar baking, use TexUtil's ExportBake workflow with the
exported mesh and your material recipes. FoliageUtil's optional
[CardBake export](CARD_BAKE.md) projects existing textured foliage onto fitted
strand cards; it does not modify the source TexUtil material graphs.

## Blender material inspection

For seeded variants, TexUtil now provides native `spritesheet` output and a shared
atlas manifest. FoliageUtil `instance.atlas` selects occupied cells without
duplicating grid/padding settings. See [SPRITESHEETS.md](SPRITESHEETS.md) for a
complete example with aligned color, normal and metallic/roughness maps.

`tools/render_blender.py` imports the exported GLB and renders its actual embedded
materials through Cycles in a separate background process. This is useful for
normal maps, vertex colors and cutouts that the solid-color Filament helper does
not show. It saves a packed scene plus a settings manifest. See [the rose study](ROSE.md)
for the material recipes, inward-fold correction, geometry-density controls and
verified Blender commands.

The Blender helper optionally adds petal subsurface scattering with
`--petal-subsurface` and `--petal-subsurface-scale`. It records this preview-only
setting in the manifest and packed scene; the source GLB remains unchanged.
FoliageUtil also saves `translucency` and `subsurface` in material metadata, which
the Blender adapter reads. The solid-color Filament helper does not display those
optics. See [SURFACES.md](SURFACES.md) for the implemented controls and remaining
stencil/vein work.
