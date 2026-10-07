#include "internal.hpp"
#include <chrono>
#include <fstream>
#include <functional>
#include <set>

namespace foliage {
std::string kindName(Kind kind) { return kind==Kind::skeleton?"skeleton":kind==Kind::points?"points":"mesh"; }
std::vector<std::string> references(const Json& node) {
    std::vector<std::string> result;
    const auto fields=catalog().at(node.at("op").get<std::string>()).at("parameters");
    for(auto it=fields.begin();it!=fields.end();++it) if(node.contains(it.key())) {
        if(it.value().at("type")=="reference") result.push_back(node.at(it.key()).get<std::string>());
        if(it.value().at("type")=="references") for(const auto& name:node.at(it.key())) result.push_back(name.get<std::string>());
    }
    return result;
}
Graph load(const std::filesystem::path& filename) {
    require(std::filesystem::file_size(filename)<=8*1024*1024,"recipe exceeds 8 MiB");
    std::ifstream in(filename); require(bool(in),"cannot read recipe: "+filename.string());
    Json doc; in>>doc; return parse(doc,std::filesystem::absolute(filename).parent_path());
}
Graph parse(const Json& document, const std::filesystem::path& base) {
    Graph graph{document,base.empty()?std::filesystem::current_path():std::filesystem::absolute(base),{}, {}};
    auto& doc=graph.document; require(doc.is_object(),"recipe must be an object");
    const std::set<std::string> keys={"version","seed","units","name","description","nodes","outputs","materials","lods","growth","card_bakes","exports"};
    for(auto it=doc.begin();it!=doc.end();++it) require(keys.count(it.key()),"unknown document field '"+it.key()+"'");
    if(doc.contains("version")) require(doc["version"].is_number_integer()&&doc["version"]==1,"version must be 1");
    doc["version"]=1;
    if(!doc.contains("seed")) doc["seed"]=42;
    require(doc["seed"].is_number_integer()&&doc["seed"].get<double>()>=-2147483648.0&&doc["seed"].get<double>()<=2147483647.0,"seed must be a signed 32-bit integer");
    if(doc.contains("units")) require(doc["units"]=="meters","only meters are supported");
    doc["units"]="meters";
    for(auto key:{"name","description"}) if(doc.contains(key)) require(doc[key].is_string(),std::string(key)+" must be a string");
    require(doc.contains("nodes")&&doc["nodes"].is_object()&&!doc["nodes"].empty()&&doc["nodes"].size()<=512,"nodes must contain 1..512 named nodes");
    require(doc.contains("outputs")&&doc["outputs"].is_object()&&!doc["outputs"].empty()&&doc["outputs"].size()<=64,"outputs must contain 1..64 files");
    if(!doc.contains("materials")) doc["materials"]=Json::object();
    require(doc["materials"].is_object()&&doc["materials"].size()<=256,"materials must be an object with at most 256 slots");
    Json defaults={{"bark",{{"base_color",{0.18,0.09,0.035,1}},{"double_sided",false}}},
        {"cut",{{"base_color",{0.58,0.37,0.17,1}},{"double_sided",false}}},
        {"leaf",{{"base_color",{0.12,0.32,0.035,1}}}}};
    for(auto it=defaults.begin();it!=defaults.end();++it) if(!doc["materials"].contains(it.key())) doc["materials"][it.key()]=it.value();
    for(auto it=doc["materials"].begin();it!=doc["materials"].end();++it) {
        require(!it.key().empty()&&it.key().size()<=128&&it.key().find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")==std::string::npos,"invalid material name: "+it.key());
        validateFields(it.value(),materialFields(),"material '"+it.key()+"'");
        require(it.value()["thickness"]==0||it.value()["transmission"].get<float>()>0,"material '"+it.key()+"': thickness requires transmission");
        require(!it.value().contains("attenuation_distance")||it.value()["thickness"].get<float>()>0,"material '"+it.key()+"': attenuation_distance requires thickness");
        for(auto key:{"base_color_texture","normal_texture","metallic_roughness_texture"}) if(it.value().contains(key)) {
            auto path=graph.base/it.value()[key].get<std::string>();
            require(std::filesystem::is_regular_file(path),"missing texture: "+path.string());
            require(std::filesystem::file_size(path)<=64*1024*1024,"texture exceeds 64 MiB: "+path.string());
        }
    }
    auto cat=catalog();
    for(auto it=doc["nodes"].begin();it!=doc["nodes"].end();++it) {
        require(!it.key().empty()&&it.key().size()<=128&&it.key().find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-")==std::string::npos,"invalid node name: "+it.key());
        auto& node=it.value(); std::string label="node '"+it.key()+"'";
        require(node.is_object()&&node.contains("op")&&node["op"].is_string(),label+" needs op");
        std::string op=node["op"]; require(cat.contains(op),label+": unknown op '"+op+"'");
        auto fields=cat[op]["parameters"]; fields["op"]={{"type","string"},{"required",true}};
        validateFields(node,fields,label);
        if(node.contains("direction")) require(length(vector3(node["direction"]))>1e-7f,label+": direction must be nonzero");
        if(node.contains("normal_direction")) require(length(vector3(node["normal_direction"]))>1e-7f,label+": normal_direction must be nonzero");
        if(node["op"]=="scatter"&&!node.contains("normal_direction")) require(node["max_surface_angle"]==90,label+": max_surface_angle requires normal_direction");
        if(node.contains("up")) require(length(vector3(node["up"]))>1e-7f,label+": up must be nonzero");
        for(auto key:{"range","scale"}) if(node.contains(key)&&node[key].is_array()&&op!="radial") require(node[key][0]<=node[key][1],label+"."+key+" must be ordered low to high");
        for(auto key:{"material","cap_material","name"}) if(node.contains(key)&&fields.contains(key)&&fields[key]["type"]=="material") require(doc["materials"].contains(node[key].get<std::string>()),label+": undefined material '"+node[key].get<std::string>()+"'");
        if(op=="leaf"&&node.contains("width_profile")) {
            const auto& profile=node["width_profile"];
            require(profile.size()>=3&&profile.front()[1]==0&&profile.back()[1]==0,label+": width_profile needs zero end widths and positive interior keys");
            for(size_t i=1;i+1<profile.size();++i) require(profile[i][1].get<float>()>=.0001f,label+": width_profile interior widths must be positive");
        }
        if(op=="ellipsoid") for(const auto& r:node["radii"]) require(r.get<float>()>0,label+": radii must be positive");
        if(op=="crystal") {
            const auto& profile=node["radius_profile"]; bool positive=false;
            for(size_t i=0;i<profile.size();++i) {
                float radius=profile[i][1].get<float>(); positive|=radius>0;
                require(radius==0||radius>=.0001f,label+": radius_profile nonzero radii must be at least 0.0001");
                if(i>0&&i+1<profile.size()) require(radius>0,label+": radius_profile interior radii must be positive");
            }
            require(positive,label+": radius_profile needs a positive radius");
        }
        if(op=="card") {
            if(node["curl"]!=0||node["twist"]!=0) require(node["segments"]>=2,label+": curl or twist requires segments >= 2");
            if(node["fold"]!=0) require(node["width_segments"]>=2,label+": fold requires width_segments >= 2");
        }
        if(op=="scatter"&&!node.contains("proximity_mesh")) require(!node["snap_to_mesh"].get<bool>()&&node["max_distance"].get<float>()==cat[op]["parameters"]["max_distance"]["default"].get<float>(),label+": proximity settings require proximity_mesh");
        if(op=="instance"||op=="ribbon") {
            if(node.contains("atlas")) { auto atlas=readAtlas(graph.base/node["atlas"].get<std::string>()); require(node["atlas_index"].get<size_t>()<atlas.regions.size(),label+": atlas_index is not an occupied cell"); }
            else require(node["atlas_mode"]=="random"&&node["atlas_index"]==0,label+": atlas selection requires an atlas manifest");
        }
        if(op=="mesh") { auto path=graph.base/node["path"].get<std::string>(); require(path.extension()==".obj","mesh input must be .obj"); require(std::filesystem::is_regular_file(path),"missing mesh: "+path.string()); require(std::filesystem::file_size(path)<=64*1024*1024,"OBJ exceeds 64 MiB"); }
    }
    std::map<std::string,int> state;
    std::function<void(const std::string&)> visit=[&](const std::string& id) {
        require(doc["nodes"].contains(id),"unknown node reference '"+id+"'");
        require(state[id]!=1,"cycle involving node '"+id+"'"); if(state[id]==2) return;
        state[id]=1; const auto& node=doc["nodes"][id]; auto refs=references(node);
        for(const auto& name:refs) visit(name);
        const auto spec=cat[node["op"].get<std::string>()];
        for(auto it=spec["parameters"].begin();it!=spec["parameters"].end();++it) if(node.contains(it.key())&&it.value()["type"]=="reference") {
            std::string actual=kindName(graph.kinds.at(node[it.key()].get<std::string>())); const auto& kinds=it.value().at("kinds");
            require(std::find(kinds.begin(),kinds.end(),actual)!=kinds.end(),"node '"+id+"'."+it.key()+": incompatible input type "+actual);
        }
        if(node["op"]=="scatter"&&node.contains("normal_direction")) require(node.contains("input")&&graph.kinds.at(node["input"].get<std::string>())==Kind::mesh,"normal_direction requires mesh input");
        std::string output=spec["output"];
        Kind kind=output=="skeleton"?Kind::skeleton:output=="points"?Kind::points:Kind::mesh;
        if(output=="same") {
            kind=graph.kinds.at(refs.at(0)); for(const auto& r:refs) require(graph.kinds.at(r)==kind,"node '"+id+"': merge inputs must have the same type");
        }
        graph.kinds[id]=kind; graph.order.push_back(id); state[id]=2;
    };
    for(auto it=doc["nodes"].begin();it!=doc["nodes"].end();++it) visit(it.key());
    std::set<std::string> outputNames;
    for(auto it=doc["outputs"].begin();it!=doc["outputs"].end();++it) {
        auto path=std::filesystem::path(it.key());
        require(!it.key().empty()&&!path.is_absolute()&&it.key().find('\\')==std::string::npos&&it.key().find_first_of("\r\n\t") == std::string::npos&&it.key().find('\0')==std::string::npos,"output path must be relative and portable");
        for(const auto& component:path) require(component!=".."&&component!=".","output path cannot contain . or ..");
        require(path.extension()==".obj"||path.extension()==".glb","output extension must be .obj or .glb");
        require(it.value().is_string(),"output must refer to a mesh node by name");
        std::string id=it.value(); require(graph.kinds.count(id)&&graph.kinds.at(id)==Kind::mesh,"output '"+it.key()+"' needs a mesh node");
        require(outputNames.insert(path.generic_string()).second,"duplicate output path");
        if(path.extension()==".obj") { path.replace_extension(".mtl"); require(outputNames.insert(path.generic_string()).second,"output sidecar collision"); }
    }
    validateCardBakes(graph);
    validateGrowth(graph);
    validateLods(graph);
    validateExportProfiles(graph);
    return graph;
}
static Result generateTargets(const Graph& graph, const Options& options, const Json& targets) {
    if(options.growth) require(std::isfinite(*options.growth)&&*options.growth>=0&&*options.growth<=1,"growth progress must be within 0..1");
    Budget budget{options.limits}; Inputs cache,matureProximity; Result result;
    int32_t seed=options.seed.value_or(graph.document.at("seed").get<int32_t>());
    std::set<std::string> needed;
    std::function<void(const std::string&)> mark=[&](const std::string& id) { if(!needed.insert(id).second) return; for(const auto& ref:references(graph.document["nodes"][id])) mark(ref); };
    for(const auto& output:targets) mark(output.get<std::string>());
    Json stats=Json::array();
    bool developmental=options.growth&&*options.growth<1&&graph.document.contains("growth")&&graph.document["growth"]["mode"]=="developmental";
    for(const auto& id:graph.order) if(needed.count(id)) {
        const auto& node=graph.document["nodes"][id];
        Random random{hashName(id)^static_cast<uint64_t>(static_cast<uint32_t>(node.value("seed",seed)))};
        auto start=std::chrono::steady_clock::now();
        try {
            Json diagnostics=Json::object();
            float factor=options.growth?growthFactor(graph,id,*options.growth):1;
            GrowthContext growth{options.growth.value_or(1),nullptr};
            if(developmental&&graph.document["growth"]["stages"].contains(id)) growth.stage=&graph.document["growth"]["stages"][id];
            if(developmental&&node["op"]=="scatter"&&node.contains("proximity_mesh")&&node.contains("input")&&graph.kinds.at(node["input"].get<std::string>())==Kind::skeleton) {
                std::string support=node["proximity_mesh"];
                if(!matureProximity.count(support)) {
                    Options mature=options; mature.growth.reset();
                    mature.limits={budget.limit.vertices-budget.vertices,budget.limit.triangles-budget.triangles,budget.limit.points-budget.points};
                    auto prepared=generateTargets(graph,mature,Json{{support,support}});
                    budget.charge(prepared.stats["generated_vertices"].get<size_t>(),prepared.stats["generated_triangles"].get<size_t>(),prepared.stats["generated_points"].get<size_t>());
                    matureProximity[support]=prepared.outputs.at(support);
                }
                growth.mature_proximity=&matureProximity.at(support)->mesh;
            }
            auto empty=[](const Value& v) { return v.kind==Kind::mesh?v.mesh.triangles.empty():v.kind==Kind::points?v.points.empty():v.stems.empty(); };
            bool skip=developmental?(graph.kinds.at(id)==Kind::mesh&&node["op"]!="instance"&&growth.stage&&growthScale(growth.stage,growth.progress,0)==0):factor==0;
            if(options.growth&&(node["op"]=="scatter"||node["op"]=="solidify")) {
                for(auto key:{"input","proximity_mesh"}) if(node.contains(key)&&empty(*cache.at(node[key].get<std::string>()))&&!(developmental&&node["op"]=="scatter")) skip=true;
            }
            auto value=std::make_shared<Value>(); value->kind=graph.kinds.at(id);
            if(!skip) {
                Json evaluated=node;
                if(!developmental&&factor!=1&&node["op"]=="instance") evaluated["scale"]=node["scale"].get<float>()*factor;
                *value=evaluate(evaluated,cache,random,budget,graph.base,&diagnostics,developmental?&growth:nullptr);
                if(developmental) developValue(*value,node,growth,diagnostics);
                else if(factor!=1&&node["op"]!="instance") growValue(*value,graph.document["growth"]["stages"][id],factor);
            }
            if(options.growth) diagnostics["growth_factor"]=factor;
            require(value->kind==graph.kinds.at(id),"internal node type mismatch");
            if(value->kind==Kind::mesh) checkMesh(value->mesh);
            cache[id]=value;
            Json s={{"node",id},{"op",node["op"]},{"kind",kindName(value->kind)},{"milliseconds",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()}};
            s.update(diagnostics);
            if(value->kind==Kind::mesh) s["mesh"]=meshStats(value->mesh);
            else { s["stems"]=value->stems.size(); s["points"]=value->points.size(); }
            if(node["op"]=="scatter"&&node.contains("proximity_mesh")) {
                size_t copies=1;
                if(node.contains("input")) { const auto& source=*cache.at(node["input"].get<std::string>()); if(source.kind==Kind::skeleton) copies=source.stems.size(); }
                size_t candidates=copies*node["count"].get<size_t>(),accepted=value->points.size();
                if(diagnostics.contains("normal_filter")) candidates=diagnostics["normal_filter"]["accepted"].get<size_t>();
                s["proximity"]={{"candidates",candidates},{"accepted",accepted},{"rejected",candidates-accepted},{"snapped",node["snap_to_mesh"].get<bool>()?accepted:0},{"max_distance",node["max_distance"]}};
                if(developmental) {
                    size_t visible=std::count_if(value->points.begin(),value->points.end(),[](const Point& point) { return point.growth_visible; });
                    s["proximity"]["visible"]=visible;
                    s["proximity"]["snapped"]=node["snap_to_mesh"].get<bool>()?visible:0;
                }
            }
            stats.push_back(s);
        } catch(const std::exception& e) { throw std::runtime_error("node '"+id+"': "+e.what()); }
    }
    for(auto it=targets.begin();it!=targets.end();++it) {
        auto value=cache.at(it.value().get<std::string>());
        require(options.growth.has_value()||value->kind!=Kind::mesh||!value->mesh.triangles.empty(),"output '"+it.key()+"' is empty"); result.outputs[it.key()]=value;
    }
    result.stats={{"seed",seed},{"nodes",stats},{"generated_vertices",budget.vertices},{"generated_triangles",budget.triangles},{"generated_points",budget.points}};
    if(options.growth) result.stats["growth_progress"]=*options.growth;
    return result;
}
Result generate(const Graph& graph, const Options& options) { return generateTargets(graph,options,graph.document["outputs"]); }
Result generateNodes(const Graph& graph, const Options& options, const std::vector<std::string>& names) {
    Json targets=Json::object();
    for(const auto& name:names) { require(graph.kinds.count(name),"unknown node: "+name); targets[name]=name; }
    return generateTargets(graph,options,targets);
}
Json meshStats(const Mesh& mesh) {
    Vec3 lo{},hi{}; if(!mesh.vertices.empty()) lo=hi=mesh.vertices.front().position;
    for(const auto& v:mesh.vertices) { lo={std::min(lo.x,v.position.x),std::min(lo.y,v.position.y),std::min(lo.z,v.position.z)}; hi={std::max(hi.x,v.position.x),std::max(hi.y,v.position.y),std::max(hi.z,v.position.z)}; }
    std::map<std::string,size_t> materials; for(const auto& t:mesh.triangles) ++materials[t.material];
    return {{"vertices",mesh.vertices.size()},{"triangles",mesh.triangles.size()},{"materials",materials},{"bounds",{{"min",{lo.x,lo.y,lo.z}},{"max",{hi.x,hi.y,hi.z}}}}};
}
void checkMesh(const Mesh& mesh) {
    for(const auto& v:mesh.vertices) {
        require(finite(v.position)&&finite(v.normal)&&std::isfinite(v.uv.x)&&std::isfinite(v.uv.y),"nonfinite vertex data");
        require(std::abs(length(v.normal)-1)<0.002f,"nonunit normal");
        require(std::isfinite(v.wind.x)&&std::isfinite(v.wind.y),"nonfinite wind data");
        for(float c:v.color) require(std::isfinite(c)&&c>=0&&c<=1,"invalid vertex color");
    }
    for(const auto& t:mesh.triangles) {
        for(auto i:t.vertices) require(i<mesh.vertices.size(),"triangle index out of range");
        Vec3 a=mesh.vertices[t.vertices[0]].position,b=mesh.vertices[t.vertices[1]].position,c=mesh.vertices[t.vertices[2]].position;
        // Boolean intersections can create thin valid triangles. Subtract in
        // double precision so float cancellation cannot falsely flatten them.
        double x=double(b.x)-a.x,y=double(b.y)-a.y,z=double(b.z)-a.z;
        double u=double(c.x)-a.x,v=double(c.y)-a.y,w=double(c.z)-a.z;
        require(y*w-z*v!=0||z*u-x*w!=0||x*v-y*u!=0,"degenerate triangle");
    }
}
}
