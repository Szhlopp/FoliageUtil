#include "foliage/foliage.hpp"
#include <fstream>
#include <cstring>
#include <functional>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>

using namespace foliage;
namespace {
int checks=0;
void expect(bool ok, const std::string& message) { ++checks; if(!ok) throw std::runtime_error(message); }
void fails(const std::function<void()>& fn, const std::string& part) { try { fn(); } catch(const std::exception& e) { expect(std::string(e.what()).find(part)!=std::string::npos,"unexpected failure: "+std::string(e.what())); return; } throw std::runtime_error("expected failure: "+part); }
Json doc(Json nodes, const std::string& output="mesh") { return {{"nodes",nodes},{"outputs",{{"test.glb",output}}}}; }
Json simple() { return doc({{"stem",{{"op","trunk"},{"noise",0},{"flare",0},{"taper",0},{"length",2},{"radius",0.2},{"segments",4}}},{"mesh",{{"op","tube"},{"input","stem"},{"sides",8}}}}); }
Mesh mesh(const Json& document, const Options& options={}) { auto result=generate(parse(document),options); return result.outputs.begin()->second->mesh; }
std::string bytes(const std::filesystem::path& p) { std::ifstream in(p,std::ios::binary); std::ostringstream s; s<<in.rdbuf(); return s.str(); }
uint32_t read32(const std::string& s, size_t at) { uint32_t v=0; for(int i=0;i<4;++i) v|=uint32_t(uint8_t(s.at(at+i)))<<(8*i); return v; }
Json glbJson(const std::string& file) { expect(read32(file,0)==0x46546c67&&read32(file,4)==2,"GLB header"); expect(read32(file,8)==file.size(),"GLB length"); expect(read32(file,16)==0x4e4f534a,"JSON chunk type"); return Json::parse(file.substr(20,read32(file,12))); }
void geometry() {
    auto m=mesh(simple()); expect(m.vertices.size()==63&&m.triangles.size()==80,"closed tube topology counts");
    double volume=0;
    for(const auto& face:m.triangles) {
        auto a=m.vertices[face.vertices[0]],b=m.vertices[face.vertices[1]],c=m.vertices[face.vertices[2]];
        Vec3 faceNormal=cross(b.position-a.position,c.position-a.position);
        expect(dot(faceNormal,a.normal+b.normal+c.normal)>0,"outward winding agrees with normals");
        volume+=dot(a.position,cross(b.position,c.position))/6;
    }
    expect(std::abs(volume-8*0.5*std::sin(2*pi/8)*.2*.2*2)<1e-5,"closed cylinder signed volume");
    auto cylinder=m;
    for(int row=0;row<5;++row) { auto& a=cylinder.vertices[row*9]; auto& b=cylinder.vertices[row*9+8]; expect(length(a.position-b.position)<1e-7&&length(a.normal-b.normal)<1e-7,"tube seam continuity"); }
    for(const auto& shape:{"oval","lanceolate","needle","petal"}) {
        auto leaf=mesh(doc({{"mesh",{{"op","leaf"},{"shape",shape},{"curl",0},{"fold",0}}}}));
        for(const auto& v:leaf.vertices) expect(v.normal.z>.99f,"leaf normals face +Z");
    }
    auto sphere=mesh(doc({{"mesh",{{"op","ellipsoid"},{"radii",{.5,1,.3}}}}}));
    for(const auto& face:sphere.triangles) { auto a=sphere.vertices[face.vertices[0]],b=sphere.vertices[face.vertices[1]],c=sphere.vertices[face.vertices[2]]; expect(dot(cross(b.position-a.position,c.position-a.position),a.normal+b.normal+c.normal)>0,"ellipsoid outward normals"); }
    auto card=mesh(doc({{"mesh",{{"op","card"},{"planes",3},{"uv_rect",{.25,.5,.5,1}}}}})); expect(card.triangles.size()==6,"crossed cards triangle budget");
    for(const auto& v:card.vertices) expect(v.uv.x>=.25f&&v.uv.x<=.5f&&v.uv.y>=.5f&&v.uv.y<=1,"atlas UV region");
    auto low=simple(); low["nodes"]["mesh"]["stride"]=3; expect(mesh(low).triangles.size()<m.triangles.size(),"tube structural LOD");
    auto growth=simple(); growth["nodes"]["grown"]={{"op","grow"},{"input","stem"},{"amount",.5},{"strength",0}}; growth["nodes"]["mesh"]["input"]="grown";
    expect(std::abs(meshStats(mesh(growth))["bounds"]["max"][1].get<float>()-1)<1e-5,"growth truncates arc length");
    growth["nodes"]["grown"]["amount"]=1; auto grown=mesh(growth); expect(grown.vertices.size()==m.vertices.size(),"growth identity count");
    for(size_t i=0;i<m.vertices.size();++i) expect(length(m.vertices[i].position-grown.vertices[i].position)<1e-6,"growth identity positions");
    auto pruned=simple(); pruned["nodes"]["prune"]={{"op","prune"},{"input","stem"},{"radius",.01},{"center",{100,0,0}}}; pruned["nodes"]["mesh"]["input"]="prune"; fails([&]{mesh(pruned);},"empty");
    auto transformed=doc({{"source",{{"op","ellipsoid"}}},{"mesh",{{"op","transform"},{"input","source"},{"translation",{1,2,3}},{"rotation",{23,41,-18}},{"scale",3}}}});
    checkMesh(mesh(transformed)); ++checks;
}
void growthProfiles() {
    auto d=simple(); d["nodes"]["stem"].update({{"origin",{2,5,-3}},{"direction",{1,0,0}},{"length",4},{"radius",.002},{"segments",32}});
    d["nodes"]["grown"]={{"op","grow"},{"input","stem"},{"direction",{0,-1,0}},{"strength",1},{"interpolation","spherical"},{"strength_profile",{{0,0},{.25,0},{.5,1},{1,1}}}};
    d["nodes"]["mesh"]["input"]="grown";
    auto center=[](const Mesh& m, int row) { Vec3 p{}; for(int k=0;k<8;++k) p=p+m.vertices[row*9+k].position/8; return p; };
    auto hanging=mesh(d),repeat=mesh(d); checkMesh(hanging); ++checks;
    expect(length(center(hanging,0)-Vec3{2,5,-3})<1e-6,"hanging stem retains attachment pivot");
    for(int row=1;row<=32;++row) {
        Vec3 delta=center(hanging,row)-center(hanging,row-1); float t=float(row)/32;
        float fraction=std::clamp((t-.25f)/.25f,0.0f,1.0f);
        Vec3 expected{std::cos(fraction*pi/2),-std::sin(fraction*pi/2),0};
        expect(std::abs(length(delta)-.125f)<2e-6,"hanging deformation preserves each segment length");
        expect(length(delta/.125f-expected)<2e-5,"bend profile keeps the shoulder straight then turns into a vertical tail");
        expect(length(center(hanging,row)-center(repeat,row))==0,"hanging deformation is repeatable");
    }
    auto child=d; child["nodes"]["child"]={{"op","branch"},{"input","grown"},{"count",1},{"range",{.5,.5}},{"segments",2},{"noise",0},{"length",.3}};
    child["nodes"]["mesh"]["input"]="child";
    expect(length(center(mesh(child),0)-center(hanging,16))<2e-6,"children attach to the deformed parent at arc length");
    auto partial=d; partial["nodes"]["grown"]["amount"]=.53;
    auto shortened=mesh(partial); float total=0;
    for(int row=1;row<=17;++row) total+=length(center(shortened,row)-center(shortened,row-1));
    expect(std::abs(total-4*.53f)<3e-6,"partial final segment retains requested arc length");
    expect(normalized(center(shortened,17)-center(shortened,16)).y<-.9999f,"profile spans the retained length after truncation");
    auto opposite=d; opposite["nodes"]["stem"]["direction"]={0,1,0}; opposite["nodes"]["grown"]["strength_profile"]={{0,0},{1,1}};
    auto turned=mesh(opposite); checkMesh(turned); ++checks;
    for(int row=1;row<=32;++row) {
        Vec3 delta=center(turned,row)-center(turned,row-1); float angle=pi*row/32;
        expect(length(delta/.125f-Vec3{std::sin(angle),std::cos(angle),0})<2e-5,"opposite directions turn smoothly without a collapsed segment or sudden reversal");
    }
    auto parallel=opposite; parallel["nodes"]["grown"]["direction"]={0,1,0}; checkMesh(mesh(parallel)); ++checks;
    for(const auto& mode:{"linear","spherical"}) {
        auto zero=d; zero["nodes"]["grown"]["interpolation"]=mode; zero["nodes"]["grown"]["strength_profile"]={{0,0},{1,0}};
        auto unchanged=mesh(zero); expect(length(center(unchanged,32)-Vec3{6,5,-3})<2e-6,"zero profile leaves source geometry unchanged");
    }
    auto legacy=d; legacy["nodes"]["grown"].erase("strength_profile"); legacy["nodes"]["grown"].erase("interpolation"); auto linear=mesh(legacy);
    for(int row=1;row<=32;++row) expect(length((center(linear,row)-center(linear,row-1))/.125f-normalized(Vec3{1-float(row)/32,-float(row)/32,0}))<2e-5,"default grow retains the original linear ramp");
    Options limited; limited.limits.points=65; fails([&]{mesh(d,limited);},"point budget");
    for(const Json& bad:std::vector<Json>{Json::array(),{{.1,0},{1,1}},{{0,0},{.5,1},{.4,0},{1,1}},{{0,0},{1,1.1}},{{0,-.1},{1,1}}}) {
        auto invalid=d; invalid["nodes"]["unused"]=invalid["nodes"]["grown"]; invalid["nodes"]["unused"]["strength_profile"]=bad;
        fails([&]{parse(invalid);},"strength_profile");
    }
    auto invalid=d; invalid["nodes"]["grown"]["interpolation"]="other"; fails([&]{parse(invalid);},"interpolation");
    expect(parse(parse(d).document).document==parse(d).document,"growth profile normalized graph reparses");
}
void solidMeshes() {
    auto d=doc({
        {"stem",{{"op","trunk"},{"radius",std::sqrt(2.0f)},{"length",2},{"segments",1},{"noise",0},{"flare",0},{"taper",0}}},
        {"tube",{{"op","tube"},{"input","stem"},{"sides",4}}},
        {"a",{{"op","transform"},{"input","tube"},{"rotation",{0,45,0}}}},
        {"b",{{"op","transform"},{"input","a"},{"translation",{1,0,0}}}},
        {"second",{{"op","material"},{"input","b"},{"name","leaf"}}},
        {"joined",{{"op","merge"},{"inputs",{"a","second"}}}},
        {"weighted",{{"op","wind"},{"input","joined"},{"height",2},{"strength",.6},{"exponent",1}}},
        {"mesh",{{"op","solidify"},{"input","weighted"}}}});
    auto watertight=[&](const Mesh& m, double expectedVolume) {
        std::map<std::array<float,3>,uint32_t> positions; std::vector<uint32_t> ids;
        for(const auto& v:m.vertices) { std::array<float,3> p{v.position.x,v.position.y,v.position.z}; auto it=positions.emplace(p,uint32_t(positions.size())).first; ids.push_back(it->second); }
        std::map<std::pair<uint32_t,uint32_t>,std::pair<int,int>> edges; double volume=0;
        for(const auto& face:m.triangles) {
            auto a=m.vertices[face.vertices[0]].position,b=m.vertices[face.vertices[1]].position,c=m.vertices[face.vertices[2]].position;
            volume+=double(dot(a,cross(b,c)))/6;
            for(int k=0;k<3;++k) { uint32_t x=ids[face.vertices[k]],y=ids[face.vertices[(k+1)%3]]; expect(x!=y,"solid triangle has distinct geometric vertices"); auto& edge=edges[{std::min(x,y),std::max(x,y)}]; ++edge.first; edge.second+=x<y?1:-1; }
        }
        for(const auto& [edge,uses]:edges) expect(uses.first==2&&uses.second==0,"solid has exactly two opposite faces per geometric edge");
        expect(std::abs(volume-expectedVolume)<1e-4,"boolean union matches analytic volume, without internal shells");
    };
    auto result=generate(parse(d)); const auto& m=result.outputs.begin()->second->mesh; watertight(m,12);
    auto again=mesh(d); expect(m.vertices.size()==again.vertices.size()&&m.triangles.size()==again.triangles.size(),"boolean union repeat counts");
    std::set<std::string> materials;
    for(size_t i=0;i<m.vertices.size();++i) {
        const auto& v=m.vertices[i];
        expect(length(v.position-again.vertices[i].position)==0&&length(v.normal-again.vertices[i].normal)==0&&v.uv.x==again.vertices[i].uv.x&&v.uv.y==again.vertices[i].uv.y,"boolean union deterministic properties");
        expect(std::abs(v.wind.x-.3f*v.position.y)<1e-6,"boolean union interpolates existing wind weights");
        expect(v.uv.x>=-1e-6&&v.uv.x<=1.000001f&&v.uv.y>=-1e-6&&v.uv.y<=2.000001f,"boolean union retains valid source UV bounds");
        expect(v.color==std::array<float,4>{1,1,1,1},"boolean union preserves vertex color");
    }
    for(const auto& face:m.triangles) materials.insert(face.material);
    expect(materials.count("bark")&&materials.count("cut")&&materials.count("leaf"),"boolean union preserves surviving material slots");
    const auto& report=result.stats["nodes"].back()["solid"];
    expect(report["input_shells"]==2&&report["components"]==1&&report["watertight"]==true,"solid diagnostics report shell union");
    expect(std::abs(report["volume"].get<double>()-12)<1e-4,"solid diagnostics volume");
    auto duplicate=d; duplicate["nodes"]["b"]["translation"]={0,0,0}; watertight(mesh(duplicate),8);
    auto apart=d; apart["nodes"]["b"]["translation"]={4,0,0}; fails([&]{mesh(apart);},"disconnected solids");
    apart["nodes"]["mesh"]["require_connected"]=false; auto separate=generate(parse(apart)); watertight(separate.outputs.begin()->second->mesh,16); expect(separate.stats["nodes"].back()["solid"]["components"]==2,"disconnected solid option reports multiple bodies");
    auto nested=d; nested["nodes"]["b"]["translation"]={0,.5,0}; nested["nodes"]["b"]["scale"]=.5; watertight(mesh(nested),8);
    auto open=d; open["nodes"]["tube"]["caps"]=false; fails([&]{mesh(open);},"oriented closed manifold");
    open=d; open["nodes"]["mesh"]["input"]="open"; open["nodes"]["open"]={{"op","card"}}; fails([&]{mesh(open);},"oriented closed manifold");
    auto limited=d; limited["nodes"]["mesh"]["max_input_triangles"]=4; fails([&]{mesh(limited);},"input triangle budget");
    limited=d; limited["nodes"]["mesh"]["max_shells"]=1; fails([&]{mesh(limited);},"shell budget");
    Options budget; budget.limits.vertices=result.stats["generated_vertices"].get<size_t>()-1; fails([&]{mesh(d,budget);},"vertex budget");
    budget={}; budget.limits.triangles=result.stats["generated_triangles"].get<size_t>()-1; fails([&]{mesh(d,budget);},"triangle budget");
    auto invalid=d; invalid["nodes"]["mesh"]["input"]="stem"; fails([&]{parse(invalid);},"incompatible input type");
    invalid=d; invalid["nodes"]["unused"]={{"op","solidify"},{"input","joined"},{"weld_tolerance",-.1}}; fails([&]{parse(invalid);},"outside allowed bounds");
    expect(parse(parse(d).document).document==parse(d).document,"solidify normalized graph reparses");
}
void rootsAndProfiles() {
    auto d=simple(); d["nodes"]["stem"]["segments"]=16; d["nodes"]["mesh"]["sides"]=16;
    auto original=mesh(d); d["nodes"]["mesh"]["radius_noise"]=.35;
    d["nodes"]["mesh"]["noise_profile"]={{0,0},{.2,1},{.8,1},{1,0}};
    auto surface=mesh(d),repeat=mesh(d); bool changed=false;
    for(size_t i=0;i<surface.vertices.size();++i) {
        expect(length(surface.vertices[i].position-repeat.vertices[i].position)==0,"surface noise repeats with seed");
        if(i<17*17) {
            float radius=std::hypot(surface.vertices[i].position.x,surface.vertices[i].position.z);
            expect(radius>=.2f*(1-.35f)-1e-6&&radius<=.2f*(1+.35f)+1e-6,"surface noise respects radial amplitude bound");
            if(i<17||i>=16*17) expect(length(surface.vertices[i].position-original.vertices[i].position)==0,"zero profile pins both end rings");
            changed|=length(surface.vertices[i].position-original.vertices[i].position)>1e-5;
        }
    }
    expect(changed,"surface noise changes interior rings");
    for(int row=0;row<=16;++row) expect(length(surface.vertices[row*17].normal-surface.vertices[row*17+16].normal)<1e-7&&length(surface.vertices[row*17].position-surface.vertices[row*17+16].position)<1e-7,"noisy tube seam is continuous");
    auto low=d; low["nodes"]["mesh"]["stride"]=4; auto lod=mesh(low);
    for(int row=0;row<=4;++row) for(int k=0;k<=16;++k) expect(length(lod.vertices[row*17+k].position-surface.vertices[row*4*17+k].position)<1e-6,"surface noise sampled in space survives straight-path LOD");
    Options other; other.seed=43; auto variant=mesh(d,other); changed=false;
    for(size_t i=0;i<surface.vertices.size();++i) changed|=length(surface.vertices[i].position-variant.vertices[i].position)>1e-6;
    expect(changed,"surface noise responds to root seed");
    d["nodes"]["mesh"]["seed"]=12; auto fixed=mesh(d),fixedOther=mesh(d,other);
    for(size_t i=0;i<fixed.vertices.size();++i) expect(length(fixed.vertices[i].position-fixedOther.vertices[i].position)==0,"explicit tube seed isolates radial noise");
    d=simple(); d["nodes"]["stem"]["noise"]=.5; d["nodes"]["stem"]["noise_profile"]={{0,0},{1,0}};
    auto quiet=mesh(d),baseline=mesh(simple());
    for(size_t i=0;i<quiet.vertices.size();++i) expect(length(quiet.vertices[i].position-baseline.vertices[i].position)==0,"zero directional profile disables perturbations");
    d["nodes"]["stem"]["radius_profile"]={{0,2},{.5,1.5},{1,1}};
    auto radii=mesh(d); expect(std::abs(radii.vertices[0].position.x-.4f)<1e-6&&std::abs(radii.vertices[2*9].position.x-.3f)<1e-6,"radius profile interpolates known radii");
    d=simple(); d["nodes"]["stem"]["flare"]=1; d["nodes"]["stem"]["flare_length"]=.5;
    auto flare=mesh(d); expect(std::abs(flare.vertices[9].position.x-.25f)<1e-6&&std::abs(flare.vertices[18].position.x-.2f)<1e-6,"flare length controls fade extent");
    d=simple(); d["nodes"]["mesh"]["ridges"]=4; d["nodes"]["mesh"]["ridge_depth"]=.4; d["nodes"]["mesh"]["ridge_profile"]={{0,1},{.5,0},{1,0}};
    auto ridged=mesh(d); expect(std::abs(ridged.vertices[0].position.x-.28f)<1e-6&&std::abs(ridged.vertices[18].position.x-.2f)<1e-6,"basal ridge profile fades out");
    d=simple(); d["nodes"]["stem"].update({{"origin",{2,1,-3}},{"length",4},{"radius",1},{"segments",8}});
    d["nodes"]["roots"]={{"op","roots"},{"input","stem"},{"count",4},{"attachment",.25},{"length",3},{"length_variation",0},{"angle_jitter",0},{"ground_height",.4},{"surface_at",.5},{"bury_depth",.3},{"radius_scale",.2},{"radius_variation",0},{"taper",.9},{"noise",0},{"segments",8}};
    d["nodes"]["mesh"].update({{"input","roots"},{"sides",16}}); auto roots=mesh(d);
    const int perRoot=9*17+2*17;
    expect(roots.vertices.size()==4*perRoot&&roots.triangles.size()==4*(8*16*2+32),"root replication topology counts");
    for(int r=0;r<4;++r) for(int row=0;row<=8;++row) {
        Vec3 center{}; for(int k=0;k<16;++k) center=center+roots.vertices[r*perRoot+row*17+k].position/16;
        float t=float(row)/8,u=std::min(1.0f,t/.5f),angle=2*pi*r/4;
        Vec3 expected{2+3*t*std::cos(angle),.4f+1.6f*(1-u)*(1-u)-.3f*t*t,-3+3*t*std::sin(angle)};
        expect(length(center-expected)<2e-6,"roots attach to parent and follow horizontal reach and ground descent");
        if(row==8) for(int k=0;k<16;++k) expect(roots.vertices[r*perRoot+row*17+k].position.y<.4f,"root tip ring is fully below ground");
    }
    for(const auto& face:roots.triangles) { auto a=roots.vertices[face.vertices[0]],b=roots.vertices[face.vertices[1]],c=roots.vertices[face.vertices[2]]; expect(dot(cross(b.position-a.position,c.position-a.position),a.normal+b.normal+c.normal)>0,"root winding agrees with normals"); }
    d["nodes"]["roots"]["noise"]=.2; d["nodes"]["roots"]["seed"]=18;
    auto noisy=mesh(d),again=mesh(d,other); changed=false;
    for(size_t i=0;i<noisy.vertices.size();++i) { expect(length(noisy.vertices[i].position-again.vertices[i].position)==0,"explicit roots seed repeats independently"); changed|=length(noisy.vertices[i].position-roots.vertices[i].position)>1e-5; }
    expect(changed,"root meander changes geometry");
    Options tight; tight.limits.points=44; fails([&]{mesh(d,tight);},"point budget");
    auto empty=d; empty["nodes"]["roots"]["count"]=0; fails([&]{mesh(empty);},"empty");
    auto invalid=d; invalid["nodes"]["roots"]["input"]="mesh"; fails([&]{parse(invalid);},"cycle");
    invalid=d; invalid["nodes"]["roots"]["input"]="wrong"; invalid["nodes"]["wrong"]={{"op","leaf"}}; fails([&]{parse(invalid);},"incompatible input type");
    invalid=d; invalid["nodes"]["roots"]["surface_at"]=0; fails([&]{parse(invalid);},"bounds");
    for(const Json& bad:std::vector<Json>{Json::array(),{{0,1},{0,0}},{{.1,1},{1,0}},{{0,1},{.5,1},{.4,0},{1,0}},{{0,1},{1,2}},{{0,1},{.5,1},{.500000001,0},{1,0}},{{0,1},{1,"x"}}}) {
        invalid=simple(); invalid["nodes"]["unused"]={{"op","trunk"},{"noise_profile",bad}};
        fails([&]{parse(invalid);},"noise_profile");
    }
    expect(parse(parse(d).document).document==parse(d).document,"profiles and roots survive normalized graph reparse");
}
void validation() {
    auto d=simple(); d["nodes"]["stem"]["lenght"]=3; fails([&]{parse(d);},"unknown parameter");
    d=simple(); d["nodes"]["unused"]={{"op","wat"}}; fails([&]{parse(d);},"unknown op");
    d=simple(); d["nodes"]["mesh"]["input"]="missing"; fails([&]{parse(d);},"unknown node reference");
    d=simple(); d["nodes"]["stem"]={{"op","grow"},{"input","stem"}}; fails([&]{parse(d);},"cycle");
    d=simple(); d["nodes"]["stem"]={{"op","leaf"}}; fails([&]{parse(d);},"incompatible input type");
    d=simple(); d["nodes"]["stem"]["direction"]={0,0,0}; fails([&]{parse(d);},"nonzero");
    d=simple(); d["nodes"]["stem"]["segments"]=2.5; fails([&]{parse(d);},"integer");
    d=simple(); d["seed"]=2147483648ll; fails([&]{parse(d);},"32-bit");
    d=simple(); d["nodes"]["stem"]["seed"]=18446744073709551615ull; fails([&]{parse(d);},"bounds");
    d=simple(); d["nodes"]["mesh"]["material"]="undefined"; fails([&]{parse(d);},"undefined material");
    d=simple(); d["outputs"]={{"../escape.obj","mesh"}}; fails([&]{parse(d);},"..");
    d=simple(); d["outputs"]={{"/absolute.glb","mesh"}}; fails([&]{parse(d);},"relative");
    d=simple(); d["outputs"]={{"test.obj","stem"}}; fails([&]{parse(d);},"mesh node");
    d=doc({{"mesh",{{"op","card"},{"uv_rect",{1,0,0,1}}}}}); fails([&]{parse(d);},"positive width");
    d=doc({{"mesh",{{"op","ellipsoid"},{"radii",{1,0,1}}}}}); fails([&]{parse(d);},"positive");
    d=simple(); Options options; options.limits.vertices=10; fails([&]{mesh(d,options);},"vertex budget");
    options={}; options.limits.triangles=10; fails([&]{mesh(d,options);},"triangle budget");
    options={}; options.limits.points=2; fails([&]{mesh(d,options);},"point budget");
    d=doc({{"prototype",{{"op","ellipsoid"}}},{"points",{{"op","scatter"},{"count",100000}}},{"mesh",{{"op","instance"},{"input","prototype"},{"points","points"}}}}); fails([&]{mesh(d);},"vertex budget");
    d=simple(); d["nodes"]["unused"]={{"op","scatter"},{"count",100000}}; options={}; options.limits.points=10; auto result=generate(parse(d),options); expect(result.stats["nodes"].size()==2,"unused nodes validated but not evaluated");
    d=simple(); d["outputs"]["duplicate.glb"]="mesh"; result=generate(parse(d)); expect(result.outputs.at("duplicate.glb")==result.outputs.at("test.glb"),"shared outputs evaluate once");
}
void scattering() {
    auto d=doc({{"prototype",{{"op","card"},{"width",.001},{"height",.001}}},{"spiral",{{"op","radial"},{"mode","spiral"},{"count",12},{"radius",{1,2}},{"height",{0,3}},{"jitter",0}}},{"mesh",{{"op","instance"},{"input","prototype"},{"points","spiral"},{"color_variation",0}}}});
    auto m=mesh(d); expect(m.vertices.size()==48,"spiral instance count");
    expect(std::abs(m.vertices.front().position.x-1)<.001f&&std::abs(m.vertices[44].position.y-3)<.001f,"spiral radius and height progression");
    d=doc({{"surface",{{"op","card"},{"width",2},{"height",4}}},{"points",{{"op","scatter"},{"input","surface"},{"count",1000},{"angle",0},{"offset",.1}}},{"prototype",{{"op","card"},{"width",.00001},{"height",.00001}}},{"mesh",{{"op","instance"},{"input","prototype"},{"points","points"}}}});
    m=mesh(d); double x=0,y=0;
    for(size_t i=0;i<m.vertices.size();i+=4) { auto p=m.vertices[i].position; x+=p.x;y+=p.y;expect(std::abs(p.z-.1)<.0001,"mesh scatter follows surface normal"); }
    expect(std::abs(x/1000)<.08&&std::abs(y/1000-2)<.15,"mesh scatter roughly uniform across triangle area");
    d=simple(); d["nodes"]["wind"]={{"op","wind"},{"input","mesh"},{"base",0},{"height",2},{"exponent",1}}; d["outputs"]["test.glb"]="wind"; m=mesh(d);
    for(const auto& v:m.vertices) expect(std::abs(v.wind.x-v.position.y/2)<1e-6,"wind height weighting");
    d["nodes"]["merge"]={{"op","merge"},{"inputs",{"wind","wind"}}}; d["outputs"]["test.glb"]="merge"; auto merged=mesh(d);
    for(size_t i=0;i<m.vertices.size();++i) expect(m.vertices[i].wind.y==merged.vertices[i].wind.y,"merge preserves wind phase");
}
void proximityScattering() {
    auto d=doc({{"plane",{{"op","card"},{"width",2},{"height",2},{"pivot","center"}}},
        {"support",{{"op","transform"},{"input","plane"},{"rotation",{-90,0,0}}}},
        {"sites",{{"op","scatter"},{"count",400},{"radius",2},{"offset",.1}}},
        {"prototype",{{"op","card"},{"width",.001},{"height",.002}}},
        {"copies",{{"op","instance"},{"input","prototype"},{"points","sites"},{"color_variation",0}}},
        {"mesh",{{"op","merge"},{"inputs",{"copies","support"}}}}});
    // Allow float rotation roundoff when comparing against the analytic ground plane.
    auto baseline=mesh(d);
    d["nodes"]["sites"]["proximity_mesh"]="support";
    d["nodes"]["sites"]["max_distance"]=.15;
    auto filtered=mesh(d),repeat=mesh(d);
    auto pivot=[](const Mesh& m,size_t i) { return (m.vertices[i*4].position+m.vertices[i*4+1].position)*.5f; };
    size_t accepted=0;
    for(size_t i=0;i<400;++i) {
        auto p=pivot(baseline,i); Vec3 q{std::clamp(p.x,-1.0f,1.0f),0,std::clamp(p.z,-1.0f,1.0f)};
        if(length(p-q)>.15f) continue;
        for(size_t k=0;k<4;++k) {
            expect(length(baseline.vertices[i*4+k].position-filtered.vertices[accepted*4+k].position)<1e-7,"proximity filtering retains candidate positions, scale and ordering");
            expect(length(filtered.vertices[accepted*4+k].position-repeat.vertices[accepted*4+k].position)==0,"proximity filtering repeats with the seed");
        }
        ++accepted;
    }
    expect(accepted>0&&accepted<400&&filtered.vertices.size()==accepted*4+4,"triangle surface distance filters ground candidates, including edges");
    d["nodes"]["sites"]["snap_to_mesh"]=true; auto snapped=mesh(d);
    expect(snapped.vertices.size()==filtered.vertices.size(),"snapping never adds candidates beyond the distance limit");
    for(size_t i=0;i<accepted;++i) {
        auto p=pivot(filtered,i),q=pivot(snapped,i); Vec3 nearest{std::clamp(p.x,-1.0f,1.0f),0,std::clamp(p.z,-1.0f,1.0f)};
        expect(length(q-nearest)<2e-7,"snap lands at the nearest face, edge or corner rather than the nearest vertex");
        expect(length(snapped.vertices[i*4].normal-filtered.vertices[i*4].normal)==0,"snap preserves orientation");
        expect(snapped.vertices[i*4].wind.y==filtered.vertices[i*4].wind.y,"snap preserves phase");
    }
    auto generated=generate(parse(d)); bool diagnostics=false;
    for(const auto& s:generated.stats["nodes"]) if(s["node"]=="sites") {
        diagnostics=true; expect(s["proximity"]["candidates"]==400&&s["proximity"]["accepted"]==accepted&&s["proximity"]["rejected"]==400-accepted&&s["proximity"]["snapped"]==accepted,"scatter reports accepted, rejected and snapped counts");
    }
    expect(diagnostics,"proximity diagnostics present");
    auto bad=d; bad["nodes"]["sites"]["max_distance"]=-.01; fails([&]{parse(bad);},"bounds");
    bad=d; bad["nodes"]["sites"].erase("proximity_mesh"); fails([&]{parse(bad);},"require proximity_mesh");
    bad=d; bad["nodes"]["sites"]["proximity_mesh"]="sites"; fails([&]{parse(bad);},"cycle");
    bad=d; bad["nodes"]["wrong"]={{"op","trunk"}}; bad["nodes"]["sites"]["proximity_mesh"]="wrong"; fails([&]{parse(bad);},"incompatible input type");
    bad=d; bad["nodes"]["unused"]={{"op","scatter"},{"max_distance",1}}; fails([&]{parse(bad);},"require proximity_mesh");
    d["nodes"]["sites"]["max_distance"]=0; expect(mesh(d).vertices.size()==4,"rejected candidates are not retried or moved across the distance limit");
    Options small; small.limits.points=399; fails([&]{mesh(d,small);},"point budget");
    d["nodes"]["sites"]["count"]=0; expect(mesh(d).vertices.size()==4,"zero proximity candidates are safe");
    auto empty=d; empty["nodes"]["stem"]={{"op","trunk"}}; empty["nodes"]["no_branches"]={{"op","branch"},{"input","stem"},{"count",0}};
    empty["nodes"]["support"]={{"op","tube"},{"input","no_branches"}}; empty["nodes"]["sites"]["count"]=8; empty["nodes"]["mesh"]["inputs"]={"copies","plane"};
    expect(mesh(empty).vertices.size()==4,"empty supporting mesh rejects all candidates without a query failure");
    // A source mesh's own surface can be queried at interior points. This catches
    // nearest-vertex shortcuts and verifies offset is applied before filtering.
    d["nodes"]["sites"]={{"op","scatter"},{"input","plane"},{"count",32},{"angle",0},{"offset",0},{"proximity_mesh","plane"},{"max_distance",0}};
    expect(mesh(d).vertices.size()==32*4+4,"zero-distance candidates on triangle interiors survive");
    d["nodes"]["sites"]["offset"]=.2; d["nodes"]["sites"]["max_distance"]=.1;
    expect(mesh(d).vertices.size()==4,"mesh scatter proximity checks the final offset position");
    // Exercise the hierarchy with many separated triangle planes, then compare
    // every accepted placement against an independent rectangle-distance oracle.
    Json supports=Json::array();
    for(int i=0;i<12;++i) { auto name="support_"+std::to_string(i); d["nodes"][name]={{"op","transform"},{"input","plane"},{"scale",.22},{"rotation",{-90,0,0}},{"translation",{(i%4-1.5)*.7,0,(i/4-1)*.7}}}; supports.push_back(name); }
    d["nodes"]["support"]={{"op","merge"},{"inputs",supports}};
    d["nodes"]["sites"]={{"op","scatter"},{"count",400},{"radius",2},{"offset",.1},{"proximity_mesh","support"},{"max_distance",.15},{"snap_to_mesh",true}};
    auto many=mesh(d); accepted=0;
    for(size_t i=0;i<400;++i) {
        auto p=pivot(baseline,i); Vec3 nearest{}; float best=1e10f;
        for(int k=0;k<12;++k) { float x=float((k%4-1.5)*.7),z=float((k/4-1)*.7); Vec3 q{std::clamp(p.x,x-.22f,x+.22f),0,std::clamp(p.z,z-.22f,z+.22f)}; float distance=length(p-q); if(distance<best) { best=distance; nearest=q; } }
        if(best>.15f) continue;
        expect(length(pivot(many,accepted)-nearest)<3e-7,"triangle hierarchy agrees with independent surface-distance oracle"); ++accepted;
    }
    expect(many.vertices.size()==accepted*4+12*4,"hierarchy preserves all and only nearby candidates");
}
void orientations() {
    auto d=doc({{"sites",{{"op","radial"},{"count",8},{"radius",{2,2}},{"height",{.5,1.5}},{"jitter",0}}},
        {"facing",{{"op","orient"},{"input","sites"},{"center",{0,0,0}},{"horizontal",true}}},
        {"card",{{"op","card"},{"width",.1},{"height",.2}}},
        {"mesh",{{"op","instance"},{"input","card"},{"points","facing"},{"color_variation",0}}}});
    auto exact=mesh(d);
    for(size_t i=0;i<exact.vertices.size();i+=4) {
        auto a=exact.vertices[i],b=exact.vertices[i+1],c=exact.vertices[i+2]; Vec3 pivot=(a.position+b.position)*.5f; pivot.y=0;
        expect(dot(a.normal,normalized(pivot))>.99999f,"outward card normal points away from pivot");
        expect(dot(normalized(c.position-b.position),Vec3{0,1,0})>.99999f,"horizontal-facing cards stay upright");
    }
    d["nodes"]["facing"]["mode"]="inward";
    auto inward=mesh(d); for(size_t i=0;i<exact.vertices.size();++i) expect(dot(exact.vertices[i].normal,inward.vertices[i].normal)<-.99999f,"inward reverses card face");
    d["nodes"]["facing"]["mode"]="fixed"; d["nodes"]["facing"]["direction"]={1,0,0}; auto fixed=mesh(d);
    for(const auto& v:fixed.vertices) expect(dot(v.normal,{1,0,0})>.99999f,"fixed card normals");
    d["nodes"]["facing"]["mode"]="outward"; d["nodes"]["facing"]["rotation_jitter"]={0,25,0}; auto varied=mesh(d); auto repeat=mesh(d);
    bool changed=false;
    for(size_t i=0;i<exact.vertices.size();++i) {
        float agreement=dot(exact.vertices[i].normal,varied.vertices[i].normal);
        expect(agreement>=std::cos(radians(25))-.00001f,"orientation jitter stays within specified yaw");
        expect(length(varied.vertices[i].normal-repeat.vertices[i].normal)==0,"orientation jitter is seeded");
        changed=changed||agreement<.999f;
    }
    expect(changed,"nonzero orientation jitter changes facing");
    d["nodes"]["facing"]["rotation_jitter"]={0,0,0}; d["nodes"]["sites"]["radius"]={0,0}; d["nodes"]["facing"]["direction"]={0,1,0}; checkMesh(mesh(d)); ++checks;
    d["nodes"]["facing"]["up"]={0,0,0}; fails([&]{parse(d);},"up must be nonzero");
    d["nodes"]["facing"]["up"]={0,1,0}; d["nodes"]["facing"]["rotation_jitter"]={-1,0,0}; fails([&]{parse(d);},"bounds");
}
void cardVariation() {
    auto d=doc({{"mesh",{{"op","card"},{"planes",3},{"segments",8},{"width_segments",4},{"uv_rect",{.2,.3,.8,.9}}}}});
    auto flat=mesh(d); expect(flat.vertices.size()==135&&flat.triangles.size()==192,"segmented crossed card counts");
    d["nodes"]["mesh"]["curl"]=-.2; d["nodes"]["mesh"]["fold"]=-.3; d["nodes"]["mesh"]["twist"]=20;
    auto curved=mesh(d); checkMesh(curved); ++checks;
    bool bent=false;
    for(size_t i=0;i<curved.vertices.size();++i) {
        const auto& v=curved.vertices[i];
        expect(v.uv.x==flat.vertices[i].uv.x&&v.uv.y==flat.vertices[i].uv.y,"card deformation preserves atlas UVs");
        expect(std::abs(length(v.normal)-1)<1e-5,"curved card normals are normalized");
        bent=bent||length(v.position-flat.vertices[i].position)>.001f;
    }
    expect(bent,"curved cards change actual geometry");
    for(const auto& face:curved.triangles) {
        const auto& a=curved.vertices[face.vertices[0]]; const auto& b=curved.vertices[face.vertices[1]]; const auto& c=curved.vertices[face.vertices[2]];
        expect(dot(cross(b.position-a.position,c.position-a.position),a.normal+b.normal+c.normal)>0,"curved crossed-card winding agrees with normals");
    }
    for(int plane=0;plane<3;++plane) expect(length(curved.vertices[plane*45+2].position)<1e-7,"base card attachment stays at origin");
    d["nodes"]["mesh"]["pivot"]="center"; auto centered=mesh(d);
    for(int plane=0;plane<3;++plane) expect(length(centered.vertices[plane*45+22].position)<1e-7,"center card pivot stays at origin");
    Options limited; limited.limits.vertices=134; fails([&]{mesh(d,limited);},"vertex budget");
    limited={}; limited.limits.triangles=191; fails([&]{mesh(d,limited);},"triangle budget");
    d=doc({{"mesh",{{"op","card"},{"curl",.1}}}}); fails([&]{parse(d);},"segments >= 2");
    d=doc({{"mesh",{{"op","card"},{"fold",.1}}}}); fails([&]{parse(d);},"width_segments >= 2");
    d=doc({{"mesh",{{"op","card"},{"segments",129}}}}); fails([&]{parse(d);},"bounds");
    d=doc({{"sites",{{"op","radial"},{"count",16},{"radius",{2,2}},{"height",{0,0}},{"jitter",0}}},
        {"facing",{{"op","orient"},{"input","sites"},{"mode","fixed"},{"direction",{0,0,1}}}},
        {"card",{{"op","card"},{"width",2},{"height",4}}},
        {"mesh",{{"op","instance"},{"input","card"},{"points","facing"}}}});
    auto baseline=mesh(d); d["nodes"]["mesh"]["scale_jitter"]={0,0,0}; auto zero=mesh(d);
    for(size_t i=0;i<zero.vertices.size();++i) expect(length(zero.vertices[i].position-baseline.vertices[i].position)==0&&zero.vertices[i].color==baseline.vertices[i].color,"zero size variation preserves defaults");
    d["nodes"]["mesh"]["scale_jitter"]={.1,.15,0}; auto varied=mesh(d),repeat=mesh(d); bool changed=false,aspectChanged=false;
    for(size_t i=0;i<varied.vertices.size();i+=4) {
        float width=length(varied.vertices[i+1].position-varied.vertices[i].position),height=length(varied.vertices[i+2].position-varied.vertices[i+1].position);
        expect(width>=1.8f-1e-5f&&width<=2.2f+1e-5f&&height>=3.4f-1e-5f&&height<=4.6f+1e-5f,"independent size variation stays in specified bounds");
        expect(length((varied.vertices[i].position+varied.vertices[i+1].position-baseline.vertices[i].position-baseline.vertices[i+1].position)*.5f)<1e-6,"size variation preserves attachment pivot");
        changed=changed||std::abs(width-2)>.01f; aspectChanged=aspectChanged||std::abs(width/height-.5f)>.01f;
    }
    expect(changed&&aspectChanged,"instance size jitter stretches aspect ratio");
    for(size_t i=0;i<varied.vertices.size();++i) {
        expect(length(varied.vertices[i].position-repeat.vertices[i].position)==0,"size variation repeats with same seed");
        expect(varied.vertices[i].color==baseline.vertices[i].color,"size variation does not perturb color stream");
        expect(varied.vertices[i].uv.x==baseline.vertices[i].uv.x&&varied.vertices[i].uv.y==baseline.vertices[i].uv.y,"size variation keeps texture coordinates attached");
    }
    Options other; other.seed=314; auto another=mesh(d,other); changed=false;
    for(size_t i=0;i<varied.vertices.size();++i) changed=changed||length(varied.vertices[i].position-another.vertices[i].position)>.001f;
    expect(changed,"root seed changes size variation");
    d["nodes"]["mesh"]["seed"]=17; auto fixed=mesh(d),isolated=mesh(d,other);
    for(size_t i=0;i<fixed.vertices.size();++i) expect(length(fixed.vertices[i].position-isolated.vertices[i].position)==0,"explicit instance seed isolates size variation");
    d["nodes"]["tilted"]={{"op","transform"},{"input","card"},{"rotation",{35,25,0}}}; d["nodes"]["mesh"]["input"]="tilted";
    d["nodes"]["mesh"]["scale_jitter"]={.4,.6,.7}; d["nodes"]["mesh"]["rotation"]={12,23,34}; auto stretched=mesh(d);
    for(const auto& face:stretched.triangles) {
        const auto& a=stretched.vertices[face.vertices[0]]; const auto& b=stretched.vertices[face.vertices[1]]; const auto& c=stretched.vertices[face.vertices[2]];
        expect(dot(normalized(cross(b.position-a.position,c.position-a.position)),a.normal)>.99999f,"nonuniform size transforms normals with inverse scale before rotation");
    }
    d["nodes"]["mesh"]["scale_jitter"]={-.01,0,0}; fails([&]{parse(d);},"bounds");
    d["nodes"]["mesh"]["scale_jitter"]={1,0,0}; fails([&]{parse(d);},"bounds");
}
void atlasSelection(const std::filesystem::path& source, const std::filesystem::path& work) {
    auto directory=std::filesystem::absolute(work/"atlas-fixture"); std::filesystem::create_directories(directory);
    auto atlas=Json::parse(bytes(source/"assets/spritesheets/leaves.atlas.json"));
    for(const auto& image:atlas["images"]) {
        auto name=image["file"].get<std::string>(); std::filesystem::create_directories((directory/name).parent_path());
        std::filesystem::copy_file(source/"assets/spritesheets"/name,directory/name,std::filesystem::copy_options::overwrite_existing);
    }
    auto manifest=directory/"atlas.json"; auto save=[&](const Json& value) { std::ofstream file(manifest); file<<value.dump(2); }; save(atlas);
    Json card={{"op","card"},{"planes",2},{"segments",4},{"width_segments",2},{"curl",-.1},{"fold",-.2},{"uv_rect",{.1,.2,.8,.9}}};
    auto prototype=mesh(doc({{"mesh",card}})); size_t perCopy=prototype.vertices.size();
    auto d=doc({{"card",card},{"sites",{{"op","radial"},{"count",24},{"radius",{1,1}},{"jitter",0}}},
        {"mesh",{{"op","instance"},{"input","card"},{"points","sites"},{"atlas",manifest.string()},{"atlas_mode","cycle"},{"scale_jitter",{.1,.15,.1}}}}});
    auto cycle=mesh(d);
    auto verifyCell=[&](const Mesh& m, size_t copy, size_t index) {
        const auto& rect=atlas["cells"][index]["uv_rect"]; float u0=rect[0],v0=1-rect[3].get<float>(),u1=rect[2],v1=1-rect[1].get<float>();
        for(size_t v=0;v<perCopy;++v) {
            const auto& uv=m.vertices[copy*perCopy+v].uv; const auto& src=prototype.vertices[v].uv;
            expect(std::abs(uv.x-(u0+(u1-u0)*src.x))<1e-6&&std::abs(uv.y-(v0+(v1-v0)*src.y))<1e-6,"atlas selects matching cell and preserves local UV rectangle on all crossed planes");
        }
    };
    for(size_t i=0;i<24;++i) verifyCell(cycle,i,i%12);
    auto plainDoc=d; plainDoc["nodes"]["mesh"].erase("atlas"); plainDoc["nodes"]["mesh"].erase("atlas_mode"); auto plain=mesh(plainDoc);
    for(size_t i=0;i<plain.vertices.size();++i) expect(length(plain.vertices[i].position-cycle.vertices[i].position)==0&&plain.vertices[i].color==cycle.vertices[i].color,"atlas selection leaves size and color streams unchanged");
    d["nodes"]["mesh"]["atlas_mode"]="fixed"; d["nodes"]["mesh"]["atlas_index"]=5; auto fixed=mesh(d);
    for(size_t i=0;i<24;++i) verifyCell(fixed,i,5);
    auto graph=parse(d); auto generated=generate(graph); write(graph,generated,directory/"export");
    auto file=bytes(directory/"export/test.glb"); auto gltf=glbJson(file); size_t accessor=gltf["meshes"][0]["primitives"][0]["attributes"]["TEXCOORD_0"];
    auto view=gltf["bufferViews"][gltf["accessors"][accessor]["bufferView"].get<size_t>()]; size_t binary=20+read32(file,12)+8+view["byteOffset"].get<size_t>();
    uint32_t bits=read32(file,binary+4); float exportedV; std::memcpy(&exportedV,&bits,4);
    expect(std::abs(exportedV-(1-fixed.vertices[0].uv.y))<1e-7,"GLB applies exactly one V flip after atlas remapping");
    atlas["count"]=3; while(atlas["cells"].size()>3) atlas["cells"].erase(atlas["cells"].end()-1); save(atlas);
    d["nodes"]["mesh"]["atlas_mode"]="random"; d["nodes"]["mesh"]["atlas_index"]=0; auto random=mesh(d),repeat=mesh(d); std::set<int> selected;
    for(size_t i=0;i<24;++i) {
        float u=random.vertices[i*perCopy].uv.x; int index=int(u*4); selected.insert(index); expect(index>=0&&index<3,"random atlas selection excludes unoccupied cells"); verifyCell(random,i,size_t(index));
    }
    expect(selected.size()>1,"random atlas selection varies between copies");
    for(size_t i=0;i<random.vertices.size();++i) expect(random.vertices[i].uv.x==repeat.vertices[i].uv.x&&random.vertices[i].uv.y==repeat.vertices[i].uv.y,"atlas selection repeats with same seed");
    Options other; other.seed=314; auto changed=mesh(d,other); bool different=false;
    for(size_t i=0;i<random.vertices.size();++i) different=different||random.vertices[i].uv.x!=changed.vertices[i].uv.x;
    expect(different,"new seed changes atlas choices");
    d["nodes"]["mesh"]["seed"]=81; auto pinned=mesh(d),isolated=mesh(d,other);
    for(size_t i=0;i<pinned.vertices.size();++i) expect(pinned.vertices[i].uv.x==isolated.vertices[i].uv.x&&pinned.vertices[i].uv.y==isolated.vertices[i].uv.y,"explicit instance seed isolates atlas choices");
    d["nodes"]["mesh"]["atlas_index"]=3; fails([&]{parse(d);},"occupied cell"); d["nodes"]["mesh"]["atlas_index"]=0;
    auto bad=atlas; bad["cells"][0]["uv_rect"][1]=.5; save(bad); fails([&]{parse(d);},"UV rectangle");
    bad=atlas; bad["columns"]=3; save(bad); fails([&]{parse(d);},"dimensions disagree");
    std::filesystem::copy_file(source/"assets/leaf.png",directory/"small.png",std::filesystem::copy_options::overwrite_existing);
    bad=atlas; bad["images"][0]["file"]="small.png"; save(bad); fails([&]{parse(d);},"PNG dimensions"); save(atlas);
    auto noAtlas=plainDoc; noAtlas["nodes"]["mesh"]["atlas_mode"]="cycle"; fails([&]{parse(noAtlas);},"requires an atlas");
    auto protectedPath=directory/"protected.glb"; std::ofstream protectedFile(protectedPath); protectedFile<<atlas.dump(); protectedFile.close();
    d["nodes"]["mesh"]["atlas"]=protectedPath.string(); d["outputs"]={{"protected.glb","mesh"}}; graph=parse(d); generated=generate(graph);
    auto before=bytes(protectedPath); fails([&]{write(graph,generated,directory);},"overwrite input asset"); expect(bytes(protectedPath)==before,"atlas manifest protected from export overwrite");
}
void lodExports(const std::filesystem::path& source, const std::filesystem::path& work) {
    auto d=doc({{"card",{{"op","card"},{"planes",2},{"segments",8},{"width_segments",4},{"curl",.2},{"fold",.1}}},
        {"sites",{{"op","radial"},{"count",40},{"radius",{2,2}},{"jitter",0},{"seed",99}}},
        {"mesh",{{"op","instance"},{"input","card"},{"points","sites"},{"atlas",(source/"assets/spritesheets/leaves.atlas.json").string()},{"scale_jitter",{.1,.2,.15}}}}});
    size_t perCopy=2*9*5;
    auto subset=[&](const Mesh& full, const Mesh& reduced) {
        std::set<size_t> indices;
        for(size_t i=0;i<reduced.vertices.size();i+=perCopy) {
            size_t found=full.vertices.size();
            for(size_t k=0;k<full.vertices.size();k+=perCopy) if(length(full.vertices[k].position-reduced.vertices[i].position)==0) { found=k; break; }
            expect(found<full.vertices.size(),"density keeps original placement"); indices.insert(found/perCopy);
            for(size_t k=0;k<perCopy;++k) {
                auto a=full.vertices[found+k],b=reduced.vertices[i+k];
                expect(length(a.position-b.position)==0&&length(a.normal-b.normal)==0&&a.uv.x==b.uv.x&&a.uv.y==b.uv.y&&a.color==b.color&&a.wind.x==b.wind.x&&a.wind.y==b.wind.y,"density preserves survivor shape, atlas, variation, normals and wind");
            }
        }
        return indices;
    };
    for(auto mode:{"random","cycle"}) {
        d["nodes"]["mesh"]["atlas_mode"]=mode; d["nodes"]["mesh"]["density"]=1; auto full=mesh(d);
        d["nodes"]["mesh"]["density"]=.5; auto half=mesh(d); auto halves=subset(full,half); expect(halves.size()==20,"exact half density");
        d["nodes"]["mesh"]["density"]=.2; auto fifth=mesh(d); auto fifths=subset(full,fifth); expect(fifths.size()==8,"exact fifth density");
        for(auto index:fifths) expect(halves.count(index),"lower densities are nested subsets");
        auto again=mesh(d); expect(again.vertices.size()==fifth.vertices.size(),"density repeat counts"); subset(fifth,again);
        Options other; other.seed=314; auto changed=mesh(d,other); expect(length(changed.vertices[0].position-fifth.vertices[0].position)>0,"density responds to seed");
        d["nodes"]["mesh"]["seed"]=8; auto fixed=mesh(d),isolated=mesh(d,other); subset(fixed,isolated); d["nodes"]["mesh"].erase("seed");
        d["nodes"]["mesh"]["density"]=0; fails([&]{mesh(d);},"empty");
    }
    d["nodes"]["mesh"]["density"]=1;
    auto base=mesh(d); d["lods"]={{"LOD1",{{"density",.5},{"topology",.5},{"screen_height",.3}}},{"LOD2",{{"density",.2},{"topology",.1},{"overrides",{{"card",{{"planes",1}}}}}}}};
    auto graph=parse(d); expect(mesh(d).vertices.size()==base.vertices.size(),"base generation ignores export LOD reductions");
    auto first=selectLod(graph,"LOD1"),last=selectLod(graph,"LOD2");
    expect(first.document["nodes"]["card"]["segments"]==4&&last.document["nodes"]["card"]["segments"]==2&&last.document["nodes"]["card"]["width_segments"]==2,"automatic topology retains valid card deformation minima");
    expect(first.document["nodes"]["sites"]==graph.document["nodes"]["sites"],"LOD retains placement source controls");
    auto low=generate(last); expect(low.outputs.begin()->second->mesh.triangles.size()<base.triangles.size(),"LOD reduces exported topology");
    auto result=exportGraph(graph,work/"lods"); expect(result["lods"].size()==2,"export includes configured LODs");
    auto manifest=Json::parse(bytes(work/"lods/lods.json")); expect(manifest["levels"].size()==3&&manifest["levels"][1]["outputs"][0]["path"]=="test-LOD1.glb","portable manifest names every level");
    auto baseReport=exportGraph(graph,work/"lod-base",{},false); expect(!baseReport.contains("lods")&&!std::filesystem::exists(work/"lod-base/test-LOD1.glb"),"base-only export opt-out");
    expect(parse(graph.document).document==graph.document,"LOD normalized document reparses");
    auto invalid=d; invalid["lods"]["LOD1"]["topology"]=0; fails([&]{parse(invalid);},"bounds");
    invalid=d; invalid["lods"]["LOD1"]["tube_stride"]=1.2; fails([&]{parse(invalid);},"integer");
    invalid=d; invalid["lods"]["LOD1"]["typo"]=true; fails([&]{parse(invalid);},"unknown parameter");
    invalid=d; invalid["lods"]["../escape"]={{"density",.5}}; fails([&]{parse(invalid);},"invalid LOD name");
    invalid=d; invalid["lods"]["lod1"]=Json::object(); fails([&]{parse(invalid);},"duplicate LOD name");
    invalid=d; invalid["lods"]["LOD1"]["overrides"]={{"missing",{{"sides",4}}}}; fails([&]{parse(invalid);},"unknown override node");
    invalid=d; invalid["lods"]["LOD1"]["overrides"]={{"card",{{"op","leaf"}}}}; fails([&]{parse(invalid);},"without op");
    invalid=d; invalid["lods"]["LOD1"]["overrides"]={{"sites",{{"direction",{0,0,0}}}}}; fails([&]{parse(invalid);},"unknown parameter");
    invalid=d; invalid["lods"]["LOD1"]["outputs"]={"missing.glb"}; fails([&]{parse(invalid);},"output filename");
    fails([&]{selectLod(graph,"missing");},"unknown LOD");
    invalid=d; invalid["outputs"]["test-LOD1.glb"]="mesh"; auto collision=parse(invalid); fails([&]{exportGraph(collision,work/"lod-collision");},"collision"); expect(!std::filesystem::exists(work/"lod-collision/test.glb"),"LOD collision preflight precedes any file writes");
}

void stagedGrowth(const std::filesystem::path& source, const std::filesystem::path& work) {
    auto graph=load(source/"graphs/growth-tree.json");
    auto node=[](const Result& result, const std::string& name) { for(const auto& n:result.stats["nodes"]) if(n["node"]==name) return n; throw std::runtime_error("missing node"); };
    auto at=[&](double progress) { Options opt; opt.growth=progress; return generate(graph,opt); };
    auto zero=at(0),early=at(.4),middle=at(.75),late=at(.875),full=at(1);
    expect(zero.outputs.begin()->second->mesh.triangles.empty(),"growth starts empty when all geometry depends on staged trunk");
    expect(node(early,"trunk")["stems"]==1&&node(early,"limbs")["stems"]==0,"branches absent through 40 percent");
    expect(node(middle,"limbs")["stems"]==14&&node(middle,"leaves")["mesh"]["triangles"]==0,"leaves absent through 75 percent");
    expect(node(late,"leaves")["mesh"]["triangles"].get<int>()>0,"leaves develop after their start");
    write(graph,generate(graph),work/"growth-mature"); write(graph,full,work/"growth-final");
    expect(bytes(work/"growth-mature/growth-tree.glb")==bytes(work/"growth-final/growth-tree.glb"),"final growth exactly equals mature export");
    auto report=exportGraph(graph,work/"growth"); expect(report["growth"].size()==9,"all configured growth steps export");
    expect(!std::filesystem::exists(work/"growth/growth-tree-G00.glb")&&std::filesystem::exists(work/"growth/growth-tree-G08.glb"),"zero step omits invalid empty GLB");
    auto manifest=Json::parse(bytes(work/"growth/growth.json")); expect(manifest["snapshots"][0]["empty_outputs"][0]=="growth-tree-G00.glb"&&manifest["snapshots"][8]["progress"]==1,"growth manifest records empty and final snapshots");
    std::ofstream(work/"growth/growth-tree-G00.glb")<<"old mesh"; exportGraph(graph,work/"growth",{},false,true,0);
    expect(!std::filesystem::exists(work/"growth/growth-tree-G00.glb"),"empty step removes stale generated output");
    auto d=doc({{"card",{{"op","card"}}},{"sites",{{"op","radial"},{"count",1},{"center",{7,2,3}},{"radius",{0,0}},{"tilt",{0,0}},{"jitter",0}}},{"mesh",{{"op","instance"},{"input","card"},{"points","sites"}}}});
    d["growth"]={{"steps",3},{"stages",{{"mesh",{{"start",.5},{"end",1}}}}}};
    Options opt; opt.growth=.75; auto partial=mesh(d,opt),grown=mesh(d); Vec3 pivot{7,2,3};
    for(size_t i=0;i<partial.vertices.size();++i) expect(length(partial.vertices[i].position-(pivot+(grown.vertices[i].position-pivot)*.5f))<1e-6f,"instance grows at its own attachment, not world origin");
    auto repeated=mesh(d,opt); expect(repeated.vertices[2].position.x==partial.vertices[2].position.x,"growth repeats for same seed");
    d["lods"]={{"LOD1",{{"density",1}}}}; d["growth"]["lods"]=true;
    expect(exportGraph(parse(d),work/"growth-lods")["growth"].size()==6,"growth and LOD combinations exported");
    auto selected=selectLod(parse(d),"LOD1"); auto selectedReport=exportGraph(selected,work/"selected-growth-lod",{},true,true,2);
    expect(selectedReport["growth"][0]["level"]=="LOD1","single selected LOD keeps its identity in growth reports");
    auto selectedManifest=Json::parse(bytes(work/"selected-growth-lod/growth.json")); expect(selectedManifest["snapshots"][0]["level"]=="LOD1","single selected LOD keeps its identity in growth manifest");
    auto invalid=d; invalid["growth"]["stages"]["missing"]=Json::object(); fails([&]{parse(invalid);},"unknown growth stage node");
    invalid=d; invalid["growth"]["stages"]["mesh"]["end"]=.5; fails([&]{parse(invalid);},"start must precede end");
    invalid=d; invalid["growth"]["steps"]=1; fails([&]{parse(invalid);},"integer in 2..32");
    invalid=d; invalid["growth"]["stages"]["mesh"]["typo"]=1; fails([&]{parse(invalid);},"unknown growth stage field");
    invalid=d; invalid["growth"]["stages"]["mesh"]["pivot"]={1,0,0}; fails([&]{parse(invalid);},"pivot applies only");
    opt.growth=-.1; fails([&]{mesh(d,opt);},"progress must be within");
    fails([&]{exportGraph(graph,work/"bad-step",{},true,true,9);},"step in range");
    invalid=d; invalid["outputs"]["test-G01.glb"]="mesh"; fails([&]{exportGraph(parse(invalid),work/"growth-collision");},"collision");
}

void surfaceFacing() {
    auto d=doc({{"sphere",{{"op","ellipsoid"},{"radii",{1,1,1}},{"rings",24},{"sides",32}}},{"pin",{{"op","card"},{"width",.001},{"height",.02}}},{"sites",{{"op","scatter"},{"input","sphere"},{"count",400},{"angle",0},{"normal_direction",{0,0,1}},{"max_surface_angle",60},{"scale",{1,1}}}},{"mesh",{{"op","instance"},{"input","pin"},{"points","sites"}}}});
    auto result=generate(parse(d)); auto m=result.outputs.begin()->second->mesh;
    expect(!m.vertices.empty()&&m.vertices.size()<400*4,"surface cone accepts a subset");
    for(size_t i=0;i<m.vertices.size();i+=4) {
        Vec3 base=(m.vertices[i].position+m.vertices[i+1].position)*.5f,tip=(m.vertices[i+2].position+m.vertices[i+3].position)*.5f;
        expect(base.z>.49f&&normalized(tip-base).z>=.5f-1e-5f,"all pins on facing cap and local growth follows surface normal");
    }
    bool found=false; for(const auto& n:result.stats["nodes"]) if(n["node"]=="sites") { found=true; expect(n["normal_filter"]["accepted"].get<size_t>()==m.vertices.size()/4,"normal filter reports accepted candidates"); } expect(found,"normal filter diagnostics exist");
    d["nodes"]["sites"]["normal_direction"]={0,0,5}; auto scaled=mesh(d); expect(scaled.vertices.size()==m.vertices.size()&&length(scaled.vertices[0].position-m.vertices[0].position)==0,"normal direction magnitude does not matter");
    d["nodes"]["sites"]["max_surface_angle"]=180; expect(mesh(d).vertices.size()==400*4,"180-degree cone retains all candidates");
    auto all=mesh(d); d["nodes"]["sites"].erase("normal_direction"); d["nodes"]["sites"].erase("max_surface_angle"); auto unfiltered=mesh(d);
    for(size_t i=0;i<all.vertices.size();++i) expect(length(all.vertices[i].position-unfiltered.vertices[i].position)==0,"filter does not change random candidate stream");
    d["nodes"]["sites"]["normal_direction"]={0,0,0}; fails([&]{parse(d);},"nonzero");
    d["nodes"]["sites"]["normal_direction"]={0,0,1}; d["nodes"]["sites"].erase("input"); fails([&]{parse(d);},"requires mesh input");
}

void petalOutlines() {
    auto d=doc({{"mesh",{{"op","leaf"},{"shape","petal"},{"segments",20},{"width_segments",8},{"length",.2},{"width",.06}}}});
    auto original=mesh(d);
    d["nodes"]["mesh"]["width_profile"]={{0,0},{.25,.6},{.5,1},{.8,.8},{1,0}};
    auto profiled=mesh(d); expect(profiled.vertices.size()==original.vertices.size(),"custom outline retains topology");
    expect(std::abs(profiled.vertices[1+9*9+8].position.x-.03f)<1e-6f,"profile controls actual half-width at midpoint");
    d["nodes"]["mesh"]["lateral_bend"]=.12; d["nodes"]["mesh"]["edge_wave"]=.2;
    auto folded=mesh(d); expect(length(folded.vertices.front().position)==0,"petal deformations retain attachment pivot");
    expect(std::abs(folded.vertices.back().position.x-.024f)<1e-6f,"lateral bend displaces tip by length fraction");
    bool changed=false;
    for(size_t i=0;i<folded.vertices.size();++i) { expect(folded.vertices[i].uv.x==profiled.vertices[i].uv.x&&folded.vertices[i].uv.y==profiled.vertices[i].uv.y,"petal UVs stay attached"); changed|=std::abs(folded.vertices[i].position.z-profiled.vertices[i].position.z)>1e-6f; }
    expect(changed,"edge ripple changes geometry");
    auto invalid=d; invalid["nodes"]["mesh"]["width_profile"]={{0,1},{.5,1},{1,0}}; fails([&]{parse(invalid);},"zero end widths");
    invalid=d; invalid["nodes"]["mesh"]["width_profile"]={{0,0},{.5,0},{1,0}}; fails([&]{parse(invalid);},"interior widths must be positive");
}

void packedRibbons(const std::filesystem::path& source, const std::filesystem::path& work) {
    auto d=doc({{"path",{{"op","curve"},{"points",{{2,3,1},{2,2,1},{2.2,1,1},{2.5,0,1}}}}},{"mesh",{{"op","ribbon"},{"input","path"},{"width",.4},{"planes",2},{"stride",2},{"color_variation",0}}}});
    auto m=mesh(d); expect(m.vertices.size()==12&&m.triangles.size()==8,"ribbon count includes partial final stride and crossed planes");
    for(size_t plane=0;plane<2;++plane) {
        expect(length((m.vertices[plane*6].position+m.vertices[plane*6+1].position)*.5f-Vec3{2,3,1})<1e-6f,"ribbon base stays on path");
        expect(length((m.vertices[plane*6+4].position+m.vertices[plane*6+5].position)*.5f-Vec3{2.5,0,1})<1e-6f,"ribbon retains path tip");
        expect(m.vertices[plane*6].uv.y==0&&m.vertices[plane*6+4].uv.y==1,"ribbon UVs span its whole path");
        expect(std::abs(length(m.vertices[plane*6].position-m.vertices[plane*6+1].position)-.4f)<1e-6f,"ribbon uses physical width");
    }
    Options limited; limited.limits.vertices=11; fails([&]{mesh(d,limited);},"vertex budget");
    auto invalid=d; invalid["nodes"]["mesh"]["width_profile"]={{0,0},{1,1}}; fails([&]{parse(invalid);},"allowed bounds");
    d["nodes"]["mesh"]["atlas"]=(source/"assets/willow/packed/sprays.atlas.json").string(); d["nodes"]["mesh"]["atlas_mode"]="fixed"; d["nodes"]["mesh"]["atlas_index"]=2;
    m=mesh(d); expect(m.vertices[0].uv.x>2.0f/3&&m.vertices[1].uv.x<1&&m.vertices[4].uv.y<1,"ribbon uses occupied atlas content with padding");
    for(size_t i=0;i<6;++i) expect(m.vertices[i].uv.x==m.vertices[6+i].uv.x&&m.vertices[i].uv.y==m.vertices[6+i].uv.y,"crossed strips use one common atlas cell");
    d["nodes"]["trunk"]={{"op","trunk"},{"segments",4},{"noise",0}};
    d["nodes"]["branches"]={{"op","branch"},{"input","trunk"},{"count",10},{"segments",3},{"seed",90}};
    d["nodes"]["mesh"]["input"]="branches"; d["nodes"]["mesh"]["rotation_jitter"]=180; d["nodes"]["mesh"]["color_variation"]=.3; d["nodes"]["mesh"]["atlas_mode"]="cycle";
    auto full=mesh(d); d["nodes"]["mesh"]["density"]=.4; auto reduced=mesh(d); expect(reduced.vertices.size()==4*12,"ribbon density selects exact retained count");
    for(size_t i=0;i<reduced.vertices.size();i+=12) {
        size_t found=full.vertices.size(); for(size_t k=0;k<full.vertices.size();k+=12) if(length(full.vertices[k].position-reduced.vertices[i].position)==0) {found=k;break;}
        expect(found<full.vertices.size(),"ribbon density retains original placement");
        for(size_t k=0;k<12;++k) { auto a=full.vertices[found+k],b=reduced.vertices[i+k]; expect(length(a.position-b.position)==0&&length(a.normal-b.normal)==0&&a.uv.x==b.uv.x&&a.uv.y==b.uv.y&&a.color==b.color,"ribbon survivor keeps roll, shape, atlas and color"); }
    }
    d["nodes"]["mesh"]["density"]=0; fails([&]{mesh(d);},"empty");
    Options growth; growth.growth=0; expect(mesh(d,growth).triangles.empty(),"empty ribbons supported during growth");
    d["nodes"]["mesh"]["density"]=1; d["lods"]={{"Low",{{"density",.5},{"tube_stride",2}}}};
    auto low=selectLod(parse(d),"Low"); expect(low.document["nodes"]["mesh"]["density"]==.5&&low.document["nodes"]["mesh"]["stride"]==4,"LOD controls apply to packed ribbons");
    auto graph=load(source/"graphs/weeping-willow.json"); auto packed=generate(selectLod(graph,"Cards"));
    expect(packed.outputs.begin()->second->mesh.triangles.size()==52082,"packed willow retains welded structure at much lower topology");
    write(selectLod(graph,"Cards"),packed,work/"packed-willow"); auto glb=glbJson(bytes(work/"packed-willow/weeping-willow-Cards.glb")); bool cutout=false;
    for(const auto& mat:glb["materials"]) if(mat["name"]=="willow_cards") cutout=mat["alphaMode"]=="MASK"&&mat["doubleSided"]==true&&mat["pbrMetallicRoughness"].contains("baseColorTexture");
    expect(cutout,"packed willow exports actual alpha card material and embedded textures");
}

void files(const std::filesystem::path& source, const std::filesystem::path& work) {
    std::filesystem::create_directories(work); auto graph=load(source/"graphs/tree.json"); auto result=generate(graph); write(graph,result,work/"first"); auto again=generate(graph); write(graph,again,work/"second");
    expect(bytes(work/"first/tree.glb")==bytes(work/"second/tree.glb"),"same seed byte-identical GLB"); expect(bytes(work/"first/tree.obj")==bytes(work/"second/tree.obj"),"same seed byte-identical OBJ");
    auto manifest=glbJson(bytes(work/"first/tree.glb")); expect(manifest["materials"].size()==3,"GLB material slots");
    auto attrs=manifest["meshes"][0]["primitives"][0]["attributes"]; expect(attrs.contains("NORMAL")&&attrs.contains("TEXCOORD_0")&&attrs.contains("COLOR_0")&&attrs.contains("_WIND"),"GLB vertex attributes");
    Options opt; opt.seed=43; auto changed=generate(graph,opt); write(graph,changed,work/"changed"); expect(bytes(work/"first/tree.glb")!=bytes(work/"changed/tree.glb"),"different seed produces different GLB");
    auto d=simple(); d["nodes"]["stem"]["noise"]=.3; d["nodes"]["stem"]["seed"]=12; auto a=mesh(d),b=mesh(d,opt);
    for(size_t i=0;i<a.vertices.size();++i) expect(length(a.vertices[i].position-b.vertices[i].position)==0,"explicit node seed overrides root seed");
    d["nodes"]["unrelated"]={{"op","leaf"}}; auto c=mesh(d); for(size_t i=0;i<a.vertices.size();++i) expect(length(a.vertices[i].position-c.vertices[i].position)==0,"unrelated node does not perturb random stream");
    std::ofstream obj(work/"prototype.obj"); obj<<"v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nf -4/1 -3/2 -2/3 -1/4\n"; obj.close();
    d=doc({{"mesh",{{"op","mesh"},{"path","prototype.obj"}}}}); graph=parse(d,work); result=generate(graph); expect(result.outputs.begin()->second->mesh.triangles.size()==2,"OBJ negative indices and polygon triangulation");
    auto materialDoc=simple(); materialDoc["materials"]={{"bark",{{"base_color_texture","bad.png"}}}}; std::ofstream bad(work/"bad.png"); bad<<"not an image"; bad.close(); auto invalid=parse(materialDoc,work); auto generated=generate(invalid); fails([&]{write(invalid,generated,work/"bad-export");},"PNG or JPEG");
    d["outputs"]={{"prototype.obj","mesh"}}; graph=parse(d,work); result=generate(graph); auto before=bytes(work/"prototype.obj"); fails([&]{write(graph,result,work);},"overwrite input asset"); expect(bytes(work/"prototype.obj")==before,"source mesh retained");
    auto cards=load(source/"graphs/cards-outward.json"); auto cardResult=generate(cards); write(cards,cardResult,work/"textured");
    auto textured=glbJson(bytes(work/"textured/cards-outward.glb")); expect(textured["images"].size()==1&&textured["images"][0]["mimeType"]=="image/png","GLB embeds PNG texture");
    bool masked=false; for(const auto& m:textured["materials"]) if(m["name"]=="leaf") { masked=m["alphaMode"]=="MASK"&&m["doubleSided"]==true&&m["pbrMetallicRoughness"].contains("baseColorTexture"); }
    expect(masked,"card export retains alpha cutout material");
    for(const auto& m:textured["materials"]) expect(!m.contains("extras"),"default optics do not add custom material metadata");
    auto optical=cards.document;
    optical["materials"]["leaf"]["translucency"]=.16;
    optical["materials"]["leaf"]["translucency_color"]={.9,.8,.7};
    optical["materials"]["leaf"]["subsurface"]=.12;
    optical["materials"]["leaf"]["subsurface_scale"]=.001;
    optical["materials"]["leaf"]["subsurface_radius"]={1,.45,.25};
    auto opticalGraph=parse(optical,cards.base); auto opticalResult=generate(opticalGraph); write(opticalGraph,opticalResult,work/"optical");
    auto opticalGlb=glbJson(bytes(work/"optical/cards-outward.glb"));
    bool opticalFound=false;
    for(const auto& m:opticalGlb["materials"]) if(m["name"]=="leaf") {
        const auto& metadata=m["extras"]["foliageutil"]; opticalFound=true;
        expect(metadata["version"]==1,"versioned optics metadata");
        for(auto key:{"translucency","translucency_color","subsurface","subsurface_scale","subsurface_radius"}) expect(metadata[key]==optical["materials"]["leaf"][key],"optical setting survives GLB export");
        expect(m["alphaMode"]=="MASK"&&m["alphaCutoff"]==cards.document["materials"]["leaf"]["alpha_cutoff"],"tissue transmission preserves cutout alpha settings");
    }
    expect(opticalFound,"optical material exported");
    expect(opticalGlb["accessors"]==textured["accessors"]&&opticalGlb["images"]==textured["images"],"optical settings preserve mesh and embedded texture layout");
    expect(!textured.contains("extensionsUsed"),"default materials retain core glTF only");
    auto gem=doc({{"mesh",{{"op","crystal"},{"material","gem"}}}});
    gem["materials"]["gem"]={{"base_color",{.8,.03,.08,1}},{"transmission",.9},{"ior",1.77},{"thickness",.4},{"attenuation_color",{.8,.1,.2}},{"attenuation_distance",.5}};
    auto gemGraph=parse(gem); write(gemGraph,generate(gemGraph),work/"gem"); auto gemGlb=glbJson(bytes(work/"gem/test.glb"));
    expect(gemGlb["extensionsUsed"]==Json::array({"KHR_materials_ior","KHR_materials_transmission","KHR_materials_volume"}),"gem declares deduplicated standard extensions");
    const auto& gemMaterial=gemGlb["materials"][0]; const auto& extensions=gemMaterial["extensions"];
    expect(gemMaterial["alphaMode"]=="OPAQUE"&&gemMaterial["pbrMetallicRoughness"]["baseColorFactor"][3]==1,"refraction retains full surface coverage");
    expect(extensions["KHR_materials_transmission"]["transmissionFactor"]==.9&&extensions["KHR_materials_ior"]["ior"]==1.77,"transmission and IOR survive export");
    expect(extensions["KHR_materials_volume"]==Json({{"thicknessFactor",.4},{"attenuationColor",{.8,.1,.2}},{"attenuationDistance",.5}}),"volume absorption survives export");
    auto clear=gem; clear["materials"]["gem"].erase("attenuation_distance"); auto clearGraph=parse(clear); write(clearGraph,generate(clearGraph),work/"clear-gem");
    expect(!glbJson(bytes(work/"clear-gem/test.glb"))["materials"][0]["extensions"]["KHR_materials_volume"].contains("attenuationDistance"),"infinite attenuation omits distance rather than encoding infinity");
    for(auto badMaterial:std::vector<Json>{{{"ior",.9}},{{"transmission",1.01}},{{"thickness",-1}},{{"attenuation_distance",0}},{{"attenuation_color",{1,-.1,1}}}}) {
        auto invalid=gem; invalid["materials"]["unused"]=badMaterial; fails([&]{parse(invalid);},"outside allowed bounds");
    }
    auto badGem=gem; badGem["materials"]["gem"]["transmission"]=0; fails([&]{parse(badGem);},"thickness requires transmission");
    badGem=gem; badGem["materials"]["gem"]["thickness"]=0; fails([&]{parse(badGem);},"attenuation_distance requires thickness");
    for(const auto& key:{"translucency","subsurface"}) {
        auto bad=optical; bad["materials"]["leaf"][key]=1.01; fails([&]{parse(bad,cards.base);},"outside allowed bounds");
        bad["materials"]["leaf"][key]=-.01; fails([&]{parse(bad,cards.base);},"outside allowed bounds");
    }
    for(const auto& key:{"subsurface_scale","subsurface_radius","translucency_color"}) {
        auto bad=optical; bad["materials"]["unused"][key]=key==std::string("subsurface_scale")?Json(0):Json::array({1,-1,1});
        fails([&]{parse(bad,cards.base);},"outside allowed bounds");
    }
    expect(bytes(work/"textured/cards-outward-textures/leaf-base_color_texture.png")==bytes(source/"assets/leaf.png"),"OBJ copies original texture bytes");
    auto rose=generate(load(source/"graphs/rose.json")); expect(rose.outputs.begin()->second->mesh.triangles.size()>2000,"rose curvature and spiral sample");
}
}
int main(int argc, char** argv) {
    try { if(argc!=3) throw std::runtime_error("tests need source and output directories"); geometry(); growthProfiles(); solidMeshes(); rootsAndProfiles(); validation(); scattering(); proximityScattering(); orientations(); cardVariation(); atlasSelection(argv[1],argv[2]); lodExports(argv[1],argv[2]); stagedGrowth(argv[1],argv[2]); surfaceFacing(); petalOutlines(); packedRibbons(argv[1],argv[2]); files(argv[1],argv[2]); std::cout<<checks<<" checks passed\n"; return 0; }
    catch(const std::exception& e) { std::cerr<<"test failure: "<<e.what()<<'\n'; return 1; }
}
