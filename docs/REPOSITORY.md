# Repository layout and storage

Keep source recipes in Git and regenerate the bulky results locally.

| Directory | Content | Git policy |
| --- | --- | --- |
| `src/`, `include/`, `tests/`, `tools/` | Implementation, tests, source-only fixture generators and scripts | Track |
| `samples/graphs/` | Authored FoliageUtil JSON graphs | Track |
| `samples/materials/` | TexUtil JSON recipes, imports and source guidance | Track |
| `samples/manifest.json` | Reproducible material-generation jobs | Track |
| `docs/` | Guides and schema references | Track |
| `docs/images/` | Two README graphics and one social JPEG | Track only the three named exceptions |
| `out/samples/` | Portable prepared graphs, maps, atlases and MaterialX files | Ignore |
| Other `out/` directories | Meshes, CardBakes, texture studies, renders and packed Blender scenes | Ignore |
| `build*/` | Compilers, dependency caches, diagnostic test fixtures and test outputs | Ignore |
| Archives and raw media/models elsewhere | ZIPs, reference photos, generated textures, GLBs, OBJs and Blender files | Ignore |

The `.gitignore` excludes media/model extensions even when they accidentally land
outside `out/`. The only image exceptions are `foliageutil-workflow.png`,
`foliageutil-samples.png` and `foliageutil-social-preview.jpg` in `docs/images/`.
Individual sample renders, comparison sheets and close-ups belong under `out/`.
The sample guides retain their source recipes and reproduction commands; do not
replace removed PNGs with embedded-image SVGs or other bulky binary substitutes.
`.gitattributes` keeps source text consistent and marks publication PNG/JPEG files
as binary. Do not force-add generated assets to make a broken sample work. Fix its
source dependencies or preparation manifest instead.

Use [sample preparation](../samples/README.md) before running textured examples.
Keep an atlas manifest and its entire image directory together inside the generated
bundle. Source manifests cannot substitute for missing generated PNGs.

## Disk usage versus Git size

Ignoring a directory prevents new files from being committed; it does not delete
local files. Build trees, prepared samples, render outputs and private backups can
occupy substantially more disk space than the tracked source tree.

The public repository starts with one source snapshot. Pre-publication history
and recovery archives are kept separately and are not part of public clones.
Do not merge an older checkout's history into the public branch. Preserve local
work separately and use a fresh clone when moving from an older checkout.

The former `examples/` and `assets/` layout has been replaced by the source-only
`samples/` layout. Regenerate portable textures under `out/samples/assets/`.

## Keep test runs small

CTest uses source-generated diagnostic PNGs and atlas layouts, without TexUtil.
Its command wrapper removes a test's generated exports only after that test passes;
failed tests retain their artifacts for investigation. The small `test-samples/`
fixture bundle remains cached in the build tree. Direct invocations of the test
binaries still leave outputs at the directory explicitly supplied to them.

The diagnostic textures are not proof of sample appearance. Run TexUtil preparation
and inspect actual exported assets when changing foliage geometry or materials.

## Local cleanup

`python3 tools/clean_generated.py` prints a plan and total size without deleting.
Add `--apply` to remove the listed regenerable files. It handles successful-test
artifact directories, Blender backup files and duplicate OBJ exports with a matching
GLB, while retaining JSON recipes/logs and final GLB/render outputs. The optional
`--intermediates` scope includes known obsolete bake and banner iterations from
this project's development. Review the plan before applying it to another checkout.
It does not remove `.git`, source samples, documentation images or unknown archives.

Git packing can reduce loose-object overhead while retaining history:

```sh
git count-objects -vH
git gc
```

Untracking current generated assets prevents new revisions from accumulating them.
Removing those assets from old commits requires a separate, explicitly agreed
history migration. Routine cleanup must not rewrite commits
or force-push.
