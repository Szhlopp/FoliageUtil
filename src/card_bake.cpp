// CPU material sampling, barycentric rasterization and padding adapted from
// TexUtil src/model.cpp and src/export_bake.cpp (MIT, Szhlopp, 2026).
// Fitted source-to-card projection and export packaging are FoliageUtil code.
#include "internal.hpp"
#include "proximity.hpp"
#include <png.h>
#include <cctype>
#include <fstream>
#include <functional>
#include <numeric>
#include <set>

namespace foliage {
Json cardBakeFields() {
    auto integer=[](int value,int lo,int hi,const char* help) { return Json{{"type","integer"},{"default",value},{"min",lo},{"max",hi},{"description",help}}; };
    auto number=[](float value,float lo,float hi,const char* help) { return Json{{"type","number"},{"default",value},{"min",lo},{"max",hi},{"description",help}}; };
    return {
        {"source",{{"type","reference"},{"required",true},{"description","Mesh containing only the leaves and fine twigs to replace. Connected components attach to the closest path at their first indexed vertex."}}},
        {"paths",{{"type","reference"},{"required",true},{"description","Skeleton whose individual strands guide the fitted cards, in the same coordinate system as source."}}},
        {"keep",{{"type","reference"},{"description","Optional mesh to retain unchanged, such as welded trunk and branches."}}},
        {"output",{{"type","string"},{"required",true},{"description","Relative .glb filename. Atlas PNGs and a manifest are exported beside it in a matching .cardbake directory."}}},
        {"segments",integer(6,1,64,"Longitudinal segments per fitted card; lower values approximate curvature more coarsely.")},
        {"planes",integer(2,1,2,"One fitted strip or two crossed strips per occupied path. Each strip gets its own unique bake cell.")},
        {"cell_width",integer(64,32,1024,"Atlas cell width including padding, in pixels.")},
        {"cell_height",integer(256,32,2048,"Atlas cell height including padding, in pixels.")},
        {"atlas_size",integer(2048,128,8192,"Square atlas page size; unique cells spill into additional pages, up to 64.")},
        {"padding",integer(4,1,32,"Transparent gutter and RGB/normal dilation distance in texels; alpha is never dilated.")},
        {"samples",integer(2,1,3,"Supersampling grid per pixel: 1, 2x2 or 3x3. No random sampling.")},
        {"margin",number(.01f,0,1,"Extra fitted width and tip margins in meters.")},
        {"max_distance",number(.05f,.00001f,100,"Maximum component attachment distance from a guide path, in meters. Unmatched components fail instead of disappearing.")},
        {"alpha_cutoff",number(.4f,.01f,.99f,"MASK cutoff on the exported baked material. Source MASK cutoffs are respected during capture.")},
        {"translucency",number(.24f,0,1,"Uniform baked-card diffuse translucency for the existing Blender preview adapter.")},
        {"memory_mb",integer(1024,64,16384,"Working memory estimate limit for generated meshes, decoded PNGs, cell raster and one atlas page. Not a total process cap.")}
    };
}
void validateCardBakes(Graph& graph) {
    auto& doc=graph.document; if(!doc.contains("card_bakes")) return;
    auto& profiles=doc["card_bakes"]; require(profiles.is_object()&&!profiles.empty()&&profiles.size()<=8,"card_bakes needs 1..8 named profiles");
    std::set<std::string> names;
    for(auto it=profiles.begin();it!=profiles.end();++it) {
        auto name=it.key(),lower=name;
        require(!name.empty()&&name.size()<=32&&name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")==std::string::npos,"invalid CardBake name: "+name);
        for(char& c:lower) c=char(std::tolower(static_cast<unsigned char>(c)));
        require(names.insert(lower).second,"duplicate CardBake name: "+name);
        auto& p=it.value(); validateFields(p,cardBakeFields(),"CardBake '"+name+"'");
        for(auto key:{"source","paths","keep"}) if(p.contains(key)) { auto id=p[key].get<std::string>(); require(graph.kinds.count(id)&&graph.kinds.at(id)==(std::string(key)=="paths"?Kind::skeleton:Kind::mesh),"CardBake "+std::string(key)+": incompatible or unknown node '"+id+"'"); }
        auto output=p["output"].get<std::string>(); auto path=std::filesystem::path(output);
        require(!path.is_absolute()&&path.extension()==".glb"&&output.find_first_of("\\\t")==std::string::npos,"CardBake output must be a portable relative .glb path");
        for(const auto& part:path) require(part!="."&&part!="..","CardBake output cannot contain . or ..");
        require(p["cell_width"].get<int>()>2*p["padding"].get<int>()+2&&p["cell_height"].get<int>()>2*p["padding"].get<int>()+2,"CardBake padding leaves no cell interior");
        require(p["cell_width"]<=p["atlas_size"]&&p["cell_height"]<=p["atlas_size"],"CardBake cell does not fit atlas_size");
    }
}
namespace {
using Pixel=std::array<float,4>;
using Bytes=std::vector<uint8_t>;
float linear(float x) { return x<=.04045f?x/12.92f:std::pow((x+.055f)/1.055f,2.4f); }
float srgb(float x) { x=clamp01(x); return x<=.0031308f?x*12.92f:1.055f*std::pow(x,1/2.4f)-.055f; }
uint8_t byte(float x) { return uint8_t(std::lround(clamp01(x)*255)); }
struct Memory {
    size_t used=0, limit=0;
    void reserve(size_t bytes) { require(bytes<=limit-used,"CardBake memory budget exceeded; increase memory_mb or reduce atlas/cell size"); used+=bytes; }
};
struct Image {
    int width=0,height=0; Bytes pixels;
    Image(const std::filesystem::path& path, Memory& memory) {
        png_image image{}; image.version=PNG_IMAGE_VERSION;
        require(png_image_begin_read_from_file(&image,path.string().c_str()),"CardBake needs readable PNG textures: "+path.string());
        try {
            require(image.width>0&&image.height>0&&image.width<=16384&&image.height<=16384,"CardBake texture dimensions exceed 16384");
            image.format=PNG_FORMAT_RGBA; memory.reserve(PNG_IMAGE_SIZE(image)); pixels.resize(PNG_IMAGE_SIZE(image));
            require(png_image_finish_read(&image,nullptr,pixels.data(),0,nullptr),"CardBake cannot decode PNG: "+path.string());
            width=int(image.width); height=int(image.height);
        } catch(...) { png_image_free(&image); throw; }
        png_image_free(&image);
    }
    Pixel sample(Vec2 uv, bool color) const {
        float px=(uv.x-std::floor(uv.x))*width-.5f,py=(1-uv.y-std::floor(1-uv.y))*height-.5f;
        int x=int(std::floor(px)),y=int(std::floor(py)); float fx=px-x,fy=py-y; Pixel result{};
        for(int j=0;j<2;++j) for(int i=0;i<2;++i) {
            size_t index=(size_t((y+j+height)%height)*width+(x+i+width)%width)*4;
            float w=(i?fx:1-fx)*(j?fy:1-fy);
            for(int k=0;k<4;++k) { float v=pixels[index+k]/255.f; result[k]+=(color&&k<3?linear(v):v)*w; }
        }
        return result;
    }
};
void png(const std::filesystem::path& path, int width, int height, const Bytes& bytes) {
    png_image image{}; image.version=PNG_IMAGE_VERSION; image.width=unsigned(width); image.height=unsigned(height); image.format=PNG_FORMAT_RGBA;
    require(png_image_write_to_file(&image,path.string().c_str(),0,bytes.data(),0,nullptr),"CardBake cannot write PNG: "+path.string());
}
void jsonFile(const std::filesystem::path& path, const Json& json) { std::ofstream file(path); require(bool(file),"cannot write "+path.string()); file<<json.dump(2)<<'\n'; file.close(); require(bool(file),"failed writing "+path.string()); }
struct Material { Pixel color; float roughness=1,metallic=0,cutoff=.5f; bool mask=false; const Image* albedo=nullptr; const Image* normal=nullptr; const Image* packed=nullptr; };
struct Surface { Vec3 color{},normal{0,0,1}; float roughness=1,metallic=0,alpha=0; };
Surface sample(const Mesh& mesh, const Triangle& face, const Material& material, float a, float b, float c) {
    const auto& va=mesh.vertices[face.vertices[0]]; const auto& vb=mesh.vertices[face.vertices[1]]; const auto& vc=mesh.vertices[face.vertices[2]];
    Vec2 uv{va.uv.x*a+vb.uv.x*b+vc.uv.x*c,va.uv.y*a+vb.uv.y*b+vc.uv.y*c};
    Pixel color=material.albedo?material.albedo->sample(uv,true):Pixel{1,1,1,1};
    for(int k=0;k<4;++k) color[k]*=material.color[k]*(va.color[k]*a+vb.color[k]*b+vc.color[k]*c);
    Surface out; out.alpha=material.mask?(color[3]>=material.cutoff?1.f:0.f):1.f; out.color={color[0],color[1],color[2]};
    out.roughness=material.roughness; out.metallic=material.metallic;
    if(material.packed) { auto q=material.packed->sample(uv,false); out.roughness*=q[1]; out.metallic*=q[2]; }
    out.normal=normalized(va.normal*a+vb.normal*b+vc.normal*c);
    if(material.normal) {
        auto q=material.normal->sample(uv,false); Vec3 n=normalized({2*q[0]-1,2*q[1]-1,2*q[2]-1});
        Vec3 e1=vb.position-va.position,e2=vc.position-va.position;
        Vec2 d1{vb.uv.x-va.uv.x,vb.uv.y-va.uv.y},d2{vc.uv.x-va.uv.x,vc.uv.y-va.uv.y}; float det=d1.x*d2.y-d1.y*d2.x;
        if(std::abs(det)>1e-12f) { Vec3 t=(e1*d2.y-e2*d1.y)/det,bt=(e2*d1.x-e1*d2.x)/det; t=normalized(t-out.normal*dot(t,out.normal)); bt=cross(out.normal,t)*(dot(cross(out.normal,t),bt)<0?-1.f:1.f); out.normal=normalized(t*n.x+bt*n.y+out.normal*n.z); }
    }
    return out;
}
struct Row { Vec3 p; Frame f; float arc=0; };
struct Projection { float u=0,v=0,depth=0; };
struct Card { size_t path=0,plane=0; std::vector<Row> rows; float lo=0,hi=0,start=0,end=0; };
struct BoundCard { size_t path=0; float start=0,end=0,extent=0; std::vector<Vertex> vertices; std::vector<Vec3> centers; std::string material; };
Row at(const Card& card, float v) {
    size_t i=1; while(i+1<card.rows.size()&&card.rows[i].arc<v) ++i;
    const auto& a=card.rows[i-1]; const auto& b=card.rows[i]; float t=(v-a.arc)/(b.arc-a.arc);
    Row out; out.p=mix(a.p,b.p,t); out.arc=v;
    Vec3 y=normalized(b.p-a.p),x=normalized(mix(a.f.x,b.f.x,std::clamp(t,0.f,1.f))); x=normalized(x-y*dot(x,y)); out.f={x,y,normalized(cross(x,y))}; return out;
}
Projection project(const Card& card, Vec3 p) {
    float best=INFINITY,arc=0;
    for(size_t i=1;i<card.rows.size();++i) { auto a=card.rows[i-1].p,b=card.rows[i].p,d=b-a; float t=dot(p-a,d)/dot(d,d); t=std::clamp(t,i==1?-1.f:0.f,i+1==card.rows.size()?2.f:1.f); auto delta=p-(a+d*t); float distance=dot(delta,delta); if(distance<best) { best=distance; arc=card.rows[i-1].arc+t*(card.rows[i].arc-card.rows[i-1].arc); } }
    auto row=at(card,arc); auto d=p-row.p; return {dot(d,row.f.x),arc,dot(d,row.f.z)};
}
std::vector<Row> rows(const Stem& stem, size_t segments, float roll) {
    std::vector<float> arc(stem.samples.size());
    for(size_t i=1;i<arc.size();++i) arc[i]=arc[i-1]+length(stem.samples[i].position-stem.samples[i-1].position);
    require(arc.size()>=2&&arc.back()>1e-6f,"CardBake guide path has no length"); std::vector<Row> out;
    size_t cursor=1;
    for(size_t k=0;k<=segments;++k) { float s=arc.back()*float(k)/segments; while(cursor+1<arc.size()&&arc[cursor]<s) ++cursor; float d=arc[cursor]-arc[cursor-1]; require(d>1e-8f,"CardBake guide has repeated samples"); out.push_back({mix(stem.samples[cursor-1].position,stem.samples[cursor].position,(s-arc[cursor-1])/d),{},0}); }
    Frame f;
    for(size_t i=0;i<out.size();++i) { Vec3 y=normalized(out[std::min(i+1,out.size()-1)].p-out[i?i-1:0].p); Vec3 x=i?f.x-y*dot(f.x,y):frame(y,roll).x; if(length(x)<1e-6f) x=frame(y,roll).x; x=normalized(x); f={x,y,normalized(cross(x,y))}; out[i].f=f; if(i) out[i].arc=out[i-1].arc+length(out[i].p-out[i-1].p); }
    return out;
}
std::vector<std::vector<size_t>> groupFaces(const Mesh& mesh, const std::vector<Stem>& stems, float distance, size_t& components) {
    Mesh guides; std::vector<size_t> path;
    for(size_t s=0;s<stems.size();++s) for(size_t i=1;i<stems[s].samples.size();++i) { auto a=uint32_t(guides.vertices.size()); Vertex v; v.position=stems[s].samples[i-1].position; guides.vertices.push_back(v); v.position=stems[s].samples[i].position; guides.vertices.push_back(v); guides.triangles.push_back({{a,a+1,a+1},""}); path.push_back(s); }
    MeshProximity nearest(guides);
    std::vector<size_t> parent(mesh.vertices.size()); std::iota(parent.begin(),parent.end(),size_t(0));
    auto root=[&](size_t i) { while(parent[i]!=i) { parent[i]=parent[parent[i]]; i=parent[i]; } return i; };
    for(const auto& face:mesh.triangles) for(int k=1;k<3;++k) { auto a=root(face.vertices[0]),b=root(face.vertices[k]); if(a>b) std::swap(a,b); parent[b]=a; }
    std::map<size_t,size_t> assignment; std::vector<std::vector<size_t>> groups(stems.size());
    for(size_t f=0;f<mesh.triangles.size();++f) { size_t r=root(mesh.triangles[f].vertices[0]); if(!assignment.count(r)) { auto hit=nearest.nearest(mesh.vertices[r].position,distance); require(bool(hit),"CardBake component at vertex "+std::to_string(r)+" is farther than max_distance from paths; isolate attached foliage or increase the distance"); assignment[r]=path[hit->triangle]; } groups[assignment[r]].push_back(f); }
    components=assignment.size(); return groups;
}
float area(Vec2 a, Vec2 b, Vec2 c) { return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x); }
struct Texel { Vec3 color{},normal{}; float roughness=0,metallic=0,alpha=0; };
struct Raster { std::vector<Texel> pixels; size_t covered=0; };
Raster capture(const Card& card, const Mesh& source, const std::vector<size_t>& faces, const std::map<std::string,Material>& materials, int width, int height, int samples) {
    int w=width*samples,h=height*samples; size_t count=size_t(w)*h;
    std::vector<float> depth(count,-INFINITY); std::vector<Surface> surface(count);
    for(auto f:faces) {
        const auto& face=source.triangles[f]; Projection p[3]; Vec2 uv[3];
        for(int k=0;k<3;++k) { p[k]=project(card,source.vertices[face.vertices[k]].position); uv[k]={(p[k].u-card.lo)/(card.hi-card.lo),1-(p[k].v-card.start)/(card.end-card.start)}; }
        float d=area(uv[0],uv[1],uv[2]); if(std::abs(d)<1e-14f) continue;
        int x0=int(std::floor(clamp01(std::min({uv[0].x,uv[1].x,uv[2].x}))*w)),x1=std::min(w-1,int(std::ceil(clamp01(std::max({uv[0].x,uv[1].x,uv[2].x}))*w)));
        int y0=int(std::floor(clamp01(std::min({uv[0].y,uv[1].y,uv[2].y}))*h)),y1=std::min(h-1,int(std::ceil(clamp01(std::max({uv[0].y,uv[1].y,uv[2].y}))*h)));
        const auto& material=materials.at(face.material);
        for(int y=y0;y<=y1;++y) for(int x=x0;x<=x1;++x) {
            Vec2 q{(x+.5f)/w,(y+.5f)/h}; float a=area(uv[1],uv[2],q)/d,b=area(uv[2],uv[0],q)/d,c=1-a-b;
            if(a<-1e-6f||b<-1e-6f||c<-1e-6f) continue;
            float z=p[0].depth*a+p[1].depth*b+p[2].depth*c; size_t index=size_t(y)*w+x; if(z<=depth[index]) continue;
            auto value=sample(source,face,material,a,b,c); if(value.alpha==0) continue;
            auto basis=at(card,card.start+(1-q.y)*(card.end-card.start)).f;
            if(dot(value.normal,basis.z)<0) value.normal=value.normal*-1;
            value.normal=normalized({dot(value.normal,basis.x),dot(value.normal,basis.y),dot(value.normal,basis.z)});
            depth[index]=z; surface[index]=value;
        }
    }
    Raster out; out.pixels.resize(size_t(width)*height);
    for(int y=0;y<height;++y) for(int x=0;x<width;++x) {
        auto& t=out.pixels[size_t(y)*width+x]; float coverage=0;
        for(int j=0;j<samples;++j) for(int i=0;i<samples;++i) { const auto& s=surface[size_t(y*samples+j)*w+x*samples+i]; if(s.alpha==0) continue; coverage+=1; t.color=t.color+s.color; t.normal=t.normal+s.normal; t.roughness+=s.roughness; t.metallic+=s.metallic; }
        if(coverage>0) { t.color=t.color/coverage; t.normal=normalized(t.normal); t.roughness/=coverage; t.metallic/=coverage; t.alpha=coverage/(samples*samples); ++out.covered; }
    }
    return out;
}
void cell(Bytes& color, Bytes& normal, Bytes& packed, int atlasSize, int ox, int oy, int cw, int ch, int padding, const Raster& raster) {
    int w=cw-2*padding,h=ch-2*padding; std::vector<int> sources(size_t(cw)*ch,-1),next;
    for(int y=0;y<h;++y) for(int x=0;x<w;++x) if(raster.pixels[size_t(y)*w+x].alpha>0) sources[size_t(y+padding)*cw+x+padding]=y*w+x;
    for(int step=0;step<padding;++step) { next=sources; for(int y=0;y<ch;++y) for(int x=0;x<cw;++x) { auto i=size_t(y)*cw+x; if(sources[i]>=0) continue; for(auto d:{std::pair<int,int>{-1,0},{1,0},{0,-1},{0,1},{-1,-1},{1,-1},{-1,1},{1,1}}) { int xx=x+d.first,yy=y+d.second; if(xx>=0&&xx<cw&&yy>=0&&yy<ch&&sources[size_t(yy)*cw+xx]>=0) { next[i]=sources[size_t(yy)*cw+xx]; break; } } } sources.swap(next); }
    for(int y=0;y<ch;++y) for(int x=0;x<cw;++x) {
        size_t i=(size_t(oy+y)*atlasSize+ox+x)*4; int s=sources[size_t(y)*cw+x]; if(s<0) continue;
        auto t=raster.pixels[size_t(s)]; float alpha=x>=padding&&x<cw-padding&&y>=padding&&y<ch-padding?raster.pixels[size_t(y-padding)*w+x-padding].alpha:0;
        color[i]=byte(srgb(t.color.x)); color[i+1]=byte(srgb(t.color.y)); color[i+2]=byte(srgb(t.color.z)); color[i+3]=byte(alpha);
        normal[i]=byte(t.normal.x*.5f+.5f); normal[i+1]=byte(t.normal.y*.5f+.5f); normal[i+2]=byte(t.normal.z*.5f+.5f); normal[i+3]=255;
        packed[i]=255; packed[i+1]=byte(t.roughness); packed[i+2]=byte(t.metallic); packed[i+3]=255;
    }
}
void append(Mesh& to, const Mesh& from) { uint32_t offset=uint32_t(to.vertices.size()); to.vertices.insert(to.vertices.end(),from.vertices.begin(),from.vertices.end()); for(auto f:from.triangles) { for(auto& i:f.vertices) i+=offset; to.triangles.push_back(std::move(f)); } }
}
static Json exportCardBakeImpl(const Graph& graph, const std::string& name, const std::filesystem::path& directory, const Options& options, CardBakeResult* captured, std::vector<BoundCard>* bindings=nullptr) {
    require(graph.document.contains("card_bakes")&&graph.document["card_bakes"].contains(name),"unknown CardBake '"+name+"'");
    const auto& p=graph.document["card_bakes"][name]; std::string sourceName=p["source"],pathName=p["paths"],output=p["output"];
    std::vector<std::string> wanted{sourceName,pathName}; if(p.contains("keep")) wanted.push_back(p["keep"]);
    auto generated=generateNodes(graph,options,wanted); const auto& source=generated.outputs.at(sourceName)->mesh;
    auto stems=generated.outputs.at(pathName)->stems;
    for(auto& stem:stems) if(stem.growth) stem.samples=visibleSamples(stem);
    const Mesh* keep=p.contains("keep")?&generated.outputs.at(p["keep"].get<std::string>())->mesh:nullptr;
    if(options.growth&&source.triangles.empty()) {
        Graph target=graph; target.document["outputs"]={{output,sourceName}};
        auto path=std::filesystem::path(output),folder=path.parent_path()/(path.stem().string()+".cardbake");
        preflightExports({target},directory,{(folder/"manifest.json").generic_string()});
        Memory memory{0,p["memory_mb"].get<size_t>()*1024*1024};
        if(keep) memory.reserve(keep->vertices.size()*sizeof(Vertex)+keep->triangles.size()*sizeof(Triangle));
        std::shared_ptr<const Value> value=keep?generated.outputs.at(p["keep"].get<std::string>()):std::make_shared<Value>();
        Result result; result.outputs[output]=value; result.stats=generated.stats;
        auto report=write(target,result,directory);
        report.update({{"type","card_bake"},{"version",1},{"profile",name},{"seed",generated.stats["seed"]},{"settings",p},{"growth_progress",*options.growth},{"source_triangles",0},{"retained_triangles",value->mesh.triangles.size()},{"cards",0},{"card_triangles",0},{"atlas_pages",0},{"maps",Json::array()},{"cells",Json::array()}});
        report.erase("stats"); auto root=std::filesystem::absolute(directory); std::filesystem::create_directories(root/folder);
        Json manifest=report; for(auto& file:manifest["outputs"]) file["path"]=(std::filesystem::path("..")/path.filename()).generic_string();
        jsonFile(root/folder/"manifest.json",manifest); report["manifest"]=(root/folder/"manifest.json").string();
        if(captured) { captured->mesh=value->mesh; captured->materials=graph.document["materials"]; captured->base=graph.base; }
        return report;
    }
    require(!stems.empty()&&!source.triangles.empty(),"CardBake requires nonempty source and paths");
    int size=p["atlas_size"],cw=p["cell_width"],ch=p["cell_height"],pad=p["padding"],samples=p["samples"],segments=p["segments"],planes=p["planes"];
    Memory memory{0,p["memory_mb"].get<size_t>()*1024*1024};
    memory.reserve(generated.stats["generated_vertices"].get<size_t>()*sizeof(Vertex)+generated.stats["generated_triangles"].get<size_t>()*sizeof(Triangle)+generated.stats["generated_points"].get<size_t>()*sizeof(Point));
    memory.reserve(source.vertices.size()*32+source.triangles.size()*16+stems.size()*size_t(segments+1)*sizeof(Row)*planes);
    memory.reserve(size_t(size)*size*12+size_t(cw)*ch*(size_t(samples*samples)*(sizeof(Surface)+sizeof(float))+sizeof(Texel)+8));
    size_t guideSegments=0;
    for(const auto& stem:stems) { if(stem.samples.empty()) continue; require(stem.samples.size()>=2,"CardBake guide path needs at least two samples"); guideSegments+=stem.samples.size()-1; }
    require(guideSegments>0,"CardBake source has no reached guide segments");
    // Degenerate guide triangles provide a bounded segment BVH for attachment.
    memory.reserve(guideSegments*(2*sizeof(Vertex)+sizeof(Triangle)+192));
    size_t components=0; auto groups=groupFaces(source,stems,p["max_distance"],components);
    std::vector<Card> cards;
    for(size_t path=0;path<stems.size();++path) if(!groups[path].empty()) {
        Card basis; basis.rows=rows(stems[path],size_t(segments),0); float xx=0,xz=0,zz=0;
        for(auto f:groups[path]) for(auto i:source.triangles[f].vertices) { auto q=project(basis,source.vertices[i].position); xx+=q.u*q.u; xz+=q.u*q.depth; zz+=q.depth*q.depth; }
        float roll=.5f*std::atan2(2*xz,xx-zz);
        for(int plane=0;plane<planes;++plane) {
            Card card; card.path=path; card.plane=size_t(plane); card.rows=rows(stems[path],size_t(segments),roll+pi*plane/planes); card.lo=card.start=INFINITY; card.hi=card.end=-INFINITY;
            for(auto f:groups[path]) for(auto i:source.triangles[f].vertices) { auto q=project(card,source.vertices[i].position); card.lo=std::min(card.lo,q.u); card.hi=std::max(card.hi,q.u); card.start=std::min(card.start,q.v); card.end=std::max(card.end,q.v); }
            float margin=p["margin"]; card.lo-=margin; card.hi+=margin; card.start=std::min(0.f,card.start)-margin; card.end=std::max(card.rows.back().arc,card.end)+margin;
            require(card.hi-card.lo>1e-6f&&card.end-card.start>1e-6f,"CardBake source projects to a zero-area card; use a positive margin"); cards.push_back(std::move(card));
        }
    }
    size_t perPage=size_t(size/cw)*(size/ch),pages=(cards.size()+perPage-1)/perPage; require(pages>0&&pages<=64,"CardBake exceeds 64 atlas pages; increase atlas_size or reduce cell resolution");
    Budget budget{options.limits}; budget.charge(cards.size()*size_t(segments+1)*2,cards.size()*size_t(segments)*2,0); if(keep) budget.charge(keep->vertices.size(),keep->triangles.size(),0);
    memory.reserve(budget.vertices*sizeof(Vertex)+budget.triangles*sizeof(Triangle));
    if(bindings) { memory.reserve(cards.size()*(sizeof(BoundCard)+size_t(segments+1)*(2*sizeof(Vertex)+sizeof(Vec3)))); bindings->reserve(cards.size()); }
    auto path=std::filesystem::path(output),folder=path.parent_path()/(path.stem().string()+".cardbake");
    std::vector<std::string> files{(folder/"manifest.json").generic_string()};
    for(size_t page=0;page<pages;++page) for(auto map:{"color","normal","metallic-roughness"}) files.push_back((folder/(std::to_string(page)+"-"+map+".png")).generic_string());
    Graph target=graph; target.document["outputs"]={{output,sourceName}}; preflightExports({target},directory,files);
    std::map<std::filesystem::path,std::unique_ptr<Image>> images; std::map<std::string,Material> materials;
    for(const auto& face:source.triangles) if(!materials.count(face.material)) {
        const auto& m=graph.document["materials"][face.material]; require(m["alpha_mode"]!="BLEND","CardBake supports OPAQUE and MASK sources; BLEND needs multilayer compositing");
        require(m.value("subsurface",0.f)==0,"CardBake does not preserve source subsurface scattering; use the uniform translucency control for thin cards");
        require(m.value("transmission",0.f)==0&&m.value("ior",1.5f)==1.5f,"CardBake does not preserve refractive transmission or custom IOR; retain gemstone geometry");
        Material material; material.color=m["base_color"].get<Pixel>(); material.roughness=m["roughness"]; material.metallic=m["metallic"]; material.mask=m["alpha_mode"]=="MASK"; material.cutoff=m["alpha_cutoff"];
        auto image=[&](const char* key) -> const Image* { if(!m.contains(key)) return nullptr; auto file=std::filesystem::weakly_canonical(graph.base/m[key].get<std::string>()); if(!images.count(file)) images[file]=std::make_unique<Image>(file,memory); return images.at(file).get(); };
        material.albedo=image("base_color_texture"); material.normal=image("normal_texture"); material.packed=image("metallic_roughness_texture"); materials[face.material]=material;
    }
    Mesh mesh; mesh.vertices.reserve(budget.vertices); mesh.triangles.reserve(budget.triangles); if(keep) append(mesh,*keep);
    Json outputMaterials=graph.document["materials"];
    for(auto& m:outputMaterials) for(auto key:{"base_color_texture","normal_texture","metallic_roughness_texture"}) if(m.contains(key)) m[key]=std::filesystem::absolute(graph.base/m[key].get<std::string>()).string();
    for(size_t page=0;page<pages;++page) require(!outputMaterials.contains("cardbake_"+name+"_"+std::to_string(page)),"CardBake generated material name collides");
    auto root=std::filesystem::absolute(directory); std::filesystem::create_directories(root/folder);
    Json report={{"type","card_bake"},{"version",1},{"profile",name},{"seed",generated.stats["seed"]},{"settings",p},{"source_triangles",source.triangles.size()},{"retained_triangles",keep?keep->triangles.size():0},{"components",components},{"guide_paths",stems.size()},{"cards",cards.size()},{"card_triangles",cards.size()*segments*2},{"atlas_pages",pages},{"estimated_working_mb",double(memory.used)/(1024*1024)},{"cells",Json::array()},{"maps",Json::array()}};
    if(options.growth) report["growth_progress"]=*options.growth;
    size_t totalCovered=0;
    for(size_t page=0;page<pages;++page) {
        Bytes color(size_t(size)*size*4),normal(color.size()),packed(color.size());
        for(size_t i=0;i<normal.size();i+=4) { normal[i]=normal[i+1]=128; normal[i+2]=normal[i+3]=255; packed[i]=packed[i+1]=packed[i+3]=255; }
        std::string material="cardbake_"+name+"_"+std::to_string(page); require(!outputMaterials.contains(material),"CardBake generated material name collides: "+material);
        Json mat={{"base_color",{1,1,1,1}},{"roughness",1},{"metallic",1},{"double_sided",true},{"alpha_mode","MASK"},{"alpha_cutoff",p["alpha_cutoff"]},{"translucency",p["translucency"]}}; validateFields(mat,materialFields(),"baked material");
        for(size_t i=page*perPage;i<std::min(cards.size(),(page+1)*perPage);++i) {
            const auto& card=cards[i]; size_t slot=i%perPage; int ox=int(slot%size_t(size/cw))*cw,oy=int(slot/size_t(size/cw))*ch;
            auto raster=capture(card,source,groups[card.path],materials,cw-2*pad,ch-2*pad,samples); totalCovered+=raster.covered; cell(color,normal,packed,size,ox,oy,cw,ch,pad,raster);
            float u0=float(ox+pad)/size,u1=float(ox+cw-pad)/size,v0=1-float(oy+ch-pad)/size,v1=1-float(oy+pad)/size;
            uint32_t offset=uint32_t(mesh.vertices.size());
            BoundCard binding;
            if(bindings) { binding.path=card.path; binding.start=card.start; binding.end=card.end; binding.extent=card.rows.back().arc; binding.material=material; binding.centers.reserve(size_t(segments+1)); }
            for(int row=0;row<=segments;++row) { float t=float(row)/segments; auto point=at(card,card.start+(card.end-card.start)*t);
                if(bindings) binding.centers.push_back(point.p);
                for(int side=0;side<2;++side) { Vertex v; v.position=point.p+point.f.x*(side?card.hi:card.lo); v.normal=point.f.z; v.uv={side?u1:u0,v0+(v1-v0)*t};
                    float closest=INFINITY; for(auto f:groups[card.path]) for(auto index:source.triangles[f].vertices) { const auto& original=source.vertices[index]; auto d=original.position-v.position; float distance=dot(d,d); if(distance<closest) { closest=distance; v.wind=original.wind; } }
                    mesh.vertices.push_back(v);
                }
                if(row) { auto a=offset+uint32_t((row-1)*2); mesh.triangles.push_back({{a,a+1,a+2},material}); mesh.triangles.push_back({{a+1,a+3,a+2},material}); }
            }
            if(bindings) { binding.vertices.assign(mesh.vertices.begin()+offset,mesh.vertices.end()); bindings->push_back(std::move(binding)); }
            report["cells"].push_back({{"path",card.path},{"plane",card.plane},{"page",page},{"rect",{ox,oy,cw,ch}},{"uv_rect",{u0,v0,u1,v1}},{"width",card.hi-card.lo},{"length",card.end-card.start},{"covered_texels",raster.covered}});
        }
        for(auto map:{"color","normal","metallic-roughness"}) { auto relative=folder/(std::to_string(page)+"-"+map+".png"); png(root/relative,size,size,std::string(map)=="color"?color:std::string(map)=="normal"?normal:packed); mat[std::string(map)=="color"?"base_color_texture":std::string(map)=="normal"?"normal_texture":"metallic_roughness_texture"]=relative.generic_string(); }
        outputMaterials[material]=mat;
        report["maps"].push_back({{"page",page},{"size",{size,size}},{"color",std::to_string(page)+"-color.png"},{"normal",std::to_string(page)+"-normal.png"},{"metallic_roughness",std::to_string(page)+"-metallic-roughness.png"}});
    }
    require(totalCovered>0,"CardBake has no visible source coverage at this resolution"); checkMesh(mesh); saveGlb(mesh,outputMaterials,root,root/path);
    report["outputs"]=Json::array({{{"path",(root/path).string()},{"triangles",mesh.triangles.size()},{"vertices",mesh.vertices.size()}}});
    report["mesh"]=path.filename().string(); report["mesh_stats"]=meshStats(mesh); report["color_encoding"]="sRGB, straight alpha coverage; source factors and vertex colors included"; report["normal_encoding"]="OpenGL tangent space; frontmost projected surface, normals face the card front";
    report["limitations"]={"Curve-flattened vertex projection, not an arbitrary cage ray baker; close views and the back side differ from full 3D foliage.","Unique cells per path and plane. No atlas randomization or baked lighting. Alpha overdraw and mip coverage still need target-engine profiling.","Connected source components are assigned at their first indexed vertex. Keep source prototypes based at their attachment pivot."};
    Json manifest=report; manifest["outputs"][0]["path"]=(std::filesystem::path("..")/path.filename()).generic_string(); manifest["mesh"]=(std::filesystem::path("..")/path.filename()).generic_string(); jsonFile(root/folder/"manifest.json",manifest);
    report["manifest"]=(root/folder/"manifest.json").string();
    if(captured) { captured->mesh=std::move(mesh); captured->materials=std::move(outputMaterials); captured->base=root; }
    return report;
}
Json exportCardBake(const Graph& graph, const std::string& name, const std::filesystem::path& directory, const Options& options) { return exportCardBakeImpl(graph,name,directory,options,nullptr); }
CardBakeResult bakeCards(const Graph& graph, const std::string& name, const std::filesystem::path& directory, const Options& options) { CardBakeResult result; result.report=exportCardBakeImpl(graph,name,directory,options,&result); return result; }

struct CardGrowth {
    Graph graph;
    int32_t seed=0;
    std::string name,path,keep;
    std::vector<BoundCard> cards;
    size_t paths=0;
    Json materials,report;
    std::filesystem::path base;
};
std::shared_ptr<const CardGrowth> prepareCardGrowth(const Graph& graph, const std::string& name, const std::filesystem::path& directory, const Options& options) {
    require(graph.document.contains("growth")&&graph.document["growth"]["mode"]=="developmental","Reusable CardBake requires growth.mode developmental");
    require(graph.document.contains("card_bakes")&&graph.document["card_bakes"].contains(name),"unknown CardBake '"+name+"'");
    require(!std::filesystem::exists(directory)||(std::filesystem::is_directory(directory)&&std::filesystem::is_empty(directory)),"Reusable CardBake needs an empty output directory; existing files are preserved");
    require(!options.growth||*options.growth==1,"Reusable CardBake preparation must use mature growth");
    auto prepared=std::make_shared<CardGrowth>(); prepared->graph=graph; prepared->name=name;
    const auto& profile=graph.document["card_bakes"][name]; prepared->path=profile["paths"]; prepared->keep=profile.value("keep",std::string());
    Options mature=options; mature.growth.reset(); prepared->seed=options.seed.value_or(graph.document["seed"].get<int32_t>());
    CardBakeResult baked; prepared->report=exportCardBakeImpl(graph,name,directory,mature,&baked,&prepared->cards);
    prepared->paths=prepared->report["guide_paths"].get<size_t>(); prepared->base=baked.base;
    // Keep slots fixed even when the current growth sample has no cards or wood.
    prepared->materials=Json::object();
    for(const auto& face:baked.mesh.triangles) prepared->materials[face.material]=baked.materials.at(face.material);
    // A young wood cap may survive a union even if that material was entirely
    // enclosed in the mature mesh. Reserve its slot before the first sample.
    std::set<std::string> visited;
    std::function<void(const std::string&)> keepMaterials=[&](const std::string& id) {
        if(!visited.insert(id).second) return;
        const auto& node=graph.document["nodes"][id];
        for(auto key:{"material","cap_material"}) if(node.contains(key)) { std::string material=node[key]; prepared->materials[material]=baked.materials.at(material); }
        for(const auto& reference:references(node)) keepMaterials(reference);
    };
    if(!prepared->keep.empty()) keepMaterials(prepared->keep);
    return prepared;
}
CardBakeResult growCards(const CardGrowth& prepared, double progress, const Limits& limits) {
    require(std::isfinite(progress)&&progress>=0&&progress<=1,"Growth progress must be within 0..1");
    Options options; options.seed=prepared.seed; options.growth=progress; options.limits=limits;
    std::vector<std::string> wanted{prepared.path}; if(!prepared.keep.empty()) wanted.push_back(prepared.keep);
    auto generated=generateNodes(prepared.graph,options,wanted);
    const auto& stems=generated.outputs.at(prepared.path)->stems;
    require(stems.size()==prepared.paths,"Reusable CardBake guide membership changed during growth");
    CardBakeResult result; result.materials=prepared.materials; result.base=prepared.base;
    Budget budget{limits};
    budget.charge(generated.stats["generated_vertices"].get<size_t>(),generated.stats["generated_triangles"].get<size_t>(),generated.stats["generated_points"].get<size_t>());
    if(!prepared.keep.empty()) {
        const auto& keep=generated.outputs.at(prepared.keep)->mesh;
        budget.charge(keep.vertices.size(),keep.triangles.size(),0); result.mesh=keep;
    }
    size_t retained=result.mesh.triangles.size(),visible=0;
    for(const auto& card:prepared.cards) {
        const auto& stem=stems.at(card.path); const auto& state=stem.growth;
        if(state&&!state->visible) continue;
        float reach=state?state->visible_length:1;
        if(reach<=0) continue;
        // Reveal the fixed mature UVs along the reached prefix. The fitted end
        // overhang follows the tip; source leaves are never regenerated here.
        float cap=reach>=1?1:clamp01((card.end*reach-card.start)/(card.end-card.start));
        size_t segments=card.centers.size()-1;
        float last=cap*float(segments); size_t whole=size_t(std::floor(last));
        bool partial=last-float(whole)>1e-5f;
        size_t count=whole+1+(partial?1:0);
        if(count<2) continue;
        budget.charge(count*2,(count-1)*2,0);
        auto offset=uint32_t(result.mesh.vertices.size());
        for(size_t row=0;row<count;++row) {
            float index=row<=whole?float(row):last;
            size_t a=std::min(size_t(index),segments),b=std::min(a+1,segments); float blend=index-float(a);
            auto center=mix(card.centers[a],card.centers[b],blend);
            float arc=card.start+(card.end-card.start)*(index/float(segments));
            float width=state?growthRadius(state,std::min(reach,clamp01(arc/card.extent))):1;
            for(size_t side=0;side<2;++side) {
                auto v=card.vertices[a*2+side]; const auto& next=card.vertices[b*2+side];
                if(blend>0) {
                    v.position=mix(v.position,next.position,blend); v.normal=normalized(mix(v.normal,next.normal,blend));
                    v.uv={v.uv.x+(next.uv.x-v.uv.x)*blend,v.uv.y+(next.uv.y-v.uv.y)*blend};
                    v.wind={v.wind.x+(next.wind.x-v.wind.x)*blend,v.wind.y+(next.wind.y-v.wind.y)*blend};
                }
                if(width!=1) v.position=center+(v.position-center)*width;
                result.mesh.vertices.push_back(v);
            }
            if(row) { auto first=offset+uint32_t((row-1)*2); result.mesh.triangles.push_back({{first,first+1,first+2},card.material}); result.mesh.triangles.push_back({{first+1,first+3,first+2},card.material}); }
        }
        ++visible;
    }
    checkMesh(result.mesh);
    result.report={{"type","card_growth"},{"profile",prepared.name},{"seed",prepared.seed},{"progress",progress},{"lod",prepared.graph.level},{"cards",visible},{"prepared_cards",prepared.cards.size()},{"retained_triangles",retained},{"card_triangles",result.mesh.triangles.size()-retained},{"atlas_pages",prepared.report["atlas_pages"]},{"rebaked",false},{"generation",generated.stats}};
    return result;
}
}
