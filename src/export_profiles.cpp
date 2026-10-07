#include "internal.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <random>
#include <set>

namespace foliage {
Json exportFields() {
    auto integer=[](int value, int low, int high, const char* description) { return Json{{"type","integer"},{"default",value},{"min",low},{"max",high},{"description",description}}; };
    return {
        {"type",{{"type","string"},{"required",true},{"enum",{"alembic","stages"}},{"description","Export a native Ogawa geometry cache or independent growth-stage GLBs. Profiles run only with --export NAME."}}},
        {"output",{{"type","string"},{"required",true},{"description","Portable relative package directory. Contains manifest.json plus animation.abc and materials, or one GLB/CardBake package per stage."}}},
        {"source",{{"type","reference"},{"required",true},{"description","Final mesh node for Alembic and regular geometry stages. CardBake stages use their selected bake profile's source, paths and keep."}}},
        {"lod",{{"type","string"},{"default","base"},{"description","base or a saved LOD name, applied before evaluating growth. Stage overrides may select another level."}}},
        {"start",{{"type","number"},{"default",0},{"min",0},{"max",1},{"description","Starting normalized growth progress, inclusive."}}},
        {"end",{{"type","number"},{"default",1},{"min",0},{"max",1},{"description","Ending normalized growth progress, inclusive and greater than start."}}},
        {"frames",integer(192,2,2000,"Alembic only: uniformly sampled frames. Sample times are index/fps; changing topology is not interpolated by this exporter.")},
        {"fps",integer(24,1,120,"Alembic only: samples per second. Last sample time is (frames-1)/fps.")},
        {"steps",integer(16,2,128,"Stages only: independent snapshots over start..end. Separate from the legacy growth.steps batch.")},
        {"geometry",{{"type","string"},{"default","mesh"},{"enum",{"mesh","card_bake"}},{"description","Stages only: regular generated geometry or foliage captured using a saved CardBake profile."}}},
        {"card_bake",{{"type","string"},{"description","Stages only: saved CardBake name, required when geometry is card_bake. Each stage bakes its currently grown source and visible guides."}}},
        {"stage_overrides",{{"type","object"},{"default",Json::object()},{"description","Stages only: zero-based step index to geometry/card_bake/lod overrides. Unspecified values inherit the profile."}}},
        {"max_samples_mb",integer(2048,16,32768,"Cumulative uncompressed output mesh estimate, counting vertex and triangle records across samples. Not a process-memory limit.")},
        {"max_output_mb",integer(2048,16,32768,"Maximum total package file bytes. Checked during export and before publication; failed exports preserve an existing completed package.")}
    };
}
namespace {
void relativePath(const std::string& value) {
    std::filesystem::path path(value);
    require(!value.empty()&&!path.is_absolute()&&!path.filename().empty()&&value.find_first_of("\\\r\n\t") == std::string::npos&&value.find('\0')==std::string::npos,"export output must be a portable relative directory");
    for(const auto& component:path) require(component!="."&&component!="..","export output cannot contain . or ..");
    require(path.generic_string()==value&&value.find(':')==std::string::npos,"export output must use a canonical portable path");
}
void selection(const Graph& graph, const Json& p) {
    std::string lod=p.value("lod","base");
    require(lod=="base"||(graph.document.contains("lods")&&graph.document["lods"].contains(lod)),"export references unknown LOD '"+lod+"'");
    if(p.contains("card_bake")) require(graph.document.contains("card_bakes")&&graph.document["card_bakes"].contains(p["card_bake"].get<std::string>()),"export references unknown CardBake");
    require(p.value("geometry","mesh")!="card_bake"||p.contains("card_bake"),"card_bake geometry requires a CardBake profile name");
}
Graph level(const Graph& graph, const Json& p) { return p["lod"]=="base"?graph:selectLod(graph,p["lod"].get<std::string>()); }
void jsonFile(const std::filesystem::path& path, const Json& data) { std::ofstream file(path); require(bool(file),"cannot write "+path.string()); file<<data.dump(2)<<'\n'; file.close(); require(bool(file),"failed writing "+path.string()); }
std::vector<std::string> filesIn(const std::filesystem::path& directory) {
    std::vector<std::string> files;
    for(const auto& entry:std::filesystem::recursive_directory_iterator(directory)) {
        require(!entry.is_symlink(),"export packages cannot contain symlinks: "+entry.path().string());
        if(entry.is_regular_file()) files.push_back(entry.path().lexically_relative(directory).generic_string());
        else require(entry.is_directory(),"export package contains a non-regular file");
    }
    std::sort(files.begin(),files.end()); return files;
}
size_t packageBytes(const std::filesystem::path& directory) {
    size_t total=0; for(const auto& name:filesIn(directory)) total+=std::filesystem::file_size(directory/name); return total;
}
void previousPackage(const std::filesystem::path& directory) {
    if(!std::filesystem::exists(directory)) return;
    require(std::filesystem::is_directory(directory)&&!std::filesystem::is_symlink(directory),"export package target must be a directory without symlinks");
    if(std::filesystem::is_empty(directory)) return;
    auto manifest=directory/"manifest.json";
    require(std::filesystem::is_regular_file(manifest)&&std::filesystem::file_size(manifest)<=8*1024*1024,"existing nonempty export directory has no valid package manifest");
    std::ifstream file(manifest); Json data; file>>data;
    require(data.value("type","")=="foliage_export"&&data.value("version",0)==1&&data.contains("files")&&data["files"].is_array(),"existing directory is not a FoliageUtil export package");
    std::vector<std::string> expected{"manifest.json"};
    for(const auto& item:data["files"]) { require(item.is_string(),"invalid package file list"); auto name=item.get<std::string>(); relativePath(name); expected.push_back(name); }
    std::sort(expected.begin(),expected.end());
    auto actual=filesIn(directory);
    // Finder metadata is disposable, but remains in filesIn for imported-asset
    // protection before replacement. All other added files require a new destination.
    auto metadata=[](const std::string& path) { return std::filesystem::path(path).filename()==".DS_Store"; };
    actual.erase(std::remove_if(actual.begin(),actual.end(),metadata),actual.end());
    expected.erase(std::remove_if(expected.begin(),expected.end(),metadata),expected.end());
    require(expected==actual,"export package contains added or missing files; choose a new output directory to preserve them");
}
Json stages(const Graph& graph, const Json& p, const std::filesystem::path& directory, const Options& options) {
    Json snapshots=Json::array(); size_t estimate=0,limit=p["max_samples_mb"].get<size_t>()*1024*1024;
    for(size_t index=0;index<p["steps"].get<size_t>();++index) {
        Json settings=p;
        if(p["stage_overrides"].contains(std::to_string(index))) settings.update(p["stage_overrides"][std::to_string(index)]);
        auto variant=level(graph,settings); Options opt=options;
        opt.growth=p["start"].get<double>()+(p["end"].get<double>()-p["start"].get<double>())*index/(p["steps"].get<size_t>()-1);
        std::string folder="G"+std::string(index<10?"00":index<100?"0":"")+std::to_string(index); Json report;
        try {
            if(settings["geometry"]=="card_bake") {
                std::string bake=settings["card_bake"]; variant.document["card_bakes"][bake]["output"]="model.glb";
                report=exportCardBake(variant,bake,directory/folder,opt);
            } else {
                variant.document["outputs"]={{"model.glb",p["source"]}};
                report=write(variant,generate(variant,opt),directory/folder);
            }
            for(auto& output:report["outputs"]) {
                size_t bytes=output["vertices"].get<size_t>()*sizeof(Vertex)+output["triangles"].get<size_t>()*sizeof(Triangle);
                require(bytes<=limit-estimate,"export max_samples_mb exceeded"); estimate+=bytes;
                output["path"]=(std::filesystem::path(folder)/"model.glb").generic_string();
            }
            Json snapshot={{"step",index},{"progress",*opt.growth},{"geometry",settings["geometry"]},{"lod",settings["lod"]},{"outputs",report["outputs"]},{"empty",report["outputs"].empty()}};
            if(settings["geometry"]=="card_bake") { snapshot["card_bake"]=settings["card_bake"]; snapshot["card_bake_manifest"]=(std::filesystem::path(folder)/"model.cardbake/manifest.json").generic_string(); snapshot["cards"]=report["cards"]; }
            snapshots.push_back(snapshot);
            require(packageBytes(directory)<=p["max_output_mb"].get<size_t>()*1024*1024,"export max_output_mb exceeded");
        } catch(const std::exception& error) { throw std::runtime_error("growth stage "+std::to_string(index)+": "+error.what()); }
    }
    return {{"snapshots",snapshots},{"estimated_samples_mb",double(estimate)/(1024*1024)}};
}
}
void validateExportProfiles(Graph& graph) {
    if(!graph.document.contains("exports")) return;
    require(graph.document.contains("growth"),"growth export profiles require saved growth stages");
    auto& profiles=graph.document["exports"];
    require(profiles.is_object()&&!profiles.empty()&&profiles.size()<=16,"exports requires 1..16 named profiles");
    std::set<std::string> names,outputs;
    for(auto it=profiles.begin();it!=profiles.end();++it) {
        std::string name=it.key(),lower=name;
        require(!name.empty()&&name.size()<=32&&name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")==std::string::npos,"invalid export profile name");
        std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c) { return char(std::tolower(c)); });
        require(names.insert(lower).second,"duplicate export profile name");
        auto& p=it.value(); require(p.is_object()&&p.contains("type")&&p["type"].is_string(),"export profile needs type");
        Json fields=exportFields(),overrides=p.value("stage_overrides",Json::object());
        bool isStages=p["type"]=="stages";
        if(isStages) { fields.erase("frames"); fields.erase("fps"); fields.erase("stage_overrides"); p.erase("stage_overrides"); }
        else for(auto key:{"steps","geometry","card_bake","stage_overrides"}) fields.erase(key);
        validateFields(p,fields,"export '"+name+"'");
        require(p["start"]<p["end"],"export start must precede end");
        std::string source=p["source"]; require(graph.kinds.count(source)&&graph.kinds.at(source)==Kind::mesh,"export source must be a mesh node");
        std::string output=p["output"]; relativePath(output);
        std::transform(output.begin(),output.end(),output.begin(),[](unsigned char c) { return char(std::tolower(c)); });
        for(const auto& other:outputs) require(output!=other&&output.rfind(other+"/",0)!=0&&other.rfind(output+"/",0)!=0,"export package directories overlap");
        outputs.insert(output); selection(graph,p);
        if(isStages) {
            require(overrides.is_object()&&overrides.size()<=p["steps"].get<size_t>(),"stage_overrides must be an index map");
            for(auto o=overrides.begin();o!=overrides.end();++o) {
                require(!o.key().empty()&&o.key().size()<=3&&o.key().find_first_not_of("0123456789")==std::string::npos,"stage override index must be decimal");
                size_t index=std::stoul(o.key()); require(std::to_string(index)==o.key()&&index<p["steps"].get<size_t>(),"stage override index outside range or noncanonical");
                Json allowed=Json::object(); for(auto key:{"geometry","card_bake","lod"}) { allowed[key]=exportFields()[key]; allowed[key].erase("default"); }
                validateFields(o.value(),allowed,"stage override"); Json effective=p; effective.update(o.value()); selection(graph,effective);
            }
            p["stage_overrides"]=overrides;
        }
    }
}
Json exportProfile(const Graph& graph, const std::string& name, const std::filesystem::path& directory, const Options& options) {
    require(!options.growth&&graph.level=="base","named exports select their own growth and LOD settings");
    require(graph.document.contains("exports")&&graph.document["exports"].contains(name),"unknown export profile '"+name+"'");
    const auto& p=graph.document["exports"][name];
    if(p["type"]=="alembic") require(alembicAvailable(),"Alembic support is disabled; rebuild with -DFOLIAGE_ALEMBIC=ON");
    auto root=std::filesystem::weakly_canonical(std::filesystem::absolute(directory)),target=root/p["output"].get<std::string>();
    Graph check=graph; check.document["outputs"]=Json::object();
    preflightExports({check},root,{(std::filesystem::path(p["output"].get<std::string>())/"manifest.json").generic_string()});
    previousPackage(target); std::filesystem::create_directories(target.parent_path());
    std::random_device random; std::filesystem::path temporary;
    for(int attempt=0;attempt<16;++attempt) { temporary=target.parent_path()/(".foliage-export-"+std::to_string(random())+"-"+std::to_string(random())); if(std::filesystem::create_directory(temporary)) break; temporary.clear(); }
    require(!temporary.empty(),"cannot reserve export staging directory");
    try {
        Json report=p["type"]=="alembic"?exportAlembicSamples(level(graph,p),p,temporary,options):stages(graph,p,temporary,options);
        report.update({{"type","foliage_export"},{"version",1},{"profile",name},{"format",p["type"]},{"settings",p},{"seed",options.seed.value_or(graph.document["seed"].get<int32_t>())},{"units","meters"},{"coordinates","right-handed Y-up"}});
        report["files"]=filesIn(temporary); jsonFile(temporary/"manifest.json",report);
        require(packageBytes(temporary)<=p["max_output_mb"].get<size_t>()*1024*1024,"export max_output_mb exceeded");
        std::vector<std::string> destinations;
        for(const auto& file:filesIn(temporary)) destinations.push_back((std::filesystem::path(p["output"].get<std::string>())/file).generic_string());
        if(std::filesystem::exists(target)) for(const auto& file:filesIn(target)) {
            auto path=(std::filesystem::path(p["output"].get<std::string>())/file).generic_string();
            if(std::find(destinations.begin(),destinations.end(),path)==destinations.end()) destinations.push_back(path);
        }
        preflightExports({check},root,destinations); previousPackage(target);
        auto backup=temporary; backup+="-previous"; bool existing=std::filesystem::exists(target);
        require(!std::filesystem::exists(backup),"export backup path already exists");
        if(existing) std::filesystem::rename(target,backup);
        try { std::filesystem::rename(temporary,target); } catch(...) { if(existing) std::filesystem::rename(backup,target); throw; }
        if(existing) std::filesystem::remove_all(backup);
        report["manifest"]=(target/"manifest.json").string(); report["directory"]=target.string(); return report;
    } catch(...) { std::filesystem::remove_all(temporary); throw; }
}
}
