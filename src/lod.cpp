#include "internal.hpp"
#include <cctype>
#include <fstream>
#include <set>

namespace foliage {
Json lodFields() {
    return {
        {"density",{{"type","number"},{"default",1},{"min",0},{"max",1},{"description","Multiply instance/ribbon density; survivors retain their original placement and variation when upstream points are unchanged."}}},
        {"topology",{{"type","number"},{"default",1},{"min",0.01},{"max",1},{"description","Multiply tube sides, leaf/card subdivisions and ellipsoid rings/sides, rounding down to valid minima. Leaves/cards keep their deformation controls; crossed plane count is unchanged."}}},
        {"tube_stride",{{"type","integer"},{"default",1},{"min",1},{"max",512},{"description","Multiply tube/ribbon stride, clamped at 512; retain the original growth paths and both tube endpoints."}}},
        {"overrides",{{"type","object"},{"default",Json::object()},{"description","Node-name to parameter-patch map, applied after global reductions. Existing op cannot change; all resulting nodes validate."}}},
        {"outputs",{{"type","array"},{"description","Optional nonempty list of base output filenames to export for this level. Defaults to all outputs."}}},
        {"screen_height",{{"type","number"},{"min",0},{"max",1},{"description","Optional normalized screen-height threshold recorded in the manifest for engine integration; no runtime switching is implemented."}}}
    };
}
Graph selectLod(const Graph& graph, const std::string& name) {
    require(graph.document.contains("lods")&&graph.document["lods"].contains(name),"unknown LOD '"+name+"'");
    const auto& profile=graph.document["lods"][name];
    Json doc=graph.document; doc.erase("lods"); doc.erase("exports");
    for(auto& node:doc["nodes"]) {
        std::string op=node["op"]; double factor=profile.value("topology",1.0);
        auto reduce=[&](const char* key, int minimum) { node[key]=std::max(minimum,int(std::floor(node[key].get<int>()*factor))); };
        if(op=="instance"||op=="ribbon") node["density"]=node["density"].get<double>()*profile.value("density",1.0);
        if(op=="tube") { reduce("sides",3); node["stride"]=std::min(512,node["stride"].get<int>()*profile.value("tube_stride",1)); }
        if(op=="ribbon") node["stride"]=std::min(512,node["stride"].get<int>()*profile.value("tube_stride",1));
        if(op=="leaf") { reduce("segments",2); reduce("width_segments",2); }
        if(op=="card") { reduce("segments",node["curl"]!=0||node["twist"]!=0?2:1); reduce("width_segments",node["fold"]!=0?2:1); }
        if(op=="ellipsoid") { reduce("rings",3); reduce("sides",3); }
    }
    if(profile.contains("overrides")) for(auto it=profile["overrides"].begin();it!=profile["overrides"].end();++it) {
        require(doc["nodes"].contains(it.key()),"LOD '"+name+"': unknown override node '"+it.key()+"'");
        require(it.value().is_object()&&!it.value().contains("op"),"LOD '"+name+"': overrides must be parameter objects without op");
        doc["nodes"][it.key()].update(it.value());
    }
    Json outputs=Json::object();
    for(auto it=graph.document["outputs"].begin();it!=graph.document["outputs"].end();++it) {
        if(profile.contains("outputs")&&std::find(profile["outputs"].begin(),profile["outputs"].end(),it.key())==profile["outputs"].end()) continue;
        auto path=std::filesystem::path(it.key()); path=path.parent_path()/(path.stem().string()+"-"+name+path.extension().string()); outputs[path.generic_string()]=it.value();
    }
    doc["outputs"]=outputs;
    try { auto result=parse(doc,graph.base); result.level=name; return result; } catch(const std::exception& e) { throw std::runtime_error("LOD '"+name+"': "+e.what()); }
}
void validateLods(Graph& graph) {
    auto& doc=graph.document; if(!doc.contains("lods")) return;
    require(doc["lods"].is_object()&&!doc["lods"].empty()&&doc["lods"].size()<=8,"lods must contain 1..8 named profiles");
    auto fields=lodFields(); std::set<std::string> names;
    for(auto it=doc["lods"].begin();it!=doc["lods"].end();++it) {
        std::string name=it.key(),lower=name;
        require(!name.empty()&&name.size()<=32&&name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")==std::string::npos,"invalid LOD name: "+name);
        for(char& c:lower) c=char(std::tolower(static_cast<unsigned char>(c)));
        require(lower!="base"&&names.insert(lower).second,"reserved or duplicate LOD name: "+name);
        auto& p=it.value(); require(p.is_object(),"LOD '"+name+"' must be an object");
        for(auto f=p.begin();f!=p.end();++f) require(fields.contains(f.key()),"LOD '"+name+"': unknown parameter '"+f.key()+"'");
        for(auto f=fields.begin();f!=fields.end();++f) {
            if(!p.contains(f.key())&&f.value().contains("default")) p[f.key()]=f.value()["default"];
            if(!p.contains(f.key())) continue; const auto& value=p[f.key()];
            if(f.value()["type"]=="number"||f.value()["type"]=="integer") {
                require(value.is_number()&&std::isfinite(value.get<double>())&&value>=f.value()["min"]&&value<=f.value()["max"],"LOD '"+name+"'."+f.key()+": outside allowed bounds");
                if(f.value()["type"]=="integer") require(value.is_number_integer(),"LOD '"+name+"'."+f.key()+": expected integer");
            }
        }
        require(p["overrides"].is_object()&&p["overrides"].size()<=512,"LOD '"+name+"': overrides must be an object with at most 512 nodes");
        if(p.contains("outputs")) {
            require(p["outputs"].is_array()&&!p["outputs"].empty()&&p["outputs"].size()<=64,"LOD '"+name+"': outputs must be a nonempty filename list");
            std::set<std::string> files;
            for(const auto& output:p["outputs"]) require(output.is_string()&&doc["outputs"].contains(output.get<std::string>())&&files.insert(output.get<std::string>()).second,"LOD '"+name+"': unknown or repeated output filename");
        }
        selectLod(graph,name);
    }
}
Json exportGraph(const Graph& graph, const std::filesystem::path& directory, const Options& options, bool includeLods, bool includeGrowth, std::optional<size_t> growthStep) {
    struct Plan { Graph graph; std::string level; std::optional<size_t> step; };
    std::vector<Plan> plans;
    auto levels=[&](std::optional<size_t> step, bool lods) {
        auto add=[&](Graph variant, const std::string& name) {
            if(step) {
                Json outputs=Json::object(); auto suffix=std::string("-G")+(*step<10?"0":"")+std::to_string(*step);
                for(auto it=variant.document["outputs"].begin();it!=variant.document["outputs"].end();++it) {
                    auto p=std::filesystem::path(it.key()); outputs[(p.parent_path()/(p.stem().string()+suffix+p.extension().string())).generic_string()]=it.value();
                }
                variant.document["outputs"]=outputs;
            }
            plans.push_back({std::move(variant),name,step});
        };
        add(graph,graph.level);
        if(lods&&graph.document.contains("lods")) for(auto it=graph.document["lods"].begin();it!=graph.document["lods"].end();++it) add(selectLod(graph,it.key()),it.key());
    };
    bool growth=includeGrowth&&graph.document.contains("growth")&&!options.growth;
    if(growthStep) require(growth&&*growthStep<graph.document["growth"]["steps"].get<size_t>(),"growth step requires a configured step in range");
    if(!growthStep) levels({},includeLods);
    if(growth) {
        size_t steps=graph.document["growth"]["steps"].get<size_t>();
        for(size_t i=0;i<steps;++i) if(!growthStep||i==*growthStep) levels(i,includeLods&&graph.document["growth"]["lods"].get<bool>());
    }
    require(plans.size()<=128,"export exceeds 128 growth/LOD variants; reduce steps or disable growth.lods");
    bool lodManifest=!growthStep&&includeLods&&graph.document.contains("lods");
    std::vector<std::string> manifests; if(lodManifest) manifests.push_back("lods.json"); if(growth) manifests.push_back("growth.json");
    std::vector<Graph> graphs; for(const auto& plan:plans) graphs.push_back(plan.graph);
    preflightExports(graphs,directory,manifests);
    Json report=Json::object(),mature=Json::array(),snapshots=Json::array();
    auto root=std::filesystem::weakly_canonical(std::filesystem::absolute(directory));
    for(const auto& plan:plans) {
        Options opt=options;
        if(plan.step) opt.growth=double(*plan.step)/(graph.document["growth"]["steps"].get<size_t>()-1);
        auto result=generate(plan.graph,opt); auto item=write(plan.graph,result,directory); item["level"]=plan.level;
        if(plan.step) { item["step"]=*plan.step; item["progress"]=*opt.growth; report["growth"].push_back(item); }
        else if(report.empty()) report.update(item); else report["lods"][plan.level]=item;
        auto entry=item; entry.erase("stats"); entry["seed"]=result.stats["seed"];
        if(graph.document.contains("lods")&&graph.document["lods"].contains(plan.level)&&graph.document["lods"][plan.level].contains("screen_height")) entry["screen_height"]=graph.document["lods"][plan.level]["screen_height"];
        for(auto& output:entry["outputs"]) output["path"]=std::filesystem::path(output["path"].get<std::string>()).lexically_relative(root).generic_string();
        if(plan.step) snapshots.push_back(entry); else mature.push_back(entry);
    }
    auto manifest=[&](const std::string& name, const Json& data) {
        std::filesystem::create_directories(root); auto path=root/name; std::ofstream file(path); require(bool(file),"cannot write manifest: "+name);
        file<<data.dump(2)<<'\n'; file.close(); require(bool(file),"failed writing manifest: "+name); return path.string();
    };
    if(lodManifest) report["lod_manifest"]=manifest("lods.json",{{"version",1},{"levels",mature},{"note","Independent meshes sharing source coordinates; assign to your engine's LOD system."}});
    if(growth) report["growth_manifest"]=manifest("growth.json",{{"version",1},{"steps",graph.document["growth"]["steps"]},{"snapshots",snapshots},{"note","Independent growth snapshots, not morph targets. Empty outputs have no mesh file."}});
    return report;
}
}
