#include "internal.hpp"
#include <fstream>
#include <numeric>
#ifdef FOLIAGE_ALEMBIC
#include <Alembic/AbcGeom/All.h>
#include <Alembic/AbcCoreOgawa/All.h>
#endif

namespace foliage {
bool alembicAvailable() {
#ifdef FOLIAGE_ALEMBIC
    return true;
#else
    return false;
#endif
}
#ifdef FOLIAGE_ALEMBIC
namespace {
using namespace Alembic::AbcGeom;
// Explicit zero-length arrays must still have non-null data: null topology
// means "reuse the previous sample" to some Alembic writer paths.
template<class T> const T* sampleData(const std::vector<T>& values) { static const T empty{}; return values.empty()?&empty:values.data(); }
struct Track {
    OPolyMesh object;
    OFaceSet faces;
    OC4fGeomParam colors;
    OV2fGeomParam wind;
    OVisibilityProperty visibility;
    Track(OObject parent, const std::string& name, const std::string& material, uint32_t sampling) : object(parent,name,sampling), faces(object.getSchema().createFaceSet(material)), colors(object.getSchema().getArbGeomParams(),"Color",false,kVertexScope,1,sampling), wind(object.getSchema().getArbGeomParams(),"_WIND",false,kVertexScope,1,sampling), visibility(CreateVisibilityProperty(object,sampling)) {
        object.getSchema().setUVSourceName("UVMap"); faces.getSchema().setTimeSampling(sampling);
        OStringProperty(object.getSchema().getUserProperties(),"foliage_material").set(material);
    }
    void sample(const Mesh& mesh, const std::vector<size_t>& selected) {
        std::vector<int32_t> remap(mesh.vertices.size(),-1),indices,counts(selected.size(),3),faceIds(selected.size());
        std::vector<V3f> positions,normals; std::vector<V2f> uvs,weights; std::vector<C4f> color;
        indices.reserve(selected.size()*3);
        // Alembic uses clockwise faces; FoliageUtil uses counterclockwise faces.
        for(auto face:selected) for(auto corner:{2,1,0}) {
            auto id=mesh.triangles[face].vertices[corner];
            if(remap[id]<0) {
                remap[id]=int32_t(positions.size()); const auto& v=mesh.vertices[id];
                positions.emplace_back(v.position.x,v.position.y,v.position.z); normals.emplace_back(v.normal.x,v.normal.y,v.normal.z);
                uvs.emplace_back(v.uv.x,v.uv.y); weights.emplace_back(v.wind.x,v.wind.y); color.emplace_back(v.color[0],v.color[1],v.color[2],v.color[3]);
            }
            indices.push_back(remap[id]);
        }
        std::vector<uint32_t> uvIndices(indices.begin(),indices.end());
        OV2fGeomParam::Sample uv(V2fArraySample(sampleData(uvs),uvs.size()),UInt32ArraySample(sampleData(uvIndices),uvIndices.size()),kFacevaryingScope);
        ON3fGeomParam::Sample normal(N3fArraySample(sampleData(normals),normals.size()),kVertexScope);
        object.getSchema().set(OPolyMeshSchema::Sample(P3fArraySample(sampleData(positions),positions.size()),Int32ArraySample(sampleData(indices),indices.size()),Int32ArraySample(sampleData(counts),counts.size()),uv,normal));
        colors.set(OC4fGeomParam::Sample(C4fArraySample(sampleData(color),color.size()),kVertexScope));
        wind.set(OV2fGeomParam::Sample(V2fArraySample(sampleData(weights),weights.size()),kVertexScope));
        std::iota(faceIds.begin(),faceIds.end(),0); faces.getSchema().set(OFaceSetSchema::Sample(Int32ArraySample(sampleData(faceIds),faceIds.size())));
        visibility.set(selected.empty()?kVisibilityHidden:kVisibilityDeferred);
    }
};
Json materials(const Graph& graph, const Json& bindings, const std::filesystem::path& directory, size_t limit) {
    Json result=Json::object(); std::map<std::filesystem::path,std::string> copied; size_t total=std::filesystem::file_size(directory/"animation.abc");
    for(const auto& binding:bindings) {
        std::string name=binding["material"]; auto value=graph.document["materials"].at(name);
        for(auto key:{"base_color_texture","normal_texture","metallic_roughness_texture"}) if(value.contains(key)) {
            auto source=std::filesystem::canonical(graph.base/value[key].get<std::string>());
            if(!copied.count(source)) {
                size_t size=std::filesystem::file_size(source); require(size<=64*1024*1024,"texture exceeds 64 MiB"); require(size<=limit-total,"export max_output_mb exceeded");
                unsigned char header[8]{}; std::ifstream file(source,std::ios::binary); file.read(reinterpret_cast<char*>(header),8);
                const unsigned char png[]={137,80,78,71,13,10,26,10}; bool isPng=size>=8&&std::equal(std::begin(png),std::end(png),header);
                require(isPng||(size>=3&&header[0]==255&&header[1]==216&&header[2]==255),"texture must contain PNG or JPEG data");
                std::string path="textures/"+std::to_string(copied.size())+(isPng?".png":".jpg"); std::filesystem::create_directories(directory/"textures");
                std::filesystem::copy_file(source,directory/path); copied[source]=path; total+=size;
            }
            value[key]=copied.at(source);
        }
        result[name]=value;
    }
    Json bundle={{"version",1},{"materials",result},{"bindings",bindings},{"vertex_color","Color"},{"wind_attribute","_WIND"},{"texture_uv_origin","bottom-left"}};
    std::ofstream file(directory/"materials.json"); file<<bundle.dump(2)<<'\n'; file.close(); require(bool(file),"cannot write Alembic materials.json"); return bundle;
}
}
#endif
Json exportAlembicSamples(const Graph& graph, const Json& profile, const std::filesystem::path& directory, const Options& options) {
#ifdef FOLIAGE_ALEMBIC
    size_t frames=profile["frames"],estimate=0,limit=profile["max_samples_mb"].get<size_t>()*1024*1024,outputLimit=profile["max_output_mb"].get<size_t>()*1024*1024;
    double fps=profile["fps"],start=profile["start"],end=profile["end"]; std::string source=profile["source"];
    Json snapshots=Json::array(),bindings=Json::array(); auto path=directory/"animation.abc";
    {
        OArchive archive(Alembic::AbcCoreOgawa::WriteArchive(),path.string());
        uint32_t sampling=archive.addTimeSampling(TimeSampling(1.0/fps,0)); OXform root(archive.getTop(),"Foliage"); XformSample identity; root.getSchema().set(identity);
        OStringProperty(root.getSchema().getUserProperties(),"coordinates").set("right-handed Y-up meters");
        std::map<std::string,std::unique_ptr<Track>> tracks;
        for(size_t frame=0;frame<frames;++frame) try {
            Options opt=options; opt.growth=start+(end-start)*frame/(frames-1);
            auto generated=generateNodes(graph,opt,{source}); const auto& mesh=generated.outputs.at(source)->mesh;
            size_t bytes=mesh.vertices.size()*sizeof(Vertex)+mesh.triangles.size()*sizeof(Triangle);
            require(bytes<=limit-estimate,"export max_samples_mb exceeded"); estimate+=bytes;
            std::map<std::string,std::vector<size_t>> groups;
            for(size_t i=0;i<mesh.triangles.size();++i) groups[mesh.triangles[i].material].push_back(i);
            for(const auto& group:groups) if(!tracks.count(group.first)) {
                std::string object="Mesh_"+std::to_string(tracks.size()); auto track=std::make_unique<Track>(root,object,group.first,sampling);
                for(size_t previous=0;previous<frame;++previous) track->sample(Mesh{},{});
                bindings.push_back({{"object","/Foliage/"+object},{"material",group.first},{"face_set",group.first}}); tracks[group.first]=std::move(track);
            }
            for(auto& track:tracks) track.second->sample(mesh,groups[track.first]);
            snapshots.push_back({{"frame",frame},{"time",frame/fps},{"progress",*opt.growth},{"mesh",meshStats(mesh)}});
            require(std::filesystem::file_size(path)<=outputLimit,"export max_output_mb exceeded");
        } catch(const std::exception& error) { throw std::runtime_error("Alembic frame "+std::to_string(frame)+": "+error.what()); }
    }
    require(std::filesystem::file_size(path)<=outputLimit,"export max_output_mb exceeded"); materials(graph,bindings,directory,outputLimit);
    return {{"animation","animation.abc"},{"materials","materials.json"},{"frames",frames},{"fps",fps},{"duration",(frames-1)/fps},{"topology","varying"},{"snapshots",snapshots},{"bindings",bindings},{"estimated_samples_mb",double(estimate)/(1024*1024)}};
#else
    (void)graph; (void)profile; (void)directory; (void)options;
    throw std::runtime_error("Alembic support is disabled; rebuild with -DFOLIAGE_ALEMBIC=ON");
#endif
}
}
