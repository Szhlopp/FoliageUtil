#include "internal.hpp"
#include "noise.hpp"
#include <map>

namespace foliage {
namespace {
using Face = std::array<uint32_t,3>;

Mesh rock(const Json& node, Random& random, Budget& budget) {
    int subdivisions=node["subdivisions"]; size_t triangles=20;
    for(int i=0;i<subdivisions;++i) triangles*=4;
    // Split export vertices carry flat normals and continuous UVs within each
    // triangle. Charge the full output before allocating refinement workspace.
    budget.charge(triangles*3,triangles,0);
    float golden=(1+std::sqrt(5.0f))*.5f;
    std::vector<Vec3> directions={{-1,golden,0},{1,golden,0},{-1,-golden,0},{1,-golden,0},{0,-1,golden},{0,1,golden},{0,-1,-golden},{0,1,-golden},{golden,0,-1},{golden,0,1},{-golden,0,-1},{-golden,0,1}};
    std::vector<Face> faces={{0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},{1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},{3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},{4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1}};
    directions.reserve(triangles/2+2);
    for(auto& p:directions) p=normalized(p);
    for(int level=0;level<subdivisions;++level) {
        std::map<std::pair<uint32_t,uint32_t>,uint32_t> midpoints;
        auto midpoint=[&](uint32_t a, uint32_t b) {
            auto key=std::make_pair(std::min(a,b),std::max(a,b)); auto found=midpoints.find(key);
            if(found!=midpoints.end()) return found->second;
            uint32_t id=uint32_t(directions.size()); directions.push_back(normalized(directions[a]+directions[b])); midpoints[key]=id; return id;
        };
        std::vector<Face> refined; refined.reserve(faces.size()*4);
        for(auto f:faces) { auto a=midpoint(f[0],f[1]),b=midpoint(f[1],f[2]),c=midpoint(f[2],f[0]); refined.push_back({f[0],a,c}); refined.push_back({f[1],b,a}); refined.push_back({f[2],c,b}); refined.push_back({a,b,c}); }
        faces=std::move(refined);
    }
    std::vector<Vertex> points(directions.size());
    Vec3 size=vector3(node["size"]); float exponent=2/node["roundness"].get<float>();
    uint64_t shapeSeed=random.state^hashName("rock.shape"),colorSeed=random.state^hashName("rock.color");
    Random layers{random.state^hashName("rock.strata")}; float phase=layers.unit()*2*pi;
    for(size_t i=0;i<points.size();++i) {
        Vec3 d=directions[i];
        float denominator=std::pow(std::pow(std::abs(d.x),exponent)+std::pow(std::abs(d.y),exponent)+std::pow(std::abs(d.z),exponent),1/exponent);
        Vec3 p=d/denominator;
        float weather=layeredNoise(p,node["noise_scale"],node["noise_octaves"],shapeSeed);
        float radius=(1+node["noise"].get<float>()*weather)*(1+node["strata"].get<float>()*std::sin(p.y*pi*node["strata_frequency"].get<float>()+phase));
        auto& v=points[i]; v.position={p.x*size.x*.5f*radius,p.y*size.y*.5f*radius,p.z*size.z*.5f*radius};
        v.normal={}; v.uv={std::atan2(d.z,d.x)/(2*pi)+.5f,std::asin(std::clamp(d.y,-1.0f,1.0f))/pi+.5f};
        float color=1-node["color_variation"].get<float>()*(.5f+.5f*layeredNoise(p,3,3,colorSeed)); v.color={color,color,color,1};
    }
    if(node["shading"]=="smooth") {
        for(auto f:faces) { Vec3 normal=cross(points[f[1]].position-points[f[0]].position,points[f[2]].position-points[f[0]].position); for(auto i:f) points[i].normal=points[i].normal+normal; }
        for(auto& v:points) v.normal=normalized(v.normal);
    }
    Mesh mesh; mesh.vertices.reserve(triangles*3); mesh.triangles.reserve(triangles);
    for(auto f:faces) {
        Vertex v[3]={points[f[0]],points[f[1]],points[f[2]]};
        Vec3 normal=normalized(cross(v[1].position-v[0].position,v[2].position-v[0].position));
        float low=std::min({v[0].uv.x,v[1].uv.x,v[2].uv.x}),high=std::max({v[0].uv.x,v[1].uv.x,v[2].uv.x});
        if(high-low>.5f) for(auto& p:v) if(p.uv.x<.5f) p.uv.x+=1;
        for(int k=0;k<3;++k) if(std::abs(directions[f[k]].y)>.999999f) v[k].uv.x=(v[(k+1)%3].uv.x+v[(k+2)%3].uv.x)*.5f;
        uint32_t first=uint32_t(mesh.vertices.size());
        for(auto& p:v) { if(node["shading"]=="flat"||dot(p.normal,normal)<=0) p.normal=normal; mesh.vertices.push_back(p); }
        mesh.triangles.push_back({{first,first+1,first+2},node["material"]});
    }
    return mesh;
}

Mesh crystal(const Json& node, Random& random, Budget& budget) {
    const auto& profile=node["radius_profile"]; int sides=node["sides"];
    size_t triangles=0;
    for(size_t i=1;i<profile.size();++i) triangles+=sides*(profile[i-1][1]==0||profile[i][1]==0?1:2);
    if(profile.front()[1]!=0) triangles+=sides;
    if(profile.back()[1]!=0) triangles+=sides;
    budget.charge(triangles*3,triangles,0);
    std::vector<float> radii(sides);
    for(auto& r:radii) r=node["radius"].get<float>()*(1+node["radial_jitter"].get<float>()*random.signedUnit());
    std::vector<std::vector<Vec3>> rings;
    float height=node["height"];
    for(const auto& key:profile) {
        std::vector<Vec3> ring; ring.reserve(sides);
        for(int k=0;k<sides;++k) { float a=2*pi*k/sides,r=radii[k]*key[1].get<float>(); ring.push_back({r*std::cos(a),height*key[0].get<float>(),r*std::sin(a)}); }
        rings.push_back(std::move(ring));
    }
    Mesh mesh; mesh.vertices.reserve(triangles*3); mesh.triangles.reserve(triangles);
    auto triangle=[&](Vec3 a, Vec3 b, Vec3 c, Vec2 ua, Vec2 ub, Vec2 uc) {
        Vec3 crossProduct=cross(b-a,c-a); require(length(crossProduct)>0,"crystal profile collapses at float precision; increase feature size or separate profile keys");
        // Unlike growth-path normalization, a small facet must not fall back
        // to +Y: even very small caps still need their actual outward normal.
        double norm=std::sqrt(double(crossProduct.x)*crossProduct.x+double(crossProduct.y)*crossProduct.y+double(crossProduct.z)*crossProduct.z);
        Vec3 normal=length(crossProduct)>1e-15f?normalized(crossProduct):Vec3{float(crossProduct.x/norm),float(crossProduct.y/norm),float(crossProduct.z/norm)}; uint32_t first=uint32_t(mesh.vertices.size());
        Vertex v; v.normal=normal; v.position=a; v.uv=ua; mesh.vertices.push_back(v); v.position=b; v.uv=ub; mesh.vertices.push_back(v); v.position=c; v.uv=uc; mesh.vertices.push_back(v);
        mesh.triangles.push_back({{first,first+1,first+2},node["material"]});
    };
    for(size_t row=1;row<rings.size();++row) for(int k=0;k<sides;++k) {
        int next=(k+1)%sides; float u=float(k)/sides,w=float(k+1)/sides,v=profile[row-1][0],t=profile[row][0];
        Vec3 a=rings[row-1][k],b=rings[row-1][next],c=rings[row][k],d=rings[row][next];
        if(profile[row-1][1]==0) triangle(a,c,d,{(u+w)*.5f,v},{u,t},{w,t});
        else if(profile[row][1]==0) triangle(a,c,b,{u,v},{(u+w)*.5f,t},{w,v});
        else { triangle(a,c,b,{u,v},{u,t},{w,v}); triangle(b,c,d,{w,v},{u,t},{w,t}); }
    }
    for(size_t row:{size_t(0),profile.size()-1}) if(profile[row][1]!=0) {
        float radius=node["radius"].get<float>()*profile[row][1].get<float>()*(1+node["radial_jitter"].get<float>());
        for(int k=0;k<sides;++k) {
            Vec3 a=rings[row][k],b=rings[row][(k+1)%sides]; if(row!=0) std::swap(a,b);
            triangle({0,a.y,0},a,b,{.5f,.5f},{.5f+a.x/(2*radius),.5f+a.z/(2*radius)},{.5f+b.x/(2*radius),.5f+b.z/(2*radius)});
        }
    }
    return mesh;
}
}
Mesh mineralMesh(const Json& node, Random& random, Budget& budget) { return node["op"]=="rock"?rock(node,random,budget):crystal(node,random,budget); }
}
