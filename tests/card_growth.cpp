#include "foliage/foliage.hpp"
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <sstream>
using namespace foliage;
namespace {
void check(bool value, const std::string& message) { if(!value) throw std::runtime_error(message); }
void fails(const std::function<void()>& fn, const std::string& message) { try { fn(); } catch(const std::exception& error) { check(std::string(error.what()).find(message)!=std::string::npos,error.what()); return; } throw std::runtime_error("expected failure: "+message); }
std::string bytes(const std::filesystem::path& path) { std::ifstream file(path,std::ios::binary); std::ostringstream output; output<<file.rdbuf(); return output.str(); }
void same(const Mesh& a, const Mesh& b) {
    check(a.vertices.size()==b.vertices.size()&&a.triangles.size()==b.triangles.size(),"matching topology");
    for(size_t i=0;i<a.vertices.size();++i) {
        const auto& x=a.vertices[i]; const auto& y=b.vertices[i];
        check(length(x.position-y.position)<1e-7f&&length(x.normal-y.normal)<1e-7f,"matching positions and normals");
        check(x.uv.x==y.uv.x&&x.uv.y==y.uv.y&&x.wind.x==y.wind.x&&x.wind.y==y.wind.y&&x.color==y.color,"matching UVs, wind and colors");
    }
    for(size_t i=0;i<a.triangles.size();++i) check(a.triangles[i].vertices==b.triangles[i].vertices&&a.triangles[i].material==b.triangles[i].material,"matching winding and materials");
}
Json tree() {
    return {{"seed",73},{"nodes",{
        {"trunk",{{"op","trunk"},{"length",2},{"radius",.06},{"noise",0},{"segments",12}}},
        {"wood",{{"op","tube"},{"input","trunk"},{"sides",6}}},
        {"branches",{{"op","branch"},{"input","trunk"},{"count",3},{"range",{.25,.85}},{"length",1},{"segments",12},{"noise",.08}}},
        {"twigs",{{"op","tube"},{"input","branches"},{"sides",4}}},
        {"sites",{{"op","scatter"},{"input","branches"},{"count",8},{"range",{.04,.95}}}},
        {"leaf",{{"op","leaf"},{"length",.2},{"width",.08}}},
        {"leaves",{{"op","instance"},{"input","leaf"},{"points","sites"}}},
        {"source",{{"op","merge"},{"inputs",{"twigs","leaves"}}}}
    }},{"outputs",{{"tree.glb","source"}}},{"growth",{{"mode","developmental"},{"steps",5},{"stages",{
        {"trunk",{{"length_profile",{{0,0},{.65,1},{1,1}}}}},
        {"branches",{{"start",.2},{"attachment_delay",.03}}},
        {"leaves",{{"start",.3}}}
    }}}},{"card_bakes",{{"Test",{{"source","source"},{"paths","branches"},{"keep","wood"},{"output","tree.glb"},{"segments",4},{"planes",2},{"atlas_size",128},{"cell_width",64},{"cell_height",128},{"padding",2},{"samples",1}}}}}};
}
}
int main(int argc, char** argv) {
    try {
        check(argc==2,"output directory argument"); auto root=std::filesystem::absolute(argv[1]);
        auto graph=parse(tree()); auto prepared=prepareCardGrowth(graph,"Test",root/"prepared");
        auto original=bakeCards(graph,"Test",root/"reference"); auto mature=growCards(*prepared,1);
        same(original.mesh,mature.mesh);
        std::map<std::filesystem::path,std::pair<std::string,std::filesystem::file_time_type>> files;
        for(auto& file:std::filesystem::recursive_directory_iterator(root/"prepared")) if(file.is_regular_file()) files[file.path()]={bytes(file.path()),file.last_write_time()};
        auto early=growCards(*prepared,.1); check(early.report["cards"]==0&&!early.mesh.triangles.empty(),"wood-only early growth");
        check(growCards(*prepared,0).mesh.vertices.empty(),"empty growth");
        size_t partial=0;
        for(int frame=0;frame<=192;++frame) {
            auto result=growCards(*prepared,double(frame)/192);
            checkMesh(result.mesh); check(result.materials==mature.materials&&result.base==mature.base,"stable material slots and texture paths");
            check(result.report["rebaked"]==false,"no rebake");
            auto cards=result.report["cards"].get<size_t>(); if(cards>0&&cards<mature.report["cards"].get<size_t>()) ++partial;
            for(const auto& node:result.report["generation"]["nodes"]) check(node["node"]!="leaves"&&node["node"]!="leaf"&&node["node"]!="sites"&&node["node"]!="source"&&node["node"]!="twigs","source canopy excluded from update");
            for(const auto& face:result.mesh.triangles) {
                check(result.materials.contains(face.material),"every face has a material slot");
                auto a=result.mesh.vertices[face.vertices[0]],b=result.mesh.vertices[face.vertices[1]],c=result.mesh.vertices[face.vertices[2]];
                if(face.material.find("cardbake_")==0) check(length(cross(b.position-a.position,c.position-a.position))>1e-14f,"nondegenerate cards");
            }
        }
        check(partial>0,"children emerge independently after support arrives");
        auto middle=growCards(*prepared,.57); growCards(*prepared,.9); same(middle.mesh,growCards(*prepared,.57).mesh);
        auto job=std::async(std::launch::async,[&] { return growCards(*prepared,.57); }); same(middle.mesh,growCards(*prepared,.57).mesh); same(middle.mesh,job.get().mesh);
        for(const auto& file:files) check(bytes(file.first)==file.second.first&&std::filesystem::last_write_time(file.first)==file.second.second,"sampling leaves package bytes and timestamps untouched");
        check(std::distance(std::filesystem::recursive_directory_iterator(root/"prepared"),std::filesystem::recursive_directory_iterator())==static_cast<std::ptrdiff_t>(files.size()+1),"sampling writes no files");
        fails([&] { growCards(*prepared,NAN); },"progress");
        fails([&] { growCards(*prepared,-.1); },"progress");
        fails([&] { growCards(*prepared,1.1); },"progress");
        Limits tiny; tiny.vertices=1; fails([&] { growCards(*prepared,1,tiny); },"budget");
        tiny={}; tiny.triangles=1; fails([&] { growCards(*prepared,1,tiny); },"budget");
        tiny={}; tiny.points=1; fails([&] { growCards(*prepared,1,tiny); },"budget");
        fails([&] { prepareCardGrowth(graph,"Test",root/"prepared"); },"empty output");
        Options immature; immature.growth=.5; fails([&] { prepareCardGrowth(graph,"Test",root/"immature",immature); },"mature");
        auto scale=tree(); scale["growth"]={{"mode","scale"},{"steps",5},{"stages",{{"trunk",{{"start",0},{"end",1}}}}}};
        fails([&] { prepareCardGrowth(parse(scale),"Test",root/"scale"); },"developmental");
        // Multiple atlas pages, a curved path and distinct crossed cells survive sampling.
        check(mature.report["atlas_pages"].get<size_t>()>1&&mature.report["cards"]==6,"multipage crossed bindings");
        Options seed; seed.seed=91;
        auto seeded=prepareCardGrowth(graph,"Test",root/"seeded",seed); auto seededMature=growCards(*seeded,1);
        same(bakeCards(graph,"Test",root/"seeded-reference",seed).mesh,seededMature.mesh);
        check(seededMature.report["seed"]==91&&length(seededMature.mesh.vertices.back().position-mature.mesh.vertices.back().position)>1e-5f,"seed-specific bindings");
        auto lodDoc=tree(); lodDoc["lods"]={{"Low",{{"density",.5},{"topology",.5},{"tube_stride",2}}}};
        auto lod=selectLod(parse(lodDoc),"Low"); auto low=prepareCardGrowth(lod,"Test",root/"lod");
        same(bakeCards(lod,"Test",root/"lod-reference").mesh,growCards(*low,1).mesh);
        check(growCards(*low,.5).report["lod"]=="Low","LOD-specific bindings");
        std::cout<<"Reusable CardBake checks passed (193 growth samples)\n"; return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
