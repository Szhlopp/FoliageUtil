#ifndef FOLIAGE_C_API_H
#define FOLIAGE_C_API_H
#include <stdint.h>
#ifdef _WIN32
#define FU_CALL __cdecl
#ifdef FU_BUILD_DLL
#define FU_API __declspec(dllexport)
#else
#define FU_API __declspec(dllimport)
#endif
#else
#define FU_CALL
#define FU_API __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
#define FU_NOEXCEPT noexcept
extern "C" {
#else
#define FU_NOEXCEPT
#endif

typedef struct fu_recipe fu_recipe;
typedef struct fu_mesh fu_mesh;
typedef struct fu_card_growth fu_card_growth;
typedef struct fu_submesh { uint32_t index_start, index_count, material_index; } fu_submesh;
// Vertices: position XYZ, normal XYZ, linear color RGBA, UV XY, wind XY.
// Indices are grouped by stable material slot; coordinates are native RH Y-up.
typedef struct fu_mesh_view { const float* vertices; const uint32_t* indices; const fu_submesh* submeshes; uint32_t vertex_count, index_count, submesh_count, vertex_stride_floats; } fu_mesh_view;

FU_API uint32_t FU_CALL fu_api_version(void) FU_NOEXCEPT;
// Error text is UTF-8, thread-local, and valid until the next fallible API call.
FU_API const char* FU_CALL fu_last_error(void) FU_NOEXCEPT;
// Strings are UTF-8. Creation copies the JSON and asset-base path.
FU_API fu_recipe* FU_CALL fu_recipe_create(const char* json, const char* asset_base) FU_NOEXCEPT;
FU_API void FU_CALL fu_recipe_destroy(fu_recipe* recipe) FU_NOEXCEPT;
// Borrowed UTF-8 JSON: materials array in stable slot order, outputs and LODs.
FU_API const char* FU_CALL fu_recipe_info(const fu_recipe* recipe) FU_NOEXCEPT;
// source=NULL selects the first saved output. lod=NULL selects base.
// progress=-1 means mature; otherwise 0..1. Zero budgets use utility defaults.
// A recipe may generate concurrently, but must remain alive throughout each call.
FU_API fu_mesh* FU_CALL fu_generate(const fu_recipe* recipe, const char* source, const char* lod, int32_t seed, int32_t override_seed, double progress, uint32_t max_vertices, uint32_t max_triangles, uint32_t max_points) FU_NOEXCEPT;
// Fit/bake a saved profile at progress. Writes its GLB/PNG package into an empty
// caller-owned directory. Expensive: run on a worker, reuse the completed result.
FU_API fu_mesh* FU_CALL fu_bake_cards(const fu_recipe* recipe, const char* profile, const char* directory, const char* lod, int32_t seed, int32_t override_seed, double progress, uint32_t max_vertices, uint32_t max_triangles, uint32_t max_points) FU_NOEXCEPT;
// Bake mature cards once into an empty caller-owned directory. Requires saved
// developmental growth. The immutable handle owns its graph independently of recipe.
FU_API fu_card_growth* FU_CALL fu_card_growth_create(const fu_recipe* recipe, const char* profile, const char* directory, const char* lod, int32_t seed, int32_t override_seed, uint32_t max_vertices, uint32_t max_triangles, uint32_t max_points) FU_NOEXCEPT;
FU_API void FU_CALL fu_card_growth_destroy(fu_card_growth* growth) FU_NOEXCEPT;
// No texture baking or file writes. Keep the handle alive throughout concurrent
// calls and the texture directory alive while consumers need its material maps.
FU_API fu_mesh* FU_CALL fu_card_growth_generate(const fu_card_growth* growth, double progress, uint32_t max_vertices, uint32_t max_triangles, uint32_t max_points) FU_NOEXCEPT;
FU_API void FU_CALL fu_mesh_destroy(fu_mesh* mesh) FU_NOEXCEPT;
// The borrowed arrays and stats remain valid until fu_mesh_destroy.
FU_API int32_t FU_CALL fu_mesh_get_view(const fu_mesh* mesh, fu_mesh_view* view) FU_NOEXCEPT;
FU_API const char* FU_CALL fu_mesh_stats(const fu_mesh* mesh) FU_NOEXCEPT;
// Borrowed UTF-8 JSON: material array in this mesh's slot order and asset_base.
FU_API const char* FU_CALL fu_mesh_info(const fu_mesh* mesh) FU_NOEXCEPT;
#ifdef __cplusplus
}
#endif
#endif
