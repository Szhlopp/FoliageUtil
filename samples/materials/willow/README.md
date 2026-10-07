# Willow bark materials

This directory tracks source recipes only. Run `python3 tools/prepare_samples.py
--texutil ../TexUtil/build/texutil` from the project root. The prepared files are
written under `out/samples/assets/` with this package's relative folder structure.
Do not copy generated maps or atlas manifests back into this source directory.

Dedicated procedural TexUtil materials for the
[weeping willow](../../graphs/weeping-willow.json): dark gray-brown mature bark
with narrow irregular furrows, rough fibers and secondary splits, plus smoother
brown young branch bark. The palette and surface direction follow the descriptions
of mature bark and young stems in [NC State Extension's willow reference](https://plants.ces.ncsu.edu/plants/salix-babylonica/).
This is an authored procedural material, not a scan.

```sh
../TexUtil/build/texutil samples/materials/willow/bark.texutil.json --out out/willow-material/maps --threads 8 --json
```

The recipe imports noise and grain fields from `../rooted-tree/bark.texutil.json`.
Keep those relative files together to regenerate it. The shared source is not
modified. Foliage generation uses only the prepared PNGs and needs no TexUtil build.

All maps are 2048 square. `bark-color.png` and `young-color.png` are sRGB pigment;
normal maps use 16-bit linear OpenGL encoding. Packed metallic/roughness maps use
G=roughness and B=zero metallic. Separate 16-bit roughness maps connect the included
`bark.mtlx` and `young.mtlx` materials. `bark-height.png` is a 16-bit height master.
The GLB uses normal relief without renderer displacement. `bark-sheet.png` shows
height, its 2x2 repeat, normals, color, roughness and young branch pigment.

| Controls in the TexUtil recipe | Effect |
| --- | --- |
| `furrows.scale` / `stretch` | Number and elongation of the primary fissures. |
| `warped-furrows.strength` | Irregular lateral drift in the fissures. |
| `splits` / `split-height` | Finer splits that interrupt the long ridges. |
| `stringy-grain` / `splintered-ridges.opacity` | Rough longitudinal fibers within the ridge surfaces. |
| `pigment-color.stops` | Gray-brown bark palette, independent of the broad cell IDs. |
| `normal.strength` | UV-space relief scale. Increase cautiously after inspecting the actual mesh. |
| `grain-roughness` / `crevice-roughness` | Dry surface finish with restrained independent variation. |
| `young-color.stops` / `young-normal.strength` | Smoother bark for the young side sprays. |

FoliageUtil binds mature bark to the trunk, roots and main boughs. The side sprays
use `willow_young`; the finest hanging strands use a brown scalar material. The
trunk's `uv_scale:[2,1.1]` gives one longitudinal repeat per approximately 0.91 m;
circumference coverage varies with taper. Roots and other branch orders have their
own UV scale. Texture relief does not change their attachment positions.

Rerun sample preparation after recipe edits, then inspect the generated TexUtil
sheet and exported tree under `out/`. The full and close-up Blender renders
are described in [WILLOW.md](../../../docs/WILLOW.md).
