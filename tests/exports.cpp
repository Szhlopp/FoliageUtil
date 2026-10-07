#include "foliage/foliage.hpp"
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#ifdef FOLIAGE_TEST_ALEMBIC
#include <Alembic/AbcGeom/All.h>
#include <Alembic/AbcCoreOgawa/All.h>
#endif
using namespace foliage;
namespace {
int checks=0;
void expect(bool ok, const std::string& message) { ++checks; if(!ok) throw std::runtime_error(message); }
void fails(const std::function<void()>& call, const std::string& text) { try { call(); } catch(const std::exception& e) { expect(std::string(e.what()).find(text)!=std::string::npos,"unexpected error: "+std::string(e.what())); return; } throw std::runtime_error("expected failure: "+text); }
std::string bytes(const std::filesystem::path& file) { std::ifstream input(file,std::ios::binary); std::ostringstream result; result<<input.rdbuf(); return result.str(); }
Json recipe() {
    return {{"seed",42},{"nodes",{
        {"path",{{"op","trunk"},{"length",2},{"radius",.03},{"segments",8},{"noise",0}}},
        {"wood",{{"op","tube"},{"input","path"},{"sides",6}}},
        {"sites",{{"op","scatter"},{"input","path"},{"count",8},{"range",{.15,.9}},{"angle",50}}},
        {"leaf",{{"op","leaf"},{"length",.3},{"width",.15},{"segments",5}}},
        {"leaves",{{"op","instance"},{"input","leaf"},{"points","sites"},{"color_variation",.2}}},
        {"tree",{{"op","merge"},{"inputs",{"wood","leaves"}}}},
        {"wind",{{"op","wind"},{"input","tree"},{"height",2}}}
    }},{"outputs",{{"tree.glb","wind"}}},{"growth",{{"mode","developmental"},{"steps",5},{"stages",{
        {"path",{{"start",0},{"end",1},{"length_profile",{{0,0},{.75,1},{1,1}}}}},
        {"leaves",{{"start",.5},{"end",1}}}
    }}}},{"lods",{{"Low",{{"topology",.5},{"density",.5}}}}},
    {"card_bakes",{{"Spray",{{"source","leaves"},{"paths","path"},{"keep","wood"},{"output","baked.glb"},{"segments",3},{"planes",2},{"cell_width",64},{"cell_height",128},{"atlas_size",128},{"padding",4},{"samples",1}}}}},
    {"exports",{
        {"Animation",{{"type","alembic"},{"output","animation"},{"source","wind"},{"frames",5},{"fps",24}}},
        {"Stages",{{"type","stages"},{"output","stages"},{"source","wind"},{"steps",5}}},
        {"Cards",{{"type","stages"},{"output","cards"},{"source","wind"},{"steps",5},{"geometry","card_bake"},{"card_bake","Spray"}}},
        {"Mixed",{{"type","stages"},{"output","mixed"},{"source","wind"},{"steps",5},{"stage_overrides",{{"3",{{"geometry","card_bake"},{"card_bake","Spray"}}},{"4",{{"lod","Low"}}}}}}}
    }}};
}
#ifdef FOLIAGE_TEST_ALEMBIC
void checkArchive(const Graph& graph, const std::filesystem::path& directory) {
    using namespace Alembic::AbcGeom;
    IArchive archive(Alembic::AbcCoreOgawa::ReadArchive(),(directory/"animation.abc").string()); IObject root(archive.getTop(),"Foliage");
    Json manifest=Json::parse(bytes(directory/"manifest.json"));
    expect(root.getNumChildren()==manifest["bindings"].size(),"one track per used material");
    for(size_t frame=0;frame<5;++frame) {
        Options options; options.growth=frame/4.; const auto generated=generate(graph,options); const auto& mesh=generated.outputs.at("tree.glb")->mesh;
        size_t total=0;
        for(const auto& binding:manifest["bindings"]) {
            std::string path=binding["object"],material=binding["material"]; IPolyMesh object(root,path.substr(path.find_last_of('/')+1)); auto& schema=object.getSchema();
            expect(schema.getNumSamples()==5,"track includes empty leading samples"); expect(std::abs(schema.getTimeSampling()->getSampleTime(frame)-frame/24.)<1e-8,"sample timing");
            ISampleSelector selector{Alembic::Abc::index_t(frame)}; auto sample=schema.getValue(selector);
            auto positions=sample.getPositions(); auto indices=sample.getFaceIndices(); auto counts=sample.getFaceCounts(); auto uv=schema.getUVsParam().getExpandedValue(selector).getVals(); auto normals=schema.getNormalsParam().getExpandedValue(selector).getVals();
            IC4fGeomParam colors(schema.getArbGeomParams(),"Color"); auto color=colors.getExpandedValue(selector).getVals();
            IV2fGeomParam wind(schema.getArbGeomParams(),"_WIND"); auto weights=wind.getExpandedValue(selector).getVals();
            size_t offset=0,faces=0;
            for(const auto& face:mesh.triangles) if(face.material==material) {
                expect((*counts)[faces++]==3,"triangle face counts");
                for(auto corner:{2,1,0}) {
                    auto vertex=face.vertices[corner];
                    auto index=(*indices)[offset++]; const auto& v=mesh.vertices[vertex];
                    expect(((*positions)[index]-V3f(v.position.x,v.position.y,v.position.z)).length()<1e-6,"positions and winding roundtrip");
                    expect(((*normals)[index]-V3f(v.normal.x,v.normal.y,v.normal.z)).length()<1e-6,"normal roundtrip");
                    expect(((*uv)[offset-1]-V2f(v.uv.x,v.uv.y)).length()<1e-6,"UV orientation roundtrip");
                    expect(std::abs((*color)[index][0]-v.color[0])<1e-6&&std::abs((*color)[index][3]-v.color[3])<1e-6,"linear vertex color roundtrip");
                    expect(((*weights)[index]-V2f(v.wind.x,v.wind.y)).length()<1e-6,"wind metadata roundtrip");
                }
            }
            expect(counts->size()==faces&&indices->size()==offset,"no stale faces"); total+=faces;
            expect(GetVisibilityProperty(object).getValue(selector)==(faces?kVisibilityDeferred:kVisibilityHidden),"empty sample visibility");
            expect(schema.getFaceSet(material).getSchema().getValue(selector).getFaces()->size()==faces,"material face set samples");
        }
        expect(total==mesh.triangles.size(),"complete sampled source mesh");
    }
}
#endif
}
int main(int argc, char** argv) {
    try {
        expect(argc==3,"output directory and fixture arguments"); auto root=std::filesystem::absolute(argv[1]); std::filesystem::create_directories(root);
        auto doc=recipe(); auto graph=parse(doc,root); expect(parse(graph.document,root).document==graph.document,"normalized profiles reparse");
        auto mesh=exportProfile(graph,"Stages",root); expect(mesh["snapshots"][0]["empty"]==true,"empty first stage recorded");
        expect(!std::filesystem::exists(root/"stages/G000/model.glb"),"no empty GLB"); expect(mesh["snapshots"][4]["progress"]==1,"mature endpoint");
        auto mature=graph; mature.document["outputs"]={{"model.glb","wind"}}; write(mature,generate(mature),root/"mature"); expect(bytes(root/"mature/model.glb")==bytes(root/"stages/G004/model.glb"),"mature stage matches ordinary geometry exactly");
        auto cards=exportProfile(graph,"Cards",root); expect(cards["snapshots"][0]["cards"]==0&&cards["snapshots"][0]["empty"]==true,"empty baked stage");
        expect(cards["snapshots"][1]["cards"]==0&&!cards["snapshots"][1]["empty"].get<bool>(),"wood-only stage without foliage");
        expect(cards["snapshots"][4]["cards"]==2,"mature source baked into fitted planes");
        auto first=bytes(root/"cards/G004/model.glb"); exportProfile(graph,"Cards",root); expect(first==bytes(root/"cards/G004/model.glb"),"repeatable stage bake");
        auto mixed=exportProfile(graph,"Mixed",root); expect(mixed["snapshots"][3]["geometry"]=="card_bake"&&mixed["snapshots"][4]["lod"]=="Low","per-stage representation and LOD");
        std::ofstream(root/"stages/.DS_Store")<<"Finder metadata"; exportProfile(graph,"Stages",root);
        expect(!std::filesystem::exists(root/"stages/.DS_Store"),"disposable Finder metadata does not block replacement");
        auto saved=bytes(root/"stages/manifest.json"); Options limited; limited.limits.triangles=1;
        fails([&]{exportProfile(graph,"Stages",root,limited);},"triangle budget"); expect(saved==bytes(root/"stages/manifest.json"),"failed overwrite preserves completed package");
        std::ofstream(root/"stages/keep.txt")<<"user file"; fails([&]{exportProfile(graph,"Stages",root);},"added or missing"); std::filesystem::remove(root/"stages/keep.txt");
        auto fewer=doc; fewer["exports"]["Stages"]["steps"]=3; exportProfile(parse(fewer,root),"Stages",root); expect(!std::filesystem::exists(root/"stages/G004"),"overwrite removes obsolete owned stages");
        for(auto invalid:std::vector<Json>{{{"unknown",1}},{{"steps",1}},{{"source","path"}},{{"output","../escape"}},{{"output","cards//nested"}},{{"lod","missing"}},{{"geometry","card_bake"}},{{"stage_overrides",{{"5",{{"geometry","mesh"}}}}}},{{"stage_overrides",{{"01",{{"geometry","mesh"}}}}}},{{"frames",2}}}) {
            auto bad=doc; bad["exports"]["Stages"].update(invalid); fails([&]{parse(bad,root);},"");
        }
        auto overlap=doc; overlap["exports"]["Stages"]["output"]="cards/nested"; fails([&]{parse(overlap,root);},"overlap");
        auto unused=doc; unused["exports"]["Animation"]["steps"]=3; fails([&]{parse(unused,root);},"unknown");
        Options progress; progress.growth=.5; fails([&]{exportProfile(graph,"Stages",root,progress);},"own growth");
        std::filesystem::create_directories(root/"new"); if(!std::filesystem::exists(root/"new/escape")) std::filesystem::create_directory_symlink(root,root/"new/escape"); auto escaped=doc; escaped["exports"]["Stages"]["output"]="escape/stages"; fails([&]{exportProfile(parse(escaped,root),"Stages",root/"new");},"outside output directory");
#ifdef FOLIAGE_TEST_ALEMBIC
        expect(alembicAvailable(),"native feature enabled"); auto animation=exportProfile(graph,"Animation",root); expect(animation["frames"]==5,"Alembic profile runs"); checkArchive(graph,root/"animation");
#else
        expect(!alembicAvailable(),"native feature disabled"); fails([&]{exportProfile(graph,"Animation",root);},"disabled");
#endif
        auto textureGraph=doc;
        auto fixture=std::filesystem::absolute(argv[2])/"assets/bamboo/leaves.atlas.assets/color.png";
        textureGraph["materials"]["leaf"]={{"base_color_texture",std::filesystem::relative(fixture,root).generic_string()},{"normal_texture",std::filesystem::relative(fixture,root).generic_string()}};
#ifdef FOLIAGE_TEST_ALEMBIC
        auto textured=parse(textureGraph,root); exportProfile(textured,"Animation",root);
        auto material=Json::parse(bytes(root/"animation/materials.json"));
        expect(material["materials"]["leaf"]["base_color_texture"]==material["materials"]["leaf"]["normal_texture"],"shared texture copies are deduplicated");
        expect(bytes(root/"animation/textures/0.png")==bytes(fixture),"texture bytes copied intact");
        auto prior=bytes(root/"animation/manifest.json"); auto protectedDoc=textureGraph;
        protectedDoc["materials"]["leaf"]={{"base_color_texture","animation/textures/0.png"}};
        fails([&]{exportProfile(parse(protectedDoc,root),"Animation",root);},"overwrite input asset");
        expect(prior==bytes(root/"animation/manifest.json"),"import protection preserves published package");
        auto expensive=doc; expensive["exports"]["Animation"].update({{"frames",2000},{"start",.99},{"max_samples_mb",16}});
        fails([&]{exportProfile(parse(expensive,root),"Animation",root);},"max_samples_mb");
        expect(prior==bytes(root/"animation/manifest.json"),"sample budget failure preserves published package");
        auto tooLarge=textureGraph; tooLarge["exports"]["Animation"]["max_output_mb"]=16;
        auto large=root/"large.png"; std::ofstream big(large,std::ios::binary); auto prefix=bytes(fixture); big.write(prefix.data(),std::streamsize(prefix.size())); big.close(); std::filesystem::resize_file(large,17*1024*1024);
        tooLarge["materials"]["leaf"]["base_color_texture"]="large.png";
        fails([&]{exportProfile(parse(tooLarge,root),"Animation",root);},"max_output_mb");
        expect(prior==bytes(root/"animation/manifest.json"),"file budget failure preserves published package");
#endif
        std::cout<<checks<<" export checks passed\n"; return 0;
    } catch(const std::exception& error) { std::cerr<<"Export test failed: "<<error.what()<<'\n'; return 1; }
}
