#include "internal.hpp"
#include <manifold/manifold.h>
#include <manifold/mesh.h>
#include <map>
#include <limits>

namespace foliage {
namespace {
Mesh solidOperation(const Mesh& mesh, const Mesh* tool, const Json& node, Budget& budget, Json* diagnostics) {
    using manifold::Manifold;
    std::string op=tool?node["operation"].get<std::string>():"union",label=tool?"boolean":"solidify";
    require(tool||!mesh.triangles.empty(),"solidify needs a nonempty closed mesh");
    require(mesh.triangles.size()+(tool?tool->triangles.size():0)<=node["max_input_triangles"].get<size_t>(),label+" input triangle budget exceeded");
    // Temporary boolean workspace is separate from output geometry. Check source
    // limits before preparing it; charge the result before allocating export data.
    require(mesh.vertices.size()+(tool?tool->vertices.size():0)<=budget.limit.vertices,label+" input vertex budget exceeded");
    float coordinateScale=0;
    for(const auto* operand:{&mesh,tool}) if(operand) for(const auto& v:operand->vertices) coordinateScale=std::max({coordinateScale,std::abs(v.position.x),std::abs(v.position.y),std::abs(v.position.z)});
    // Boolean calculations use doubles, but our public mesh and GLB positions
    // are floats. Collapse numerical slivers before conversion can flatten them.
    float tolerance=std::max(node["weld_tolerance"].get<float>(),coordinateScale*std::numeric_limits<float>::epsilon()*4);
    std::map<uint32_t,std::string> names; size_t shellCount=0;
    auto prepare=[&](const Mesh& operand, bool uniteShells) {
        if(operand.triangles.empty()) return Manifold{};
        manifold::MeshGL source; source.numProp=14; source.tolerance=tolerance;
        source.vertProperties.reserve(operand.vertices.size()*source.numProp);
        for(const auto& v:operand.vertices) {
            for(float x:{v.position.x,v.position.y,v.position.z,v.normal.x,v.normal.y,v.normal.z,v.uv.x,v.uv.y,v.color[0],v.color[1],v.color[2],v.color[3],v.wind.x,v.wind.y}) source.vertProperties.push_back(x);
        }
        std::map<std::string,std::vector<const Triangle*>> materials;
        for(const auto& triangle:operand.triangles) materials[triangle.material].push_back(&triangle);
        uint32_t id=Manifold::ReserveIDs(uint32_t(materials.size()));
        source.triVerts.reserve(operand.triangles.size()*3);
        for(const auto& [name,triangles]:materials) {
            source.runIndex.push_back(uint32_t(source.triVerts.size()));
            source.runOriginalID.push_back(id); names[id++]=name;
            for(const auto* triangle:triangles) for(auto vertex:triangle->vertices) source.triVerts.push_back(vertex);
        }
        source.runIndex.push_back(uint32_t(source.triVerts.size()));
        source.Merge(); Manifold original(source);
        require(original.Status()==Manifold::Error::NoError,label+" input is not an oriented closed manifold; use closed solids and exclude leaves/cards (geometry status "+std::to_string(int(original.Status()))+")");
        auto shells=original.Decompose(); shellCount+=shells.size();
        require(shellCount<=node["max_shells"].get<size_t>(),label+" input shell budget exceeded");
        if(uniteShells) {
            for(const auto& shell:shells) require(shell.Volume()>0,"solidify needs outward closed shells with positive volume");
            return Manifold::BatchBoolean(shells,manifold::OpType::Add);
        }
        // Keep inward cavity surfaces when chaining cuts. Boolean operands are
        // solids already; raw overlapping outward shells belong in solidify.
        require(original.Volume()>0,"boolean operands need positive solid volume and outward exterior winding");
        return original;
    };
    Manifold united=prepare(mesh,tool==nullptr);
    if(tool) {
        auto cutter=prepare(*tool,false);
        united=united.Boolean(cutter,op=="union"?manifold::OpType::Add:op=="difference"?manifold::OpType::Subtract:manifold::OpType::Intersect);
    }
    // Snap to the actual export precision while topology is still available,
    // then let the solid kernel remove any triangles collapsed by rounding.
    united=united.Warp([](manifold::vec3& p) { for(int k=0;k<3;++k) p[k]=float(p[k]); }).Simplify();
    require(united.Status()==Manifold::Error::NoError,label+" "+op+" failed (geometry status "+std::to_string(int(united.Status()))+")");
    if(tool&&united.IsEmpty()) {
        if(diagnostics) (*diagnostics)["solid"]={{"operation",op},{"input_shells",shellCount},{"components",0},{"boundary_shells",0},{"volume",0},{"watertight",true},{"tolerance",tolerance},{"geometric_vertices",0},{"property_vertices",0}};
        return {};
    }
    auto components=united.Decompose(); size_t bodies=0;
    for(const auto& component:components) if(component.Volume()>0) ++bodies;
    require(!node["require_connected"].get<bool>()||bodies==1,label+" produced "+std::to_string(bodies)+" disconnected solids; connect the input geometry or set require_connected:false");
    double volume=united.Volume(); require(std::isfinite(volume)&&volume>0,"solidify produced no positive solid volume");
    auto smooth=united.CalculateNormals(0,node["crease_angle"].get<double>());
    size_t vertices=smooth.NumPropVert(),triangles=smooth.NumTri();
    budget.charge(vertices,triangles,0);
    auto result=smooth.GetMeshGL();
    require(result.NumVert()==vertices&&result.NumTri()==triangles&&result.numProp==14,"solidify returned unexpected geometry layout");
    Mesh out; out.vertices.reserve(vertices); out.triangles.reserve(triangles);
    for(size_t i=0;i<vertices;++i) {
        const auto* p=&result.vertProperties[i*14]; Vertex v;
        v.position={p[0],p[1],p[2]}; v.normal=normalized({p[3],p[4],p[5]}); v.uv={p[6],p[7]};
        for(int k=0;k<4;++k) v.color[k]=clamp01(p[8+k]); v.wind={p[12],p[13]}; out.vertices.push_back(v);
    }
    for(size_t run=0;run<result.runOriginalID.size();++run) {
        uint32_t id=result.runOriginalID[run]; require(names.count(id),label+" lost a source material");
        for(size_t i=result.runIndex[run];i<result.runIndex[run+1];i+=3) out.triangles.push_back({{result.triVerts[i],result.triVerts[i+1],result.triVerts[i+2]},names.at(id)});
    }
    require(out.triangles.size()==triangles,"solidify lost triangle material assignments");
    if(tool) {
        // Cutter properties can retain their original normal orientation on
        // reversed surfaces. Resolve the sign against the final face winding,
        // retaining the kernel's crease groups and interpolated attributes.
        std::vector<std::array<double,3>> facing(vertices);
        for(const auto& face:out.triangles) {
            Vec3 a=out.vertices[face.vertices[0]].position,b=out.vertices[face.vertices[1]].position,c=out.vertices[face.vertices[2]].position;
            double x=double(b.x)-a.x,y=double(b.y)-a.y,z=double(b.z)-a.z,u=double(c.x)-a.x,v=double(c.y)-a.y,w=double(c.z)-a.z;
            for(auto i:face.vertices) { facing[i][0]+=y*w-z*v; facing[i][1]+=z*u-x*w; facing[i][2]+=x*v-y*u; }
        }
        for(size_t i=0;i<vertices;++i) { auto& normal=out.vertices[i].normal; if(normal.x*facing[i][0]+normal.y*facing[i][1]+normal.z*facing[i][2]<0) normal=normal*-1; }
    }
    if(diagnostics) (*diagnostics)["solid"]={{"operation",op},{"input_shells",shellCount},{"components",bodies},{"boundary_shells",components.size()},{"volume",volume},{"watertight",true},{"tolerance",tolerance},{"geometric_vertices",united.NumVert()},{"property_vertices",vertices}};
    return out;
}
}
Mesh solidify(const Mesh& mesh, const Json& node, Budget& budget, Json* diagnostics) { return solidOperation(mesh,nullptr,node,budget,diagnostics); }
Mesh booleanMesh(const Mesh& input, const Mesh& tool, const Json& node, Budget& budget, Json* diagnostics) { return solidOperation(input,&tool,node,budget,diagnostics); }
}
