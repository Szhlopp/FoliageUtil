#include "internal.hpp"
#include "proximity.hpp"
#include "noise.hpp"
#include <fstream>
#include <limits>
#include <numeric>
#include <sstream>

namespace foliage {
namespace {
float n(const Json& j, const char* key) { return j.at(key).get<float>(); }
std::string str(const Json& j, const char* key) { return j.at(key).get<std::string>(); }
float lerp(float a, float b, float t) { return a+(b-a)*t; }
float interval(const Json& pair, float t) { return lerp(pair[0].get<float>(),pair[1].get<float>(),t); }
float profileAt(const Json& keys, float t) {
    for(size_t i=1;i<keys.size();++i) if(t<=keys[i][0].get<float>()) {
        float a=keys[i-1][0].get<float>(),b=keys[i][0].get<float>();
        return lerp(keys[i-1][1].get<float>(),keys[i][1].get<float>(),clamp01((t-a)/(b-a)));
    }
    return keys.back()[1].get<float>();
}
Vec3 sphericalDirection(Vec3 from, Vec3 to, float t) {
    if(t<=0) return from;
    if(t>=1) return to;
    float cosine=std::clamp(dot(from,to),-1.0f,1.0f);
    Vec3 perpendicular=to-from*cosine; float sine=length(perpendicular);
    if(sine<1e-6f&&cosine>0) return normalized(mix(from,to,t));
    // Opposite vectors have no unique turning plane. Choose one reproducibly.
    Vec3 side=sine<1e-6f?frame(from).x:perpendicular/sine;
    float angle=std::atan2(sine,cosine)*t;
    return normalized(from*std::cos(angle)+side*std::sin(angle));
}
size_t sampleCount(const Value& v) { size_t count=v.points.size(); for(const auto& s:v.stems) count+=s.samples.size(); return count; }
void charge(const Value& v, Budget& budget, size_t copies=1) { budget.charge(v.mesh.vertices.size()*copies,v.mesh.triangles.size()*copies,sampleCount(v)*copies); }
std::vector<float> distances(const Stem& stem) {
    std::vector<float> d(stem.samples.size());
    for(size_t i=1;i<d.size();++i) d[i]=d[i-1]+length(stem.samples[i].position-stem.samples[i-1].position);
    require(d.back()>1e-8f,"zero-length stem"); return d;
}
Sample at(const Stem& stem, const std::vector<float>& d, float t, Vec3* tangent=nullptr) {
    float target=clamp01(t)*d.back();
    size_t i=std::min<size_t>(std::upper_bound(d.begin(),d.end(),target)-d.begin(),d.size()-1); i=std::max<size_t>(1,i);
    const auto& a=stem.samples[i-1]; const auto& b=stem.samples[i]; float f=(target-d[i-1])/(d[i]-d[i-1]);
    if(tangent) *tangent=normalized(b.position-a.position);
    return {mix(a.position,b.position,f),lerp(a.radius,b.radius,f)};
}
Stem makeStem(Vec3 origin, Vec3 direction, float extent, float radius, const Json& j, Random& random, unsigned depth=0) {
    Stem s; s.depth=depth; int segments=j["segments"]; Vec3 p=origin, dir=normalized(direction),bend=vector3(j["bend"]); float flare=j.value("flare",0.0f);
    for(int i=0;i<=segments;++i) {
        float t=float(i)/segments,root=std::max(0.0f,1-t/j.value("flare_length",0.25f));
        float power=j.value("flare_power",2.0f),falloff=power==2?root*root:std::pow(root,power);
        s.samples.push_back({p+bend*(t*t),radius*(1-n(j,"taper")*t)*(1+flare*falloff)*profileAt(j["radius_profile"],t)});
        if(i<segments) {
            Vec3 perturb{random.signedUnit(),random.signedUnit(),random.signedUnit()};
            dir=normalized(dir+perturb*(n(j,"noise")*profileAt(j["noise_profile"],t)/std::sqrt(float(segments)))); p=p+dir*(extent/segments);
        }
    }
    return s;
}
struct Attachment { float t, azimuth; };
Attachment attachment(const Json& j, int i, int count, Random& random) {
    std::string distribution=str(j,"distribution"); int group=distribution=="opposite"?2:distribution=="whorled"?j["whorl"].get<int>():1;
    int groups=(count+group-1)/group; float t=groups<=1?0.5f:float(i/group)/(groups-1);
    if(distribution=="tip") t=1;
    else if(distribution=="random") t=random.unit();
    else t=clamp01(t+random.signedUnit()*j.value("jitter",0.0f)/std::max(1,groups));
    float azimuth=radians(n(j,"azimuth")*(i/group)+360.0f*(i%group)/group);
    if(distribution=="random") azimuth=2*pi*random.unit();
    if(distribution=="tip") azimuth=2*pi*float(i)/std::max(1,count);
    return {distribution=="tip"?j["range"][1].get<float>():interval(j["range"],t),azimuth};
}
Frame radialFrame(Vec3 tangent, float azimuth, float angle) {
    auto f=frame(tangent); Vec3 outward=f.x*std::cos(azimuth)+f.z*std::sin(azimuth);
    Vec3 side=f.x*std::sin(azimuth)-f.z*std::cos(azimuth),y=normalized(tangent*std::cos(angle)+outward*std::sin(angle));
    return {side,y,normalized(cross(side,y))};
}
void triangle(Mesh& mesh, uint32_t a, uint32_t b, uint32_t c, const std::string& material) { mesh.triangles.push_back({{a,b,c},material}); }
void normals(Mesh& mesh) {
    for(auto& v:mesh.vertices) v.normal={0,0,0};
    for(const auto& t:mesh.triangles) { auto a=t.vertices[0],b=t.vertices[1],c=t.vertices[2]; Vec3 normal=cross(mesh.vertices[b].position-mesh.vertices[a].position,mesh.vertices[c].position-mesh.vertices[a].position); for(auto i:t.vertices) mesh.vertices[i].normal=mesh.vertices[i].normal+normal; }
    for(auto& v:mesh.vertices) v.normal=normalized(v.normal);
}
std::vector<bool> densitySelection(size_t count, size_t retained, uint64_t seed) {
    std::vector<bool> selected;
    if(retained<count) {
        selected.resize(count,false);
        if(retained) {
            Random selection{seed}; std::vector<std::pair<uint64_t,size_t>> ranks; ranks.reserve(count);
            for(size_t i=0;i<count;++i) ranks.emplace_back(selection.next(),i);
            std::nth_element(ranks.begin(),ranks.begin()+retained,ranks.end());
            for(size_t i=0;i<retained;++i) selected[ranks[i].second]=true;
        }
    }
    return selected;
}
void append(Mesh& result, const Mesh& source, const Point& point, Vec3 rotation, float scale, float shade, Vec3 axisScale={1,1,1}, const std::array<float,4>* uvRect=nullptr) {
    uint32_t offset=static_cast<uint32_t>(result.vertices.size()); scale*=point.scale;
    for(auto v:source.vertices) {
        Vec3 position{v.position.x*axisScale.x,v.position.y*axisScale.y,v.position.z*axisScale.z};
        Vec3 normal{v.normal.x/axisScale.x,v.normal.y/axisScale.y,v.normal.z/axisScale.z};
        v.position=point.position+point.orientation.apply(rotate(position*scale,rotation));
        v.normal=normalized(point.orientation.apply(rotate(normal,rotation)));
        if(uvRect) v.uv={lerp((*uvRect)[0],(*uvRect)[2],v.uv.x),lerp((*uvRect)[1],(*uvRect)[3],v.uv.y)};
        for(int i=0;i<3;++i) v.color[i]*=shade;
        v.wind.y=std::fmod(v.wind.y+point.phase,1.0f); result.vertices.push_back(v);
    }
    for(auto t:source.triangles) { for(auto& index:t.vertices) index+=offset; result.triangles.push_back(t); }
}
Mesh loadObj(const std::filesystem::path& path, const std::string& material, Budget& budget) {
    std::ifstream in(path); require(bool(in),"cannot open "+path.string());
    std::vector<Vec3> positions,norm; std::vector<Vec2> uvs; Mesh mesh; std::string line;
    auto index=[](const std::string& s, size_t size) {
        size_t used=0; long long value=std::stoll(s,&used); require(used==s.size()&&value!=0,"invalid OBJ index");
        long long i=value>0?value-1:static_cast<long long>(size)+value; require(i>=0&&i<static_cast<long long>(size),"OBJ index out of range"); return size_t(i);
    };
    while(std::getline(in,line)) {
        std::istringstream row(line); std::string tag; row>>tag;
        if(tag=="v"||tag=="vn") { Vec3 v; require(bool(row>>v.x>>v.y>>v.z)&&finite(v),"invalid OBJ vector"); auto& array=tag=="v"?positions:norm; require(array.size()<budget.limit.points,"OBJ source vector budget exceeded"); array.push_back(v); }
        else if(tag=="vt") { Vec2 uv; require(bool(row>>uv.x>>uv.y)&&std::isfinite(uv.x)&&std::isfinite(uv.y),"invalid OBJ UV"); require(uvs.size()<budget.limit.points,"OBJ source UV budget exceeded"); uvs.push_back(uv); }
        else if(tag=="f") {
            std::vector<Vertex> face; std::vector<bool> hasNormal; std::string token;
            while(row>>token) {
                if(token[0]=='#') break;
                require(face.size()<1024,"OBJ face exceeds 1024 corners");
                std::vector<std::string> parts; size_t start=0;
                for(size_t k=0;k<=token.size();++k) if(k==token.size()||token[k]=='/') { parts.push_back(token.substr(start,k-start)); start=k+1; }
                require(!parts.empty()&&parts.size()<=3&&!parts[0].empty(),"invalid OBJ face"); Vertex v; v.position=positions.at(index(parts[0],positions.size()));
                if(parts.size()>1&&!parts[1].empty()) v.uv=uvs.at(index(parts[1],uvs.size()));
                bool valid=parts.size()>2&&!parts[2].empty(); if(valid) v.normal=normalized(norm.at(index(parts[2],norm.size())));
                face.push_back(v); hasNormal.push_back(valid);
            }
            require(face.size()>=3,"OBJ face needs at least three corners"); budget.charge(face.size(),face.size()-2,0);
            Vec3 normal=normalized(cross(face[1].position-face[0].position,face[2].position-face[0].position));
            uint32_t offset=uint32_t(mesh.vertices.size()); for(size_t i=0;i<face.size();++i) { if(!hasNormal[i]) face[i].normal=normal; mesh.vertices.push_back(face[i]); }
            for(size_t i=1;i+1<face.size();++i) triangle(mesh,offset,offset+uint32_t(i),offset+uint32_t(i+1),material);
        }
    }
    require(!mesh.triangles.empty(),"OBJ has no faces"); return mesh;
}
}
Value evaluate(const Json& j, const Inputs& inputs, Random& random, Budget& budget, const std::filesystem::path& base, Json* diagnostics, const GrowthContext* growth) {
    Value out; std::string op=str(j,"op");
    auto input=[&]() -> const Value& { return *inputs.at(str(j,"input")); };
    if(op=="trunk") {
        out.kind=Kind::skeleton; budget.charge(0,0,j["segments"].get<size_t>()+1);
        out.stems.push_back(makeStem(vector3(j["origin"]),vector3(j["direction"]),n(j,"length"),n(j,"radius"),j,random));
    } else if(op=="curve") {
        out.kind=Kind::skeleton; budget.charge(0,0,j["points"].size()); Stem stem;
        for(const auto& p:j["points"]) stem.samples.push_back({vector3(p),n(j,"radius")});
        auto d=distances(stem); for(size_t i=0;i<d.size();++i) { float t=d[i]/d.back(); stem.samples[i].radius*=(1-n(j,"taper")*d[i]/d.back())*profileAt(j["radius_profile"],t); } out.stems.push_back(stem);
    } else if(op=="branch") {
        out.kind=Kind::skeleton; int count=j["count"]; budget.charge(0,0,input().stems.size()*size_t(count)*(j["segments"].get<size_t>()+1));
        for(const auto& parent:input().stems) {
            auto d=distances(parent); float phase=2*pi*random.unit();
            for(int i=0;i<count;++i) {
                auto a=attachment(j,i,count,random); Vec3 tangent; auto sample=at(parent,d,a.t,&tangent);
                auto f=radialFrame(tangent,a.azimuth+phase,radians(std::clamp(n(j,"angle")+n(j,"angle_jitter")*random.signedUnit(),0.0f,180.0f)));
                float extent=n(j,"length")*(1+n(j,"length_variation")*random.signedUnit())*(1-n(j,"length_decay")*a.t);
                // Start on the centerline so independent branch tubes overlap the parent without a visible gap.
                out.stems.push_back(makeStem(sample.position,f.y,extent,sample.radius*n(j,"radius_scale"),j,random,parent.depth+1));
                out.stems.back().support=parent.growth; out.stems.back().attachment=a.t;
            }
        }
    } else if(op=="roots") {
        out.kind=Kind::skeleton; int count=j["count"],segments=j["segments"];
        budget.charge(0,0,input().stems.size()*size_t(count)*size_t(segments+1));
        for(const auto& parent:input().stems) {
            auto d=distances(parent); auto anchor=at(parent,d,n(j,"attachment"));
            for(int i=0;i<count;++i) {
                float angle=radians(n(j,"angle")+360.0f*i/count+n(j,"angle_jitter")*random.signedUnit());
                Vec3 outward{std::cos(angle),0,std::sin(angle)},side{-outward.z,0,outward.x};
                float extent=n(j,"length")*(1+n(j,"length_variation")*random.signedUnit());
                float radius=anchor.radius*n(j,"radius_scale")*(1+n(j,"radius_variation")*random.signedUnit());
                uint64_t noiseSeed=random.next(); Stem root; root.depth=parent.depth+1; root.support=parent.growth; root.attachment=n(j,"attachment");
                root.samples.reserve(size_t(segments+1));
                for(int k=0;k<=segments;++k) {
                    float t=float(k)/segments,u=clamp01(t/n(j,"surface_at"));
                    Vec3 position=anchor.position+outward*(extent*t);
                    float noise=coherentNoise({extent*t,0,0},n(j,"noise_scale"),noiseSeed)*n(j,"noise")*profileAt(j["noise_profile"],t)*std::min(1.0f,8*t);
                    position=position+side*noise;
                    position.y=n(j,"ground_height")+(anchor.position.y-n(j,"ground_height"))*(1-u)*(1-u)-n(j,"bury_depth")*t*t;
                    if(k==0) position=anchor.position;
                    root.samples.push_back({position,radius*(1-n(j,"taper")*t)*profileAt(j["radius_profile"],t)});
                }
                out.stems.push_back(std::move(root));
            }
        }
    } else if(op=="grow") {
        out.kind=Kind::skeleton;
        for(const auto& source:input().stems) {
            auto d=distances(source); Stem stem; stem.depth=source.depth; stem.growth=source.growth; stem.support=source.support; stem.attachment=source.attachment; Vec3 position=source.samples[0].position,previous=position;
            float total=d.back()*n(j,"amount");
            budget.charge(0,0,1); stem.samples.push_back({position,source.samples[0].radius*n(j,"radius_scale")});
            for(size_t i=1;i<d.size();++i) {
                float t=std::min(d[i],total)/total;
                auto sample=d[i]<=total?source.samples[i]:at(source,d,n(j,"amount"));
                Vec3 delta=sample.position-previous;
                float attraction=n(j,"strength")*profileAt(j["strength_profile"],t);
                Vec3 from=normalized(delta),to=normalized(vector3(j["direction"]));
                Vec3 attracted=str(j,"interpolation")=="spherical"?sphericalDirection(from,to,attraction):normalized(mix(from,to,attraction));
                position=position+attracted*length(delta); previous=sample.position;
                budget.charge(0,0,1); stem.samples.push_back({position,sample.radius*n(j,"radius_scale")});
                if(d[i]>=total) break;
            }
            out.stems.push_back(stem);
        }
    } else if(op=="prune") {
        out.kind=Kind::skeleton;
        for(const auto& s:input().stems) if((length(s.samples.back().position-vector3(j["center"]))<=n(j,"radius"))==j["inside"].get<bool>()) { budget.charge(0,0,s.samples.size()); out.stems.push_back(s); }
    } else if(op=="tube") {
        int sides=j["sides"],stride=j["stride"]; bool caps=j["caps"]; auto& mesh=out.mesh;
        uint64_t noiseSeed=random.next();
        std::vector<std::pair<uint32_t,uint32_t>> seams;
        for(const auto& stem:input().stems) {
            auto visible=visibleSamples(stem); if(visible.size()<2) continue;
            std::vector<Sample> samples; for(size_t i=0;i<visible.size();i+=stride) samples.push_back(visible[i]);
            if((visible.size()-1)%stride) samples.push_back(visible.back());
            size_t rows=samples.size(); budget.charge(rows*size_t(sides+1)+(caps?2*size_t(sides+1):0),(rows-1)*size_t(sides)*2+(caps?2*size_t(sides):0),0);
            uint32_t offset=uint32_t(mesh.vertices.size()); float distance=0,total=0; Frame f; std::vector<Frame> frames;
            for(size_t i=1;i<rows;++i) total+=length(samples[i].position-samples[i-1].position);
            if(stem.growth) total=distances(stem).back();
            for(size_t i=0;i<rows;++i) {
                Vec3 tangent=normalized(samples[std::min(i+1,rows-1)].position-samples[i?i-1:0].position);
                if(i==0) f=frame(tangent);
                else { Vec3 x=f.x-tangent*dot(f.x,tangent); if(length(x)<1e-6f) x=frame(tangent).x; f={normalized(x),tangent,normalized(cross(normalized(x),tangent))}; distance+=length(samples[i].position-samples[i-1].position); }
                frames.push_back(f); float ring=1;
                float t=i==rows-1&&!stem.growth?1:distance/total;
                if(n(j,"node_spacing")>0) ring+=n(j,"node_strength")*std::pow(std::max(0.0f,std::cos(2*pi*distance/n(j,"node_spacing"))),16.0f);
                for(int k=0;k<=sides;++k) {
                    float angle=2*pi*float(k%sides)/sides,radius=samples[i].radius*ring*(1+n(j,"ridge_depth")*profileAt(j["ridge_profile"],t)*(j["ridges"].get<int>()?std::cos(angle*n(j,"ridges")):0));
                    Vec3 normal=f.x*std::cos(angle)+f.z*std::sin(angle);
                    if(n(j,"radius_noise")>0) radius*=1+n(j,"radius_noise")*profileAt(j["noise_profile"],t)*layeredNoise(samples[i].position+normal*samples[i].radius,n(j,"noise_scale"),j["noise_octaves"],noiseSeed);
                    Vertex v; v.position=samples[i].position+normal*radius; v.normal=normal; v.uv={float(k)/sides*j["uv_scale"][0].get<float>(),distance*j["uv_scale"][1].get<float>()}; mesh.vertices.push_back(v);
                }
                seams.push_back({offset+uint32_t(i*(sides+1)),offset+uint32_t(i*(sides+1)+sides)});
                if(i>0) for(int k=0;k<sides;++k) { uint32_t a=offset+uint32_t((i-1)*(sides+1)+k),b=a+sides+1; triangle(mesh,a,b,a+1,str(j,"material")); triangle(mesh,a+1,b,b+1,str(j,"material")); }
            }
            if(caps) for(int end=0;end<2;++end) {
                size_t row=end?rows-1:0; uint32_t center=uint32_t(mesh.vertices.size()); Vertex v; v.position=samples[row].position; v.normal=frames[row].y*(end?1.0f:-1.0f); v.uv={0.5f,0.5f}; mesh.vertices.push_back(v);
                for(int k=0;k<sides;++k) { v.position=mesh.vertices[offset+row*(sides+1)+k].position; float angle=2*pi*k/sides; v.uv={0.5f+0.5f*std::cos(angle),0.5f+0.5f*std::sin(angle)}; mesh.vertices.push_back(v); }
                for(int k=0;k<sides;++k) { uint32_t a=center+1+k,b=center+1+(k+1)%sides; if(end) triangle(mesh,center,b,a,str(j,"cap_material")); else triangle(mesh,center,a,b,str(j,"cap_material")); }
            }
        }
        normals(mesh); for(auto [a,b]:seams) { Vec3 normal=normalized(mesh.vertices[a].normal+mesh.vertices[b].normal); mesh.vertices[a].normal=mesh.vertices[b].normal=normal; }
    } else if(op=="ribbon") {
        int planes=j["planes"],stride=j["stride"]; Atlas atlas;
        if(j.contains("atlas")) atlas=readAtlas(base/j["atlas"].get<std::string>());
        Random cells{random.state^hashName("ribbon.atlas")}; size_t placement=0; auto& mesh=out.mesh;
        size_t kept=size_t(std::ceil(input().stems.size()*j["density"].get<double>()));
        auto selected=densitySelection(input().stems.size(),kept,random.state^hashName("ribbon.density"));
        for(const auto& stem:input().stems) {
            float roll=radians(n(j,"rotation")+random.signedUnit()*n(j,"rotation_jitter")),shade=1-random.unit()*n(j,"color_variation");
            std::array<float,4> rect{0,0,1,1};
            if(!atlas.regions.empty()) {
                size_t index=str(j,"atlas_mode")=="fixed"?j["atlas_index"].get<size_t>():str(j,"atlas_mode")=="cycle"?placement%atlas.regions.size():size_t(cells.unit()*atlas.regions.size());
                rect=atlas.regions.at(index);
            }
            bool keep=selected.empty()||selected[placement]; ++placement;
            if(!keep) continue;
            auto visible=visibleSamples(stem); if(visible.size()<2) continue;
            std::vector<Sample> samples;
            for(size_t i=0;i<visible.size();i+=stride) samples.push_back(visible[i]);
            if((visible.size()-1)%stride) samples.push_back(visible.back());
            size_t rows=samples.size(); budget.charge(rows*2*size_t(planes),(rows-1)*2*size_t(planes),0);
            Stem retained; retained.samples=samples; retained.depth=stem.depth; auto arc=distances(retained); Frame f; std::vector<Frame> frames; frames.reserve(rows);
            for(size_t i=0;i<rows;++i) {
                Vec3 tangent=normalized(samples[std::min(i+1,rows-1)].position-samples[i?i-1:0].position);
                if(i==0) f=frame(tangent,roll);
                else { Vec3 x=f.x-tangent*dot(f.x,tangent); if(length(x)<1e-6f) x=frame(tangent,roll).x; f={normalized(x),tangent,normalized(cross(normalized(x),tangent))}; }
                frames.push_back(f);
            }
            for(int plane=0;plane<planes;++plane) {
                uint32_t offset=uint32_t(mesh.vertices.size()); float angle=pi*plane/planes;
                for(size_t i=0;i<rows;++i) {
                    float t=stem.growth?arc[i]/distances(stem).back():(i==rows-1?1:arc[i]/arc.back()),half=n(j,"width")*.5f*profileAt(j["width_profile"],t);
                    Vec3 axis=frames[i].x*std::cos(angle)+frames[i].z*std::sin(angle);
                    for(int side=0;side<2;++side) {
                        Vertex v; v.position=samples[i].position+axis*(side?half:-half); v.uv={rect[side?2:0],lerp(rect[1],rect[3],t)};
                        v.color={shade,shade,shade,1}; mesh.vertices.push_back(v);
                    }
                    if(i) { uint32_t a=offset+uint32_t((i-1)*2); triangle(mesh,a,a+1,a+2,str(j,"material")); triangle(mesh,a+1,a+3,a+2,str(j,"material")); }
                }
            }
        }
        normals(mesh);
        if(diagnostics) (*diagnostics)["ribbons"]={{"candidates",placement},{"paths",kept},{"planes",planes},{"stride",stride}};
    } else if(op=="scatter") {
        out.kind=Kind::points; int count=j["count"]; size_t copies=j.contains("input")&&input().kind==Kind::skeleton?input().stems.size():1; budget.charge(0,0,size_t(count)*copies);
        std::optional<MeshProximity> proximity;
        std::optional<MeshProximity> matureProximity;
        if(growth&&growth->mature_proximity&&!growth->mature_proximity->triangles.empty()) matureProximity.emplace(*growth->mature_proximity);
        bool emptySupport=j.contains("proximity_mesh")&&inputs.at(str(j,"proximity_mesh"))->mesh.triangles.empty();
        if(count&&copies&&j.contains("proximity_mesh")&&!emptySupport) proximity.emplace(inputs.at(str(j,"proximity_mesh"))->mesh);
        size_t normalAccepted=0;
        auto add=[&](Vec3 position, Frame orientation, bool facing=true, double available=0, bool visible=true, bool developing=false, std::optional<Vec3> maturePosition={}) {
            // Consume exactly the original candidate stream even when a point is rejected.
            Point point{position,orientation,interval(j["scale"],random.unit()),random.unit()};
            point.available_at=available; point.growth_visible=visible&&!emptySupport; point.developing=developing;
            if(!facing) return;
            ++normalAccepted;
            if(emptySupport&&!growth) return;
            // Keep the mature candidate membership and random-stream order fixed.
            // Current support visibility only hides candidates, never renumbers them.
            if(matureProximity&&maturePosition&&!matureProximity->nearest(*maturePosition,n(j,"max_distance"))) return;
            if(proximity) {
                auto hit=proximity->nearest(position,n(j,"max_distance"));
                if(!hit) { if(!growth) return; point.growth_visible=false; }
                if(hit&&j["snap_to_mesh"].get<bool>()) point.position=hit->position;
            }
            out.points.push_back(point);
        };
        if(j.contains("input")&&input().kind==Kind::skeleton) {
            for(const auto& stem:input().stems) { auto d=distances(stem); float phase=2*pi*random.unit();
                for(int i=0;i<count;++i) { auto a=attachment(j,i,count,random); Vec3 tangent; auto sample=at(stem,d,a.t,&tangent); float phi=a.azimuth+phase;
                    auto f=radialFrame(tangent,phi,radians(std::clamp(n(j,"angle")+random.signedUnit()*n(j,"jitter")*30,0.0f,180.0f)));
                    auto local=frame(tangent); Vec3 radial=local.x*std::cos(phi)+local.z*std::sin(phi);
                    double available=growthReach(stem.growth,a.t);
                    bool visible=!stem.growth||(stem.growth->visible&&a.t<=stem.growth->visible_length);
                    add(sample.position+radial*(sample.radius*growthRadius(stem.growth,a.t)+n(j,"offset")),f,true,available,visible,bool(stem.growth),sample.position+radial*(sample.radius+n(j,"offset")));
                }
            }
        } else if(j.contains("input")) {
            const auto& mesh=input().mesh; std::vector<double> areas; double total=0;
            for(const auto& face:mesh.triangles) { Vec3 a=mesh.vertices[face.vertices[0]].position,b=mesh.vertices[face.vertices[1]].position,c=mesh.vertices[face.vertices[2]].position; total+=length(cross(b-a,c-a))*0.5; areas.push_back(total); }
            require(growth||count==0||total>0,"cannot scatter on an empty mesh");
            for(int i=0;i<count&&total>0;++i) {
                size_t face=std::min<size_t>(std::upper_bound(areas.begin(),areas.end(),random.unit()*total)-areas.begin(),areas.size()-1);
                const auto& tri=mesh.triangles[face]; const auto& a=mesh.vertices[tri.vertices[0]]; const auto& b=mesh.vertices[tri.vertices[1]]; const auto& c=mesh.vertices[tri.vertices[2]];
                float u=std::sqrt(random.unit()),v=random.unit(); Vec3 p=a.position*(1-u)+b.position*(u*(1-v))+c.position*(u*v);
                Vec3 normal=normalized(a.normal*(1-u)+b.normal*(u*(1-v))+c.normal*(u*v));
                bool facing=!j.contains("normal_direction")||dot(normal,normalized(vector3(j["normal_direction"])))>=std::cos(radians(n(j,"max_surface_angle")))-1e-6f;
                add(p+normal*n(j,"offset"),radialFrame(normal,2*pi*random.unit(),radians(n(j,"angle"))),facing);
            }
        } else for(int i=0;i<count;++i) { float a=2*pi*random.unit(),r=n(j,"radius")*std::sqrt(random.unit()); add({r*std::cos(a),n(j,"offset"),r*std::sin(a)},frame({0,1,0},2*pi*random.unit())); }
        if(diagnostics&&j.contains("normal_direction")) (*diagnostics)["normal_filter"]={{"candidates",size_t(count)*copies},{"accepted",normalAccepted},{"rejected",size_t(count)*copies-normalAccepted}};
    } else if(op=="radial") {
        out.kind=Kind::points; int count=j["count"]; budget.charge(0,0,count);
        for(int i=0;i<count;++i) {
            float t=count<=1?0:float(i)/(count-1),phi=radians(n(j,"angle")+i*(str(j,"mode")=="ring"?360.0f/std::max(1,count):n(j,"angle_step"))+random.signedUnit()*n(j,"jitter")*10);
            float radius=interval(j["radius"],t)*(1+random.signedUnit()*n(j,"jitter")); Vec3 p=vector3(j["center"])+Vec3{radius*std::cos(phi),interval(j["height"],t),radius*std::sin(phi)};
            out.points.push_back({p,radialFrame({0,1,0},phi,radians(interval(j["tilt"],t))),interval(j["scale"],t)*(1+random.signedUnit()*n(j,"jitter")),random.unit()});
        }
    } else if(op=="leaf") {
        int segments=j["segments"],across=j["width_segments"],columns=across+1; auto& mesh=out.mesh;
        budget.charge(size_t(segments-1)*columns+2,size_t(segments-1)*across*2,0);
        std::string shape=str(j,"shape");
        auto point=[&](float t, float x, float u, float halfWidth) {
            Vertex v;
            float folded=shape=="petal"&&halfWidth>0?x*x/halfWidth:std::abs(x);
            float z=n(j,"curl")*n(j,"length")*t*t+folded*n(j,"fold");
            if(n(j,"edge_wave")!=0) z+=n(j,"edge_wave")*halfWidth*std::pow(2*u-1,2)*std::sin(2*pi*n(j,"edge_frequency")*t+(2*u-1)*.8f);
            if(n(j,"lateral_bend")!=0) x+=n(j,"lateral_bend")*n(j,"length")*t*t;
            v.position=rotate({x,n(j,"length")*t,z},{0,n(j,"twist")*t,0}); v.uv={u,t}; mesh.vertices.push_back(v);
        };
        point(0,0,0.5f,0);
        for(int i=1;i<segments;++i) {
            float t=float(i)/segments,width=std::sin(pi*t);
            if(shape=="lanceolate") width=std::pow(width,1.4f)*(1.4f-0.8f*t);
            if(shape=="needle") width=std::pow(width,0.35f);
            if(shape=="petal") width=std::pow(width,0.5f)*(0.45f+0.9f*t);
            if(j.contains("width_profile")) width=profileAt(j["width_profile"],t);
            float half=0.5f*n(j,"width")*width;
            for(int k=0;k<=across;++k) { float u=float(k)/across; point(t,lerp(-half,half,u),u,half); }
        }
        point(1,0,0.5f,0); std::string mat=str(j,"material");
        for(int k=0;k<across;++k) triangle(mesh,0,2+k,1+k,mat);
        for(int i=0;i<segments-2;++i) for(int k=0;k<across;++k) { uint32_t a=1+columns*i+k,b=a+1,c=a+columns; triangle(mesh,a,b,c,mat); triangle(mesh,b,c+1,c,mat); }
        uint32_t last=uint32_t(mesh.vertices.size()-1),row=last-columns;
        for(int k=0;k<across;++k) triangle(mesh,row+k,row+k+1,last,mat);
        normals(mesh);
    } else if(op=="card") {
        int planes=j["planes"],segments=j["segments"],across=j["width_segments"],columns=across+1;
        budget.charge(size_t(planes)*(segments+1)*columns,size_t(planes)*segments*across*2,0);
        float half=n(j,"width")*0.5f,height=n(j,"height"),y=str(j,"pivot")=="base"?0:-height*0.5f;
        for(int i=0;i<planes;++i) {
            uint32_t offset=uint32_t(out.mesh.vertices.size()); Vec3 rotation{0,180.0f*i/planes,0};
            auto point=[&](float u, float v) {
                float x=lerp(-half,half,u),py=y+height*v,t=py/height;
                Vec3 p{x,py,n(j,"curl")*height*t*t+n(j,"fold")*x*x/half};
                Vertex vertex; vertex.position=rotate(rotate(p,{0,n(j,"twist")*t,0}),rotation); vertex.normal=rotate({0,0,1},rotation);
                vertex.uv={lerp(j["uv_rect"][0].get<float>(),j["uv_rect"][2].get<float>(),u),lerp(j["uv_rect"][1].get<float>(),j["uv_rect"][3].get<float>(),v)};
                out.mesh.vertices.push_back(vertex);
            };
            if(segments==1&&across==1) {
                // Preserve the original four-vertex ordering and diagonal.
                point(0,0); point(1,0); point(1,1); point(0,1);
                triangle(out.mesh,offset,offset+1,offset+2,str(j,"material")); triangle(out.mesh,offset,offset+2,offset+3,str(j,"material"));
            } else {
                for(int row=0;row<=segments;++row) for(int k=0;k<=across;++k) point(float(k)/across,float(row)/segments);
                for(int row=0;row<segments;++row) for(int k=0;k<across;++k) {
                    uint32_t a=offset+uint32_t(row*columns+k),b=a+1,c=a+columns;
                    triangle(out.mesh,a,b,c+1,str(j,"material")); triangle(out.mesh,a,c+1,c,str(j,"material"));
                }
            }
        }
        if(n(j,"curl")!=0||n(j,"fold")!=0||n(j,"twist")!=0) normals(out.mesh);
    } else if(op=="ellipsoid") {
        int rings=j["rings"],sides=j["sides"]; auto& m=out.mesh; Vec3 r=vector3(j["radii"]); std::string material=str(j,"material");
        budget.charge(size_t(rings-1)*(sides+1)+2,size_t(rings-1)*sides*2,0);
        auto add=[&](Vec3 p, Vec2 uv) { Vertex v; v.position={p.x*r.x,p.y*r.y,p.z*r.z}; v.normal=normalized({p.x/r.x,p.y/r.y,p.z/r.z}); v.uv=uv; m.vertices.push_back(v); };
        add({0,-1,0},{0.5f,0});
        for(int i=1;i<rings;++i) for(int k=0;k<=sides;++k) { float t=float(i)/rings,a=2*pi*float(k%sides)/sides; add({std::sin(pi*t)*std::cos(a),-std::cos(pi*t),std::sin(pi*t)*std::sin(a)},{float(k)/sides,t}); }
        uint32_t top=uint32_t(m.vertices.size()); add({0,1,0},{0.5f,1});
        for(int k=0;k<sides;++k) { triangle(m,0,1+k,2+k,material); uint32_t a=1+(rings-2)*(sides+1)+k; triangle(m,top,a+1,a,material); }
        for(int i=0;i<rings-2;++i) for(int k=0;k<sides;++k) { uint32_t a=1+i*(sides+1)+k,b=a+sides+1; triangle(m,a,b,a+1,material); triangle(m,a+1,b,b+1,material); }
    } else if(op=="rock"||op=="crystal") {
        out.mesh=mineralMesh(j,random,budget);
    } else if(op=="orient") {
        charge(input(),budget); out=input(); Vec3 up=normalized(vector3(j["up"]));
        for(auto& p:out.points) {
            std::string mode=str(j,"mode");
            if(mode!="keep") {
                Vec3 direction=mode=="fixed"?vector3(j["direction"]):p.position-vector3(j["center"]);
                if(mode=="inward") direction=direction*-1;
                if(j["horizontal"].get<bool>()&&mode!="fixed") direction=direction-up*dot(direction,up);
                if(length(direction)<1e-7f) {
                    direction=vector3(j["direction"]);
                    if(j["horizontal"].get<bool>()&&mode!="fixed") { direction=direction-up*dot(direction,up); if(length(direction)<1e-7f) direction=frame(up).x; }
                }
                Vec3 z=normalized(direction),x=cross(up,z);
                if(length(x)<1e-7f) x=frame(z).x;
                x=normalized(x); p.orientation={x,normalized(cross(z,x)),z};
            }
            Vec3 variation=vector3(j["rotation_jitter"]);
            Vec3 angles{variation.x*random.signedUnit(),variation.y*random.signedUnit(),variation.z*random.signedUnit()};
            Frame f=p.orientation;
            p.orientation={f.apply(rotate({1,0,0},angles)),f.apply(rotate({0,1,0},angles)),f.apply(rotate({0,0,1},angles))};
        }
    } else if(op=="instance") {
        const auto& source=input(); const auto& points=inputs.at(str(j,"points"))->points;
        size_t retained=size_t(std::ceil(points.size()*j["density"].get<double>())); charge(source,budget,retained);
        auto selected=densitySelection(points.size(),retained,random.state^hashName("instance.density"));
        // A separate stream keeps existing color variation stable when size changes.
        Random sizes{random.state^hashName("instance.scale_jitter")}; Vec3 jitter=vector3(j["scale_jitter"]);
        Atlas atlas; Random cells{random.state^hashName("instance.atlas")}; size_t placement=0;
        if(j.contains("atlas")) {
            atlas=readAtlas(base/j["atlas"].get<std::string>());
            for(const auto& v:source.mesh.vertices) require(v.uv.x>=0&&v.uv.x<=1&&v.uv.y>=0&&v.uv.y<=1,"atlas source UVs must be within 0..1");
        }
        float feature=std::numeric_limits<float>::max();
        if(growth) for(const auto& face:source.mesh.triangles) {
            Vec3 a=source.mesh.vertices[face.vertices[0]].position,b=source.mesh.vertices[face.vertices[1]].position,c=source.mesh.vertices[face.vertices[2]].position;
            float longest=std::max({length(b-a),length(c-a),length(c-b)});
            feature=std::min(feature,length(cross(b-a,c-a))/longest);
        }
        size_t emitted=0;
        for(const auto& point:points) {
            Vec3 scale{1+jitter.x*sizes.signedUnit(),1+jitter.y*sizes.signedUnit(),1+jitter.z*sizes.signedUnit()};
            const std::array<float,4>* uvRect=nullptr;
            if(!atlas.regions.empty()) {
                size_t index=str(j,"atlas_mode")=="fixed"?j["atlas_index"].get<size_t>():str(j,"atlas_mode")=="cycle"?placement%atlas.regions.size():size_t(cells.unit()*atlas.regions.size());
                require(index<atlas.regions.size(),"atlas_index is not an occupied cell"); uvRect=&atlas.regions[index];
            }
            float color=1-random.unit()*n(j,"color_variation");
            float development=growth&&(growth->stage||point.developing)?growthScale(growth->stage,growth->progress,point.available_at):1;
            bool visible=!growth||point.growth_visible;
            if(growth&&feature*development*point.scale*n(j,"scale")*std::min({scale.x,scale.y,scale.z})<geometryResolution(point.position)*4) visible=false;
            if((selected.empty()||selected[placement])&&visible&&development>0) { append(out.mesh,source.mesh,point,vector3(j["rotation"]),n(j,"scale")*development,color,scale,uvRect); ++emitted; }
            ++placement;
        }
        if(diagnostics) (*diagnostics)["instances"]={{"candidates",points.size()},{"retained",retained},{"density",j["density"]},{"visible",emitted}};
    } else if(op=="transform") {
        charge(input(),budget); out=input(); float scale=n(j,"scale"); Vec3 rotation=vector3(j["rotation"]),translation=vector3(j["translation"]);
        for(auto& stem:out.stems) for(auto& s:stem.samples) { s.position=rotate(s.position*scale,rotation)+translation; s.radius*=scale; }
        for(auto& p:out.points) { p.position=rotate(p.position*scale,rotation)+translation; p.orientation={rotate(p.orientation.x,rotation),rotate(p.orientation.y,rotation),rotate(p.orientation.z,rotation)}; p.scale*=scale; }
        for(auto& v:out.mesh.vertices) { v.position=rotate(v.position*scale,rotation)+translation; v.normal=normalized(rotate(v.normal,rotation)); }
    } else if(op=="solidify") {
        out.mesh=solidify(input().mesh,j,budget,diagnostics);
    } else if(op=="boolean") {
        out.mesh=booleanMesh(input().mesh,inputs.at(str(j,"tool"))->mesh,j,budget,diagnostics);
    } else if(op=="merge") {
        out.kind=inputs.at(j["inputs"][0].get<std::string>())->kind;
        for(const auto& ref:j["inputs"]) { const auto& v=*inputs.at(ref.get<std::string>()); charge(v,budget); out.stems.insert(out.stems.end(),v.stems.begin(),v.stems.end()); out.points.insert(out.points.end(),v.points.begin(),v.points.end()); append(out.mesh,v.mesh,Point{},Vec3{},1,1); }
    } else if(op=="material") {
        charge(input(),budget); out=input(); for(auto& face:out.mesh.triangles) face.material=str(j,"name");
    } else if(op=="wind") {
        charge(input(),budget); out=input(); float phase=random.unit();
        for(auto& v:out.mesh.vertices) { v.wind.x=n(j,"strength")*std::pow(clamp01((v.position.y-n(j,"base"))/n(j,"height")),n(j,"exponent")); v.wind.y=std::fmod(v.wind.y+phase,1.0f); }
    } else if(op=="mesh") out.mesh=loadObj(base/str(j,"path"),str(j,"material"),budget);
    else throw std::runtime_error("unimplemented op '"+op+"'");
    return out;
}
}
