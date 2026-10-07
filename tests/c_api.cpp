#include "foliage/c_api.h"
#include "foliage/foliage.hpp"
#include <cmath>
#include <fstream>
#include <future>
#include <iostream>
#include <sstream>
#include <stdexcept>

void check(bool value, const char* message) { if(!value) throw std::runtime_error(message); }
int main(int argc, char** argv) {
    try {
        check(argc==3,"fixture and output arguments"); check(fu_api_version()==1,"ABI version");
        static_assert(sizeof(fu_submesh)==12,"submesh layout");
        auto file=std::filesystem::path(argv[1])/"graphs/growth-exports.json";
        auto graph=foliage::load(file); std::ifstream input(file); std::stringstream text; text<<input.rdbuf();
        auto recipe=fu_recipe_create(text.str().c_str(),file.parent_path().u8string().c_str()); check(recipe,fu_last_error());
        auto info=foliage::Json::parse(fu_recipe_info(recipe)); check(info["has_growth"]==true,"recipe metadata");
        for(double progress:{0.,.2,.6,1.}) {
            auto handle=fu_generate(recipe,"source",nullptr,42,1,progress,0,0,0); check(handle,fu_last_error());
            fu_mesh_view view{}; check(fu_mesh_get_view(handle,&view)==1,"view"); check(view.vertex_stride_floats==14,"vertex stride");
            foliage::Options options; options.growth=progress; options.seed=42; const auto generated=foliage::generate(graph,options); const auto& mesh=generated.outputs.begin()->second->mesh;
            check(view.vertex_count==mesh.vertices.size()&&view.index_count==mesh.triangles.size()*3,"native topology equals C++ evaluator");
            for(size_t i=0;i<mesh.vertices.size();++i) { const auto& v=mesh.vertices[i]; auto p=view.vertices+14*i; check(p[0]==v.position.x&&p[3]==v.normal.x&&p[6]==v.color[0]&&p[10]==v.uv.x&&p[12]==v.wind.x,"packed attributes"); }
            uint32_t total=0;
            for(uint32_t slot=0;slot<view.submesh_count;++slot) {
                auto sub=view.submeshes[slot]; check(sub.material_index==slot&&sub.index_start==total,"stable material range");
                std::string name=info["materials"][slot]["name"]; uint32_t at=sub.index_start;
                for(const auto& face:mesh.triangles) if(face.material==name) for(auto id:face.vertices) check(view.indices[at++]==id,"grouped winding and indices");
                check(at==sub.index_start+sub.index_count,"submesh count"); total+=sub.index_count;
            }
            check(total==view.index_count,"all triangles grouped"); fu_mesh_destroy(handle);
        }
        auto worker=[recipe](double value) { auto mesh=fu_generate(recipe,nullptr,"base",7,1,value,0,0,0); check(mesh,fu_last_error()); auto stats=std::string(fu_mesh_stats(mesh)); fu_mesh_destroy(mesh); return stats; };
        auto a=std::async(std::launch::async,worker,.5),b=std::async(std::launch::async,worker,1.);
        check(!a.get().empty()&&!b.get().empty(),"concurrent generation");
        check(!fu_generate(recipe,nullptr,nullptr,0,0,NAN,0,0,0),"reject NaN"); check(std::string(fu_last_error()).find("progress")!=std::string::npos,"native exception text");
        check(!fu_generate(recipe,nullptr,nullptr,0,0,1,1,1,1),"budget failure");
        check(!fu_generate(recipe,"missing",nullptr,0,0,1,0,0,0),"bad source");
        check(!fu_generate(recipe,nullptr,"missing",0,0,1,0,0,0),"bad LOD");
        fu_mesh_view zero{}; check(!fu_mesh_get_view(nullptr,&zero)&&zero.vertex_count==0,"null view failure");
        auto bakeRoot=std::filesystem::path(argv[2]);
        for(double progress:{0.,.2,1.}) {
            auto folder=bakeRoot/std::to_string(progress);
            auto baked=fu_bake_cards(recipe,"Strand",folder.u8string().c_str(),nullptr,42,1,progress,0,0,0); check(baked,fu_last_error());
            fu_mesh_view view{}; check(fu_mesh_get_view(baked,&view),"baked view");
            auto materials=foliage::Json::parse(fu_mesh_info(baked)); auto report=foliage::Json::parse(fu_mesh_stats(baked));
            check(report["progress"]==progress&&materials["materials"].size()==view.submesh_count,"bake metadata follows mesh");
            for(auto& material:materials["materials"]) for(auto key:{"base_color_texture","normal_texture","metallic_roughness_texture"}) if(material.contains(key)) check(std::filesystem::is_regular_file(std::filesystem::u8path(materials["asset_base"].get<std::string>())/std::filesystem::u8path(material[key].get<std::string>())),"bake textures resolve");
            if(progress==0) check(view.index_count==0,"empty bake");
            if(progress==1) check(report["card_bake"]["cards"].get<int>()>0&&view.index_count<1240*3,"baked triangle reduction");
            fu_mesh_destroy(baked);
            check(!fu_bake_cards(recipe,"Strand",folder.u8string().c_str(),nullptr,42,1,progress,0,0,0),"bake refuses existing output");
        }
        auto reusable=fu_card_growth_create(recipe,"Strand",(bakeRoot/"reusable").u8string().c_str(),nullptr,42,1,0,0,0); check(reusable,fu_last_error());
        check(!fu_card_growth_create(recipe,"Strand",(bakeRoot/"reusable").u8string().c_str(),nullptr,42,1,0,0,0),"reuse preparation preserves existing output");
        fu_recipe_destroy(recipe); // A prepared bake owns its graph, not the original handle.
        std::string slots;
        for(double progress:{0.,.2,.6,1.,.4}) {
            auto grown=fu_card_growth_generate(reusable,progress,0,0,0); check(grown,fu_last_error());
            fu_mesh_view view{}; check(fu_mesh_get_view(grown,&view),"reusable card view");
            auto metadata=std::string(fu_mesh_info(grown)); if(slots.empty()) slots=metadata; check(metadata==slots,"stable cached material slots");
            auto report=foliage::Json::parse(fu_mesh_stats(grown)); check(report["card_growth"]["rebaked"]==false,"reusable mode metadata");
            if(progress==0) check(view.vertex_count==0,"empty cached growth"); else check(view.vertex_count>0,"visible cached growth");
            fu_mesh_destroy(grown);
        }
        check(!fu_card_growth_generate(reusable,NAN,0,0,0),"cached NaN failure");
        check(!fu_card_growth_generate(reusable,1,1,0,0),"cached budget failure");
        check(!fu_card_growth_generate(nullptr,1,0,0,0),"null cached handle");
        fu_card_growth_destroy(reusable); fu_card_growth_destroy(nullptr);
        fu_recipe_destroy(nullptr); fu_mesh_destroy(nullptr);
        check(!fu_recipe_create("{",nullptr),"invalid JSON caught across ABI");
        auto procedural=fu_recipe_create(R"({"version":1,"nodes":{"card":{"op":"card"}},"outputs":{"card.glb":"card"}})",nullptr); check(procedural,fu_last_error());
        auto noAssets=fu_generate(procedural,nullptr,nullptr,0,0,1,0,0,0); check(noAssets,fu_last_error());
        check(std::filesystem::is_directory(foliage::Json::parse(fu_mesh_info(noAssets))["asset_base"].get<std::string>()),"omitted asset base uses current directory");
        fu_mesh_destroy(noAssets); fu_recipe_destroy(procedural);
        for(auto name:{"rocks","cliff","rock-formation","gems","crystal-cluster"}) {
            auto path=std::filesystem::path(argv[1])/"graphs"/(std::string(name)+".json"); std::ifstream stream(path); std::stringstream json; json<<stream.rdbuf();
            auto mineral=fu_recipe_create(json.str().c_str(),path.parent_path().u8string().c_str()); check(mineral,fu_last_error());
            auto data=fu_generate(mineral,nullptr,nullptr,42,1,1,0,0,0); check(data,fu_last_error()); fu_mesh_view view{}; check(fu_mesh_get_view(data,&view)&&view.index_count>0,"mineral native mesh");
            auto expected=foliage::generate(foliage::load(path)); const auto& mesh=expected.outputs.begin()->second->mesh;
            check(view.vertex_count==mesh.vertices.size()&&view.index_count==mesh.triangles.size()*3,"mineral native topology matches library");
            for(size_t i=0;i<mesh.vertices.size();++i) check(view.vertices[i*14]==mesh.vertices[i].position.x&&view.vertices[i*14+3]==mesh.vertices[i].normal.x,"mineral native geometry and normals match");
            if(std::string(name)=="gems") { auto metadata=foliage::Json::parse(fu_mesh_info(data)); bool found=false; for(auto& m:metadata["materials"]) if(m["name"]=="diamond") { found=true; check(m["ior"]==2.42&&m["transmission"]==1&&m["thickness"]==1,"gem optics retained in native metadata"); } check(found,"diamond material slot"); }
            fu_mesh_destroy(data); fu_recipe_destroy(mineral);
        }
        std::cout<<"C ABI checks passed\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
