#include <foliage/foliage.hpp>
#include <cmath>
#include <iostream>
#include <stdexcept>

// A small public-API adapter for Blender's continuously sampled growth preview.
// Writes one current frame, so animation rendering need not retain duplicate GLBs.
int main(int argc, char** argv) {
    try {
        if(argc==4&&std::string(argv[2])=="--check-sequence") {
            auto graph=foliage::load(argv[1]);
            if(!graph.document.contains("growth")) throw std::runtime_error("recipe needs native growth stages");
            size_t used=0; std::string text=argv[3]; int frames=std::stoi(text,&used);
            if(used!=text.size()||frames<2||frames>2000) throw std::runtime_error("frame count must be 2..2000");
            foliage::Json snapshots=foliage::Json::array();
            for(int index=0;index<frames;++index) {
                foliage::Options options; options.growth=double(index)/(frames-1);
                auto result=foliage::generate(graph,options);
                foliage::Json snapshot={{"index",index},{"progress",*options.growth},{"outputs",foliage::Json::object()},{"development",foliage::Json::object()},{"solids",foliage::Json::object()}};
                for(const auto& [name,value]:result.outputs) snapshot["outputs"][name]=foliage::meshStats(value->mesh);
                for(const auto& node:result.stats["nodes"]) {
                    for(const auto* field:{"development","instances"}) if(node.contains(field)) snapshot["development"][node["node"].get<std::string>()]=node[field];
                    if(node.contains("solid")) snapshot["solids"][node["node"].get<std::string>()]=node["solid"];
                }
                snapshots.push_back(std::move(snapshot));
            }
            std::cout<<foliage::Json{{"frames",frames},{"snapshots",snapshots}}.dump()<<'\n';
            return 0;
        }
        if(argc!=5) throw std::runtime_error("usage: foliage_growth_frame RECIPE OUTPUT_DIRECTORY OUTPUT_FILENAME PROGRESS | RECIPE --check-sequence FRAMES");
        auto graph=foliage::load(argv[1]);
        if(!graph.document.contains("growth")) throw std::runtime_error("recipe needs native growth stages");
        std::string output=argv[3], value=argv[4]; size_t used=0;
        double progress=std::stod(value,&used);
        if(used!=value.size()||!std::isfinite(progress)||progress<0||progress>1) throw std::runtime_error("progress must be a finite number in 0..1");
        if(!graph.document["outputs"].contains(output)||std::filesystem::path(output).extension()!=".glb") throw std::runtime_error("select an existing GLB output from the recipe");
        graph.document["outputs"]=foliage::Json{{output,graph.document["outputs"][output]}};
        foliage::Options options; options.growth=progress;
        auto result=foliage::generate(graph,options);
        std::cout<<foliage::write(graph,result,argv[2]).dump()<<'\n';
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
