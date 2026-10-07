# Third-party dependencies

FoliageUtil source and its original procedural sample assets use the MIT license
in [LICENSE](../LICENSE).

| Dependency | Version | Use | License |
| --- | --- | --- | --- |
| [JSON for Modern C++](https://github.com/nlohmann/json/tree/v3.12.0) | 3.12.0 | C++ JSON parsing, validation data and export manifests | MIT |
| libpng | Installed development package (1.6.58 in the verified build) | CardBake PNG decoding and writing | libpng license |
| zlib | Platform libpng dependency | PNG compression | zlib license |
| TexUtil source | Repository source, MIT 2026 | Adapted CPU sampling, rasterization and padding patterns | MIT |
| [Manifold](https://github.com/elalish/manifold/tree/v3.5.3) | 3.5.3 | Closed-mesh boolean unions for `solidify` | Apache-2.0 |

CMake downloads pinned archives with SHA-256 checks. Licenses are retained at
[licenses/nlohmann-json.txt](licenses/nlohmann-json.txt) and
[licenses/manifold.txt](licenses/manifold.txt), and installed with the tool.
Manifold is linked statically with sequential execution; cross-section, language
bindings, tests and optional dependency downloads are disabled.
No TexUtil, Filament, xatlas or model SDK is linked into FoliageUtil. CardBake
links libpng and its zlib dependency. Adapted TexUtil code is attributed in
src/card_bake.cpp; its [MIT notice](licenses/texutil.txt) and the
[libpng notice](licenses/libpng.txt) are retained and installed. Distributions must
also retain the notices for the actual platform libpng/zlib binaries they ship.
The optional preview and atlas workflows call an independently installed TexUtil;
see TexUtil's notices when distributing that application and its dependencies.

Optional development validation uses the [Khronos glTF Validator](https://github.com/KhronosGroup/glTF-Validator)
package `gltf-validator@2.0.0-dev.3.10` (Apache-2.0), installed under `build/validation`.
It is not part of FoliageUtil's runtime or installation. GLB serialization follows
the [glTF 2.0 specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html).

The original leaf PNG is generated from the included TexUtil JSON recipe. Gallery
images are Filament renders of these example meshes using TexUtil's bundled outdoor
lighting. Their environment provenance is documented in TexUtil's assets/hdri folder.
