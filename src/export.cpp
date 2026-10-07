#include "internal.hpp"
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <set>

namespace foliage {
namespace {
using Bytes=std::vector<uint8_t>;
void u32(Bytes& bytes, uint32_t value) { for(int i=0;i<4;++i) bytes.push_back(uint8_t(value>>(i*8))); }
void f32(Bytes& bytes, float value) { uint32_t bits; static_assert(sizeof(value)==sizeof(bits)); std::memcpy(&bits,&value,4); u32(bytes,bits); }
void pad(Bytes& bytes) { while(bytes.size()%4) bytes.push_back(0); }
void save(const std::filesystem::path& path, const Bytes& bytes) {
    std::ofstream out(path,std::ios::binary); require(bool(out),"cannot write "+path.string());
    out.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size())); out.close(); require(bool(out),"failed writing "+path.string());
}
Bytes read(const std::filesystem::path& path) {
    auto size=std::filesystem::file_size(path); require(size<=64*1024*1024,"texture exceeds 64 MiB");
    std::ifstream in(path,std::ios::binary); require(bool(in),"cannot read "+path.string()); Bytes result(size); in.read(reinterpret_cast<char*>(result.data()),std::streamsize(size)); require(bool(in),"failed reading "+path.string()); return result;
}
std::string mime(const Bytes& b) {
    const uint8_t png[]={137,80,78,71,13,10,26,10};
    if(b.size()>=8&&std::equal(std::begin(png),std::end(png),b.begin())) return "image/png";
    if(b.size()>=3&&b[0]==255&&b[1]==216&&b[2]==255) return "image/jpeg";
    throw std::runtime_error("texture must contain PNG or JPEG data");
}
std::vector<std::string> usedMaterials(const Mesh& mesh) { std::set<std::string> set; for(const auto& face:mesh.triangles) set.insert(face.material); return {set.begin(),set.end()}; }
std::filesystem::path textureDestination(const std::filesystem::path& filename, const std::string& material, const std::string& key, const std::filesystem::path& source) { return filename.parent_path()/(filename.stem().string()+"-textures")/(material+"-"+key+source.extension().string()); }
}
void saveObj(const Mesh& mesh, const Json& materials, const std::filesystem::path& base, const std::filesystem::path& filename) {
    checkMesh(mesh); require(!mesh.triangles.empty(),"cannot export empty mesh");
    std::filesystem::create_directories(filename.parent_path().empty()?".":filename.parent_path());
    auto mtl=filename; mtl.replace_extension(".mtl");
    std::ofstream out(filename),materialFile(mtl); require(bool(out)&&bool(materialFile),"cannot write OBJ/MTL");
    out<<std::setprecision(std::numeric_limits<float>::max_digits10); materialFile<<std::setprecision(9);
    out<<"# FoliageUtil 0.1.0, meters, right-handed Y-up\nmtllib "<<mtl.filename().string()<<'\n';
    for(const auto& v:mesh.vertices) out<<"v "<<v.position.x<<' '<<v.position.y<<' '<<v.position.z<<'\n';
    for(const auto& v:mesh.vertices) out<<"vt "<<v.uv.x<<' '<<v.uv.y<<'\n';
    for(const auto& v:mesh.vertices) out<<"vn "<<v.normal.x<<' '<<v.normal.y<<' '<<v.normal.z<<'\n';
    std::string current;
    for(const auto& face:mesh.triangles) {
        if(current!=face.material) { current=face.material; out<<"usemtl "<<current<<'\n'; }
        out<<"f"; for(auto i:face.vertices) { auto index=i+1; out<<' '<<index<<'/'<<index<<'/'<<index; } out<<'\n';
    }
    for(const auto& name:usedMaterials(mesh)) {
        const auto& m=materials.at(name); auto color=m.at("base_color");
        materialFile<<"newmtl "<<name<<"\nKd "<<color[0]<<' '<<color[1]<<' '<<color[2]<<"\nd "<<color[3]<<"\nPr "<<m["roughness"]<<"\nPm "<<m["metallic"]<<"\nillum 2\n";
        for(auto key:{"base_color_texture","normal_texture"}) if(m.contains(key)) {
            auto source=base/m[key].get<std::string>(),dest=textureDestination(filename,name,key,source); auto bytes=read(source); mime(bytes);
            require(std::filesystem::weakly_canonical(source)!=std::filesystem::weakly_canonical(dest),"output would overwrite a source texture");
            std::filesystem::create_directories(dest.parent_path()); save(dest,bytes);
            materialFile<<(std::string(key)=="base_color_texture"?"map_Kd ":"norm ")<<std::filesystem::relative(dest,filename.parent_path()).generic_string()<<'\n';
        }
        materialFile<<'\n';
    }
    out.close(); materialFile.close(); require(bool(out)&&bool(materialFile),"failed writing OBJ/MTL");
}
void saveGlb(const Mesh& mesh, const Json& materials, const std::filesystem::path& base, const std::filesystem::path& filename) {
    checkMesh(mesh); require(!mesh.triangles.empty(),"cannot export empty mesh");
    Json gltf={{"asset",{{"version","2.0"},{"generator","FoliageUtil 0.1.0"}}},{"scene",0},
        {"scenes",Json::array({{{"nodes",{0}}}})},{"nodes",Json::array({{{"mesh",0},{"name",filename.stem().string()}}})},
        {"accessors",Json::array()},{"bufferViews",Json::array()},{"materials",Json::array()}};
    Bytes binary;
    auto view=[&](size_t start, int target) { Json v={{"buffer",0},{"byteOffset",start},{"byteLength",binary.size()-start}}; if(target) v["target"]=target; int id=int(gltf["bufferViews"].size()); gltf["bufferViews"].push_back(v); pad(binary); return id; };
    auto accessor=[&](int bv, int component, const char* type, size_t count) { int id=int(gltf["accessors"].size()); gltf["accessors"].push_back({{"bufferView",bv},{"componentType",component},{"type",type},{"count",count}}); return id; };
    size_t start=binary.size(); for(const auto& v:mesh.vertices) { f32(binary,v.position.x); f32(binary,v.position.y); f32(binary,v.position.z); }
    int position=accessor(view(start,34962),5126,"VEC3",mesh.vertices.size()); auto stats=meshStats(mesh); gltf["accessors"][position]["min"]=stats["bounds"]["min"]; gltf["accessors"][position]["max"]=stats["bounds"]["max"];
    start=binary.size(); for(const auto& v:mesh.vertices) { f32(binary,v.normal.x); f32(binary,v.normal.y); f32(binary,v.normal.z); } int normal=accessor(view(start,34962),5126,"VEC3",mesh.vertices.size());
    start=binary.size(); for(const auto& v:mesh.vertices) { f32(binary,v.uv.x); f32(binary,1-v.uv.y); } int uv=accessor(view(start,34962),5126,"VEC2",mesh.vertices.size());
    start=binary.size(); for(const auto& v:mesh.vertices) for(float c:v.color) f32(binary,c); int color=accessor(view(start,34962),5126,"VEC4",mesh.vertices.size());
    start=binary.size(); for(const auto& v:mesh.vertices) { f32(binary,v.wind.x); f32(binary,v.wind.y); } int wind=accessor(view(start,34962),5126,"VEC2",mesh.vertices.size());
    Json primitives=Json::array();
    std::map<std::string,int> textures;
    auto texture=[&](const std::string& path) {
        if(textures.count(path)) return textures.at(path);
        Bytes bytes=read(base/path); std::string type=mime(bytes); size_t offset=binary.size(); binary.insert(binary.end(),bytes.begin(),bytes.end()); int bv=view(offset,0);
        if(!gltf.contains("images")) { gltf["images"]=Json::array(); gltf["textures"]=Json::array(); gltf["samplers"]=Json::array({{{"magFilter",9729},{"minFilter",9987},{"wrapS",10497},{"wrapT",10497}}}); }
        int id=int(gltf["images"].size()); gltf["images"].push_back({{"bufferView",bv},{"mimeType",type}}); gltf["textures"].push_back({{"source",id},{"sampler",0}}); textures[path]=id; return id;
    };
    std::set<std::string> extensions;
    for(const auto& name:usedMaterials(mesh)) {
        const auto& source=materials.at(name); int mat=int(gltf["materials"].size());
        Json m={{"name",name},{"pbrMetallicRoughness",{{"baseColorFactor",source["base_color"]},{"roughnessFactor",source["roughness"]},{"metallicFactor",source["metallic"]}}},
            {"doubleSided",source["double_sided"]},{"alphaMode",source["alpha_mode"]}};
        if(source["alpha_mode"]=="MASK") m["alphaCutoff"]=source["alpha_cutoff"];
        if(source.contains("base_color_texture")) m["pbrMetallicRoughness"]["baseColorTexture"]={{"index",texture(source["base_color_texture"])}};
        if(source.contains("metallic_roughness_texture")) m["pbrMetallicRoughness"]["metallicRoughnessTexture"]={{"index",texture(source["metallic_roughness_texture"])}};
        if(source.contains("normal_texture")) m["normalTexture"]={{"index",texture(source["normal_texture"])}};
        if(source.value("transmission",0.f)>0) { m["extensions"]["KHR_materials_transmission"]={{"transmissionFactor",source["transmission"]}}; extensions.insert("KHR_materials_transmission"); }
        if(source.value("ior",1.5f)!=1.5f) { m["extensions"]["KHR_materials_ior"]={{"ior",source["ior"]}}; extensions.insert("KHR_materials_ior"); }
        if(source.value("thickness",0.f)>0) {
            auto& volume=m["extensions"]["KHR_materials_volume"]; volume={{"thicknessFactor",source["thickness"]},{"attenuationColor",source["attenuation_color"]}};
            if(source.contains("attenuation_distance")) volume["attenuationDistance"]=source["attenuation_distance"];
            extensions.insert("KHR_materials_volume");
        }
        if(source.value("translucency",0.0)>0||source.value("subsurface",0.0)>0) {
            Json optics={{"version",1}};
            for(auto key:{"translucency","translucency_color","subsurface","subsurface_scale","subsurface_radius"}) if(source.contains(key)) optics[key]=source[key];
            m["extras"]["foliageutil"]=optics;
        }
        gltf["materials"].push_back(m);
        start=binary.size(); size_t count=0;
        for(const auto& face:mesh.triangles) if(face.material==name) for(auto index:face.vertices) { u32(binary,index); ++count; }
        int indices=accessor(view(start,34963),5125,"SCALAR",count);
        primitives.push_back({{"attributes",{{"POSITION",position},{"NORMAL",normal},{"TEXCOORD_0",uv},{"COLOR_0",color},{"_WIND",wind}}},{"indices",indices},{"material",mat},{"mode",4}});
    }
    if(!extensions.empty()) gltf["extensionsUsed"]=extensions;
    Json extras={{"units","meters"},{"wind_attribute","_WIND: x=bending weight, y=phase in cycles"},{"uvs","reusable per-part UVs; not a unique bake atlas"}};
    gltf["meshes"]=Json::array({{{"primitives",primitives},{"extras",{{"foliageutil",extras}}}}});
    gltf["buffers"]=Json::array({{{"byteLength",binary.size()}}});
    std::string json=gltf.dump(); while(json.size()%4) json+=' ';
    require(28+json.size()+binary.size()<=std::numeric_limits<uint32_t>::max(),"GLB exceeds 4 GiB");
    Bytes file; file.reserve(28+json.size()+binary.size()); u32(file,0x46546c67); u32(file,2); u32(file,uint32_t(28+json.size()+binary.size()));
    u32(file,uint32_t(json.size())); u32(file,0x4e4f534a); file.insert(file.end(),json.begin(),json.end()); u32(file,uint32_t(binary.size())); u32(file,0x004e4942); file.insert(file.end(),binary.begin(),binary.end());
    std::filesystem::create_directories(filename.parent_path().empty()?".":filename.parent_path()); save(filename,file);
}
void preflightExports(const std::vector<Graph>& graphs, const std::filesystem::path& directory, const std::vector<std::string>& manifests) {
    auto root=std::filesystem::weakly_canonical(std::filesystem::absolute(directory));
    std::set<std::filesystem::path> sources,planned;
    for(const auto& graph:graphs) {
        for(const auto& node:graph.document["nodes"]) {
            if(node["op"]=="mesh") sources.insert(std::filesystem::weakly_canonical(graph.base/node["path"].get<std::string>()));
            if((node["op"]=="instance"||node["op"]=="ribbon")&&node.contains("atlas")) { auto atlas=readAtlas(graph.base/node["atlas"].get<std::string>()); sources.insert(atlas.files.begin(),atlas.files.end()); }
        }
        for(const auto& m:graph.document["materials"]) for(auto key:{"base_color_texture","normal_texture","metallic_roughness_texture"}) if(m.contains(key)) sources.insert(std::filesystem::weakly_canonical(graph.base/m[key].get<std::string>()));
    }
    auto check=[&](const std::filesystem::path& path) {
        auto canonical=std::filesystem::weakly_canonical(path),relative=canonical.lexically_relative(root);
        require(!relative.empty()&&!relative.is_absolute()&&*relative.begin()!="..","output resolves outside output directory: "+path.string());
        require(!sources.count(canonical),"output would overwrite input asset: "+path.string());
        require(planned.insert(canonical).second,"output/sidecar collision: "+path.string());
    };
    for(const auto& graph:graphs) for(auto it=graph.document["outputs"].begin();it!=graph.document["outputs"].end();++it) {
        auto path=root/it.key(); check(path);
        if(path.extension()==".obj") {
            auto mtl=path; mtl.replace_extension(".mtl"); check(mtl);
            // Conservatively reserve possible texture sidecars before generation.
            for(auto m=graph.document["materials"].begin();m!=graph.document["materials"].end();++m) for(auto key:{"base_color_texture","normal_texture"}) if(m.value().contains(key)) check(textureDestination(path,m.key(),key,graph.base/m.value()[key].get<std::string>()));
        }
    }
    for(const auto& name:manifests) check(root/name);
}
Json write(const Graph& graph, const Result& result, const std::filesystem::path& directory) {
    auto root=std::filesystem::weakly_canonical(std::filesystem::absolute(directory));
    std::set<std::filesystem::path> sources,planned;
    for(const auto& node:graph.document["nodes"]) if(node["op"]=="mesh") sources.insert(std::filesystem::weakly_canonical(graph.base/node["path"].get<std::string>()));
    for(const auto& node:graph.document["nodes"]) if((node["op"]=="instance"||node["op"]=="ribbon")&&node.contains("atlas")) { auto atlas=readAtlas(graph.base/node["atlas"].get<std::string>()); sources.insert(atlas.files.begin(),atlas.files.end()); }
    for(const auto& m:graph.document["materials"]) for(auto key:{"base_color_texture","normal_texture","metallic_roughness_texture"}) if(m.contains(key)) sources.insert(std::filesystem::weakly_canonical(graph.base/m[key].get<std::string>()));
    auto check=[&](const std::filesystem::path& p) {
        auto canonical=std::filesystem::weakly_canonical(p); auto relative=canonical.lexically_relative(root);
        require(!relative.empty()&&!relative.is_absolute()&&*relative.begin()!="..","output resolves outside output directory: "+p.string());
        require(!sources.count(canonical),"output would overwrite input asset: "+p.string()); require(planned.insert(canonical).second,"output/sidecar collision: "+p.string());
    };
    for(const auto& [name,value]:result.outputs) {
        auto path=root/name; check(path);
        if(path.extension()==".obj") {
            auto mtl=path; mtl.replace_extension(".mtl"); check(mtl);
            for(const auto& mat:usedMaterials(value->mesh)) for(auto key:{"base_color_texture","normal_texture"}) {
                const auto& m=graph.document["materials"][mat]; if(m.contains(key)) check(textureDestination(path,mat,key,graph.base/m[key].get<std::string>()));
            }
        }
    }
    Json report=Json::array(),empty=Json::array();
    for(const auto& [name,value]:result.outputs) {
        auto path=root/name;
        if(value->mesh.triangles.empty()&&result.stats.contains("growth_progress")) {
            require(!std::filesystem::is_directory(path),"empty growth output is a directory: "+path.string());
            std::filesystem::remove(path);
            if(path.extension()==".obj") { auto mtl=path; mtl.replace_extension(".mtl"); require(!std::filesystem::is_directory(mtl),"empty growth sidecar is a directory"); std::filesystem::remove(mtl); }
            empty.push_back(name); continue;
        }
        if(path.extension()==".obj") saveObj(value->mesh,graph.document["materials"],graph.base,path); else saveGlb(value->mesh,graph.document["materials"],graph.base,path);
        auto item=meshStats(value->mesh); item["path"]=path.string(); report.push_back(item);
    }
    Json answer={{"outputs",report},{"stats",result.stats}}; if(!empty.empty()) answer["empty_outputs"]=empty; return answer;
}
}
