#include "foliage/foliage.hpp"
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>

using namespace foliage;
namespace {
int checks=0;
void expect(bool value, const std::string& message) { ++checks; if(!value) throw std::runtime_error(message); }
void fails(const std::function<void()>& fn, const std::string& message) { try { fn(); } catch(const std::exception& e) { expect(std::string(e.what()).find(message)!=std::string::npos,e.what()); return; } throw std::runtime_error("expected failure: "+message); }
Json document(Json nodes, const std::string& output="result") { return {{"nodes",nodes},{"outputs",{{"mineral.glb",output}}}}; }
Mesh mesh(const Json& doc, const Options& options={}) { return generate(parse(doc),options).outputs.begin()->second->mesh; }
double closed(const Mesh& mesh) {
    checkMesh(mesh);
    std::map<std::array<float,3>,uint32_t> positions; std::vector<uint32_t> ids;
    for(const auto& v:mesh.vertices) ids.push_back(positions.emplace(std::array<float,3>{v.position.x,v.position.y,v.position.z},uint32_t(positions.size())).first->second);
    std::map<std::pair<uint32_t,uint32_t>,std::pair<int,int>> edges; double volume=0;
    for(const auto& face:mesh.triangles) {
        auto a=mesh.vertices[face.vertices[0]],b=mesh.vertices[face.vertices[1]],c=mesh.vertices[face.vertices[2]];
        expect(dot(cross(b.position-a.position,c.position-a.position),a.normal+b.normal+c.normal)>-1e-6,"outward face normals: face="+std::to_string(face.vertices[0])+" material="+face.material+" dot="+std::to_string(dot(cross(b.position-a.position,c.position-a.position),a.normal+b.normal+c.normal)));
        volume+=(double(a.position.x)*(double(b.position.y)*c.position.z-double(b.position.z)*c.position.y)+double(a.position.y)*(double(b.position.z)*c.position.x-double(b.position.x)*c.position.z)+double(a.position.z)*(double(b.position.x)*c.position.y-double(b.position.y)*c.position.x))/6;
        for(int k=0;k<3;++k) { uint32_t x=ids[face.vertices[k]],y=ids[face.vertices[(k+1)%3]]; expect(x!=y,"distinct geometric corners"); auto& count=edges[{std::min(x,y),std::max(x,y)}]; ++count.first; count.second+=x<y?1:-1; }
    }
    for(const auto& edge:edges) expect(edge.second.first==2&&edge.second.second==0,"exactly two opposite faces per geometric edge");
    expect(volume>0,"positive signed volume"); return volume;
}
bool equal(const Mesh& a, const Mesh& b) {
    if(a.vertices.size()!=b.vertices.size()||a.triangles.size()!=b.triangles.size()) return false;
    for(size_t i=0;i<a.vertices.size();++i) { const auto& x=a.vertices[i]; const auto& y=b.vertices[i]; if(length(x.position-y.position)!=0||length(x.normal-y.normal)!=0||x.uv.x!=y.uv.x||x.uv.y!=y.uv.y||x.color!=y.color) return false; }
    for(size_t i=0;i<a.triangles.size();++i) if(a.triangles[i].vertices!=b.triangles[i].vertices||a.triangles[i].material!=b.triangles[i].material) return false;
    return true;
}
void rocks() {
    auto doc=document({{"result",{{"op","rock"}}}}); auto first=mesh(doc); closed(first);
    expect(first.triangles.size()==320&&first.vertices.size()==960,"rock subdivision counts"); expect(equal(first,mesh(doc)),"rock seed repeat");
    Options other; other.seed=123; expect(!equal(first,mesh(doc,other)),"rock root seed variation");
    doc["nodes"]["result"]["seed"]=77; expect(equal(mesh(doc),mesh(doc,other)),"rock explicit seed isolates root");
    auto plain=doc; plain["nodes"]["result"]["color_variation"]=0; auto a=mesh(doc),b=mesh(plain);
    for(size_t i=0;i<a.vertices.size();++i) expect(length(a.vertices[i].position-b.vertices[i].position)==0&&b.vertices[i].color==std::array<float,4>{1,1,1,1},"color variation does not change shape");
    for(int i=0;i<24;++i) {
        auto varied=document({{"raw",{{"op","rock"},{"seed",i},{"roundness",i%2?.1:1},{"noise",.7},{"strata",.4},{"strata_frequency",13},{"size",{2,.7,1.1}},{"subdivisions",i%4},{"shading",i%2?"flat":"smooth"}}},{"result",{{"op","solidify"},{"input","raw"},{"crease_angle",0}}}});
        auto source=varied; source["outputs"]["mineral.glb"]="raw";
        double original=closed(mesh(source)); expect(std::abs(closed(mesh(varied))-original)<1e-5,"weathered rock remains a valid closed Boolean solid");
    }
    for(auto op:{"rock","crystal"}) {
        auto limited=document({{"result",{{"op",op}}}}); Options options; options.limits.vertices=1;
        fails([&]{mesh(limited,options);},"vertex budget"); options={}; options.limits.triangles=1; fails([&]{mesh(limited,options);},"triangle budget");
    }
    auto lod=doc; lod["lods"]={{"Low",{{"overrides",{{"result",{{"subdivisions",1}}}}}}}};
    auto low=generate(selectLod(parse(lod),"Low")); expect(low.outputs.begin()->second->mesh.triangles.size()==80,"rock LOD override");
}
void crystals() {
    std::vector<Json> profiles={{{0,1},{1,1}},{{0,1},{.72,1},{1,0}},{{0,0},{.5,1},{1,0}},{{0,0},{.52,1},{.58,1},{.83,.75},{1,.48}},{{0,.7},{.2,1},{.9,1},{1,.4}},{{0,0},{1,1}},{{0,1},{1,0}}};
    for(int sides:{3,4,6,8,64}) for(const auto& profile:profiles) {
        auto doc=document({{"result",{{"op","crystal"},{"sides",sides},{"radius",.3},{"height",1.2},{"radius_profile",profile}}}});
        auto m=mesh(doc); double area=sides*.5*std::sin(2*pi/sides)*.3*.3,volume=0;
        for(size_t i=1;i<profile.size();++i) { double a=profile[i-1][1],b=profile[i][1]; volume+=area*(a*a+a*b+b*b)/3*(profile[i][0].get<double>()-profile[i-1][0].get<double>())*1.2; }
        expect(std::abs(closed(m)-volume)<1e-6,"profile frusta match analytic volume");
        auto stats=meshStats(m); expect(stats["bounds"]["min"][1]==0&&std::abs(stats["bounds"]["max"][1].get<float>()-1.2f)<1e-6,"crystal base pivot and height");
        for(const auto& face:m.triangles) { auto a=m.vertices[face.vertices[0]],b=m.vertices[face.vertices[1]],c=m.vertices[face.vertices[2]]; expect(length(a.normal-b.normal)==0&&length(a.normal-c.normal)==0,"flat crystal facets"); }
        for(const auto& v:m.vertices) expect(v.uv.x>=0&&v.uv.x<=1&&v.uv.y>=0&&v.uv.y<=1,"crystal UVs bounded");
        doc["nodes"]["result"]["radial_jitter"]=.35; auto varied=mesh(doc); closed(varied); expect(equal(varied,mesh(doc)),"crystal repeatable corner jitter"); Options other; other.seed=77; expect(!equal(varied,mesh(doc,other)),"crystal seed changes shape");
    }
    auto tiny=document({{"result",{{"op","crystal"},{"radius",.00001},{"height",.00001}}}}); closed(mesh(tiny));
    tiny["nodes"]["result"]["radius_profile"]={{0,.0001},{1,.0001}};
    auto needle=mesh(tiny); closed(needle);
    for(const auto& t:needle.triangles) if(std::all_of(t.vertices.begin(),t.vertices.end(),[&](auto i) { return needle.vertices[i].position.y==0; })) for(auto i:t.vertices) expect(needle.vertices[i].normal.y<-.999f,"tiny bottom cap retains outward normal");
    for(auto profile:std::vector<Json>{{{0,0},{1,0}},{{0,1},{.5,0},{1,1}},{{0,1},{.5,-1},{1,0}},{{0,1},{.5,.00001},{1,0}}}) {
        auto invalid=document({{"result",{{"op","rock"}}},{"unused",{{"op","crystal"},{"radius_profile",profile}}}}); fails([&]{parse(invalid);},"radius_profile");
    }
    auto invalid=document({{"result",{{"op","rock"},{"size",{1,0,1}}}}}); fails([&]{parse(invalid);},"outside allowed bounds");
}
Json cubes() {
    return document({{"cube",{{"op","crystal"},{"sides",4},{"radius",std::sqrt(2.0f)},{"height",2},{"radius_profile",{{0,1},{1,1}}},{"material","bark"}}},
        {"a",{{"op","transform"},{"input","cube"},{"rotation",{0,45,0}}}},
        {"shift",{{"op","transform"},{"input","a"},{"translation",{1,0,0}}}},
        {"b",{{"op","material"},{"input","shift"},{"name","cut"}}},
        {"result",{{"op","boolean"},{"input","a"},{"tool","b"}}}});
}
void booleans() {
    for(const auto& op:{"union","difference","intersection"}) {
        auto doc=cubes(); doc["nodes"]["result"]["operation"]=op; auto result=generate(parse(doc)); auto m=result.outputs.begin()->second->mesh;
        double expected=std::string(op)=="union"?12:4;
        expect(std::abs(closed(m)-expected)<1e-5,"Boolean analytic volume"); expect(equal(m,mesh(doc)),"Boolean repeatability");
        std::set<std::string> names; for(auto& t:m.triangles) names.insert(t.material); expect(names.count("bark")&&names.count("cut"),"both operands preserve material assignments");
        if(std::string(op)=="difference") {
            size_t cuts=0;
            for(const auto& t:m.triangles) if(std::all_of(t.vertices.begin(),t.vertices.end(),[&](auto i) { return std::abs(m.vertices[i].position.x)<1e-6; })) { ++cuts; expect(t.material=="cut","new cut surface inherits cutter material"); }
            expect(cuts>0,"subtraction exposes cutter faces");
        }
        for(const auto& v:m.vertices) expect(std::isfinite(v.uv.x)&&std::isfinite(v.uv.y),"Boolean UVs finite");
        Options limited; limited.limits.vertices=result.stats["generated_vertices"].get<size_t>()-1; fails([&]{mesh(doc,limited);},"vertex budget");
        limited={}; limited.limits.triangles=result.stats["generated_triangles"].get<size_t>()-1; fails([&]{mesh(doc,limited);},"triangle budget");
    }
    auto nested=cubes(); nested["nodes"]["shift"].update({{"translation",{0,.5,0}},{"scale",.5}});
    auto cavity=generate(parse(nested)); expect(std::abs(closed(cavity.outputs.begin()->second->mesh)-7)<1e-5,"enclosed cavity volume"); expect(cavity.stats["nodes"].back()["solid"]["components"]==1&&cavity.stats["nodes"].back()["solid"]["boundary_shells"]==2,"cavity is one body with two boundary shells");
    nested["nodes"]["again"]={{"op","boolean"},{"input","result"},{"tool","b"}}; nested["outputs"]["mineral.glb"]="again"; expect(std::abs(closed(mesh(nested))-7)<1e-5,"chained subtraction retains inward cavity surfaces");
    auto empty=cubes(); empty["nodes"]["result"]["tool"]="a"; fails([&]{mesh(empty);},"empty");
    empty["nodes"]["recovered"]={{"op","boolean"},{"operation","union"},{"input","result"},{"tool","a"}}; empty["outputs"]["mineral.glb"]="recovered"; expect(std::abs(closed(mesh(empty))-8)<1e-5,"empty intermediate union identity");
    auto apart=cubes(); apart["nodes"]["shift"]["translation"]={4,0,0}; apart["nodes"]["result"]["operation"]="intersection"; fails([&]{mesh(apart);},"empty");
    apart["nodes"]["result"]["operation"]="union"; fails([&]{mesh(apart);},"disconnected solids"); apart["nodes"]["result"]["require_connected"]=false; expect(std::abs(closed(mesh(apart))-16)<1e-5,"disconnected union explicitly allowed");
    for(auto key:{"input","tool"}) { auto open=cubes(); open["nodes"]["open"]={{"op","card"}}; open["nodes"]["result"][key]="open"; fails([&]{mesh(open);},"oriented closed manifold"); }
    auto budget=cubes(); budget["nodes"]["result"]["max_input_triangles"]=20; fails([&]{mesh(budget);},"input triangle budget");
    budget=cubes(); budget["nodes"]["result"]["max_shells"]=1; fails([&]{mesh(budget);},"shell budget");
    auto invalid=cubes(); invalid["nodes"]["unused"]={{"op","boolean"},{"input","a"},{"tool","b"},{"operation","xor"}}; fails([&]{parse(invalid);},"enum");
    invalid=cubes(); invalid["nodes"]["sites"]={{"op","scatter"}}; invalid["nodes"]["result"]["tool"]="sites"; fails([&]{parse(invalid);},"incompatible input type");
    expect(parse(parse(cubes()).document).document==parse(cubes()).document,"Boolean normalized recipe round trip");
}
}
int main() {
    try { rocks(); crystals(); booleans(); std::cout<<checks<<" mineral checks passed\n"; return 0; }
    catch(const std::exception& e) { std::cerr<<"FAILED after "<<checks<<" checks: "<<e.what()<<'\n'; return 1; }
}
