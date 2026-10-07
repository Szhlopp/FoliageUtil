#include "foliage/c_api.h"
#include "internal.hpp"
#include <cstring>
#include <limits>
#include <memory>

struct fu_recipe { foliage::Graph graph; std::string info; };
struct fu_card_growth { std::shared_ptr<const foliage::CardGrowth> prepared; };
struct fu_mesh { std::vector<float> vertices; std::vector<uint32_t> indices; std::vector<fu_submesh> submeshes; std::string stats,info; };
namespace {
thread_local std::string error;
void failure() noexcept { try { throw; } catch(const std::exception& e) { try { error=e.what(); } catch(...) {} } catch(...) { try { error="Unknown native exception"; } catch(...) {} } }
std::unique_ptr<fu_mesh> packMesh(const foliage::Mesh& mesh, const foliage::Json& materials, const std::filesystem::path& base) {
    foliage::require(mesh.vertices.size()<=std::numeric_limits<uint32_t>::max()&&mesh.triangles.size()<=std::numeric_limits<uint32_t>::max()/3,"Mesh exceeds the C ABI index limit");
    std::vector<std::string> names; foliage::Json array=foliage::Json::array();
    for(auto it=materials.begin();it!=materials.end();++it) { names.push_back(it.key()); auto material=it.value(); material["name"]=it.key(); array.push_back(material); }
        auto result=std::make_unique<fu_mesh>(); result->vertices.reserve(mesh.vertices.size()*14); result->indices.reserve(mesh.triangles.size()*3);
        for(const auto& v:mesh.vertices) {
            auto& values=result->vertices; values.insert(values.end(),{v.position.x,v.position.y,v.position.z,v.normal.x,v.normal.y,v.normal.z});
            values.insert(values.end(),v.color.begin(),v.color.end()); values.insert(values.end(),{v.uv.x,v.uv.y,v.wind.x,v.wind.y});
        }
        for(size_t slot=0;slot<names.size();++slot) {
            fu_submesh sub{uint32_t(result->indices.size()),0,uint32_t(slot)};
            for(const auto& face:mesh.triangles) if(face.material==names[slot]) result->indices.insert(result->indices.end(),face.vertices.begin(),face.vertices.end());
            sub.index_count=uint32_t(result->indices.size())-sub.index_start; result->submeshes.push_back(sub);
        }
    result->info=foliage::Json({{"materials",array},{"asset_base",std::filesystem::absolute(base).u8string()}}).dump(); return result;
}

}
extern "C" {
uint32_t FU_CALL fu_api_version(void) noexcept { return 1; }
const char* FU_CALL fu_last_error(void) noexcept { return error.c_str(); }
fu_recipe* FU_CALL fu_recipe_create(const char* json, const char* asset_base) noexcept {
    error.clear(); try {
        foliage::require(json&&std::strlen(json)<=8*1024*1024,"Recipe JSON is missing or exceeds 8 MiB");
        foliage::require(!asset_base||std::strlen(asset_base)<=32768,"Asset base path is too long");
        auto result=std::make_unique<fu_recipe>(); result->graph=foliage::parse(foliage::Json::parse(json),std::filesystem::u8path(asset_base&&*asset_base?asset_base:"."));
        foliage::Json materials=foliage::Json::array(),lods=foliage::Json::array({"base"});
        for(auto it=result->graph.document["materials"].begin();it!=result->graph.document["materials"].end();++it) { auto value=it.value(); value["name"]=it.key(); materials.push_back(value); }
        if(result->graph.document.contains("lods")) for(auto it=result->graph.document["lods"].begin();it!=result->graph.document["lods"].end();++it) lods.push_back(it.key());
        result->info=foliage::Json({{"api_version",1},{"materials",materials},{"lods",lods},{"outputs",result->graph.document["outputs"]},{"has_growth",result->graph.document.contains("growth")},{"name",result->graph.document.value("name","")}}).dump();
        return result.release();
    } catch(...) { failure(); return nullptr; }
}
void FU_CALL fu_recipe_destroy(fu_recipe* recipe) noexcept { delete recipe; }
const char* FU_CALL fu_recipe_info(const fu_recipe* recipe) noexcept { error.clear(); try { foliage::require(recipe,"Recipe handle is null"); return recipe->info.c_str(); } catch(...) { failure(); return nullptr; } }
fu_mesh* FU_CALL fu_generate(const fu_recipe* recipe, const char* source, const char* lod, int32_t seed, int32_t override_seed, double progress, uint32_t max_vertices, uint32_t max_triangles, uint32_t max_points) noexcept {
    error.clear(); try {
        foliage::require(recipe,"Recipe handle is null");
        foliage::require(progress==-1||(std::isfinite(progress)&&progress>=0&&progress<=1),"Growth progress must be -1 or within 0..1");
        foliage::Options options; if(override_seed) options.seed=seed; if(progress>=0) options.growth=progress;
        if(max_vertices) options.limits.vertices=max_vertices; if(max_triangles) options.limits.triangles=max_triangles; if(max_points) options.limits.points=max_points;
        auto graph=lod&&std::strcmp(lod,"base")!=0&&*lod?foliage::selectLod(recipe->graph,lod):recipe->graph;
        std::string node=source&&*source?source:graph.document["outputs"].begin().value().get<std::string>();
        foliage::require(graph.kinds.count(node)&&graph.kinds.at(node)==foliage::Kind::mesh,"Source must name a mesh node");
        auto generated=foliage::generateNodes(graph,options,{node}); const auto& mesh=generated.outputs.at(node)->mesh;
        auto result=packMesh(mesh,graph.document["materials"],graph.base);
        result->stats=foliage::Json({{"mesh",foliage::meshStats(mesh)},{"source",node},{"lod",graph.level},{"progress",progress},{"generation",generated.stats}}).dump();
        return result.release();
    } catch(...) { failure(); return nullptr; }
}
fu_mesh* FU_CALL fu_bake_cards(const fu_recipe* recipe, const char* profile, const char* directory, const char* lod, int32_t seed, int32_t override_seed, double progress, uint32_t max_vertices, uint32_t max_triangles, uint32_t max_points) noexcept {
    error.clear(); try {
        foliage::require(recipe&&profile&&*profile&&directory&&*directory,"Recipe, CardBake profile and output directory are required");
        foliage::require(progress==-1||(std::isfinite(progress)&&progress>=0&&progress<=1),"Growth progress must be -1 or within 0..1");
        auto output=std::filesystem::u8path(directory);
        foliage::require(!std::filesystem::exists(output)||(std::filesystem::is_directory(output)&&std::filesystem::is_empty(output)),"CardBake needs an empty output directory; existing files are preserved");
        foliage::Options options; if(override_seed) options.seed=seed; if(progress>=0) options.growth=progress;
        if(max_vertices) options.limits.vertices=max_vertices; if(max_triangles) options.limits.triangles=max_triangles; if(max_points) options.limits.points=max_points;
        auto graph=lod&&*lod&&std::strcmp(lod,"base")!=0?foliage::selectLod(recipe->graph,lod):recipe->graph;
        auto baked=foliage::bakeCards(graph,profile,output,options);
        foliage::Json used=foliage::Json::object(); for(const auto& face:baked.mesh.triangles) if(!used.contains(face.material)) used[face.material]=baked.materials.at(face.material);
        auto result=packMesh(baked.mesh,used,baked.base);
        result->stats=foliage::Json({{"mesh",foliage::meshStats(baked.mesh)},{"card_bake",baked.report},{"progress",progress},{"lod",graph.level}}).dump();
        return result.release();
    } catch(...) { failure(); return nullptr; }
}

fu_card_growth* FU_CALL fu_card_growth_create(const fu_recipe* recipe, const char* profile, const char* directory, const char* lod, int32_t seed, int32_t override_seed, uint32_t max_vertices, uint32_t max_triangles, uint32_t max_points) noexcept {
    error.clear(); try {
        foliage::require(recipe&&profile&&*profile&&directory&&*directory,"Recipe, CardBake profile and output directory are required");
        foliage::Options options; if(override_seed) options.seed=seed;
        if(max_vertices) options.limits.vertices=max_vertices; if(max_triangles) options.limits.triangles=max_triangles; if(max_points) options.limits.points=max_points;
        auto graph=lod&&*lod&&std::strcmp(lod,"base")!=0?foliage::selectLod(recipe->graph,lod):recipe->graph;
        auto result=std::make_unique<fu_card_growth>(); result->prepared=foliage::prepareCardGrowth(graph,profile,std::filesystem::u8path(directory),options);
        return result.release();
    } catch(...) { failure(); return nullptr; }
}
void FU_CALL fu_card_growth_destroy(fu_card_growth* growth) noexcept { delete growth; }
fu_mesh* FU_CALL fu_card_growth_generate(const fu_card_growth* growth, double progress, uint32_t max_vertices, uint32_t max_triangles, uint32_t max_points) noexcept {
    error.clear(); try {
        foliage::require(growth,"Card growth handle is null");
        foliage::Limits limits; if(max_vertices) limits.vertices=max_vertices; if(max_triangles) limits.triangles=max_triangles; if(max_points) limits.points=max_points;
        auto grown=foliage::growCards(*growth->prepared,progress,limits);
        auto result=packMesh(grown.mesh,grown.materials,grown.base);
        result->stats=foliage::Json({{"mesh",foliage::meshStats(grown.mesh)},{"card_growth",grown.report},{"progress",progress}}).dump();
        return result.release();
    } catch(...) { failure(); return nullptr; }
}
void FU_CALL fu_mesh_destroy(fu_mesh* mesh) noexcept { delete mesh; }
int32_t FU_CALL fu_mesh_get_view(const fu_mesh* mesh, fu_mesh_view* view) noexcept {
    error.clear(); if(view) *view={}; try {
        foliage::require(mesh&&view,"Mesh or view pointer is null");
        *view={mesh->vertices.data(),mesh->indices.data(),mesh->submeshes.data(),uint32_t(mesh->vertices.size()/14),uint32_t(mesh->indices.size()),uint32_t(mesh->submeshes.size()),14}; return 1;
    } catch(...) { failure(); return 0; }
}
const char* FU_CALL fu_mesh_info(const fu_mesh* mesh) noexcept { error.clear(); try { foliage::require(mesh,"Mesh handle is null"); return mesh->info.c_str(); } catch(...) { failure(); return nullptr; } }
const char* FU_CALL fu_mesh_stats(const fu_mesh* mesh) noexcept { error.clear(); try { foliage::require(mesh,"Mesh handle is null"); return mesh->stats.c_str(); } catch(...) { failure(); return nullptr; } }
}
