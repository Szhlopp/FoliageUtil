#include "foliage/foliage.hpp"
#include <png.h>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
using namespace foliage;
namespace {
int checks=0;
void expect(bool ok, const std::string& message) { ++checks; if(!ok) throw std::runtime_error(message); }
void fails(const std::function<void()>& fn, const std::string& message) { try { fn(); } catch(const std::exception& e) { expect(std::string(e.what()).find(message)!=std::string::npos,"unexpected error: "+std::string(e.what())); return; } throw std::runtime_error("expected error: "+message); }
std::string bytes(const std::filesystem::path& path) { std::ifstream f(path,std::ios::binary); std::ostringstream s; s<<f.rdbuf(); return s.str(); }
struct Image { int width,height; std::vector<uint8_t> pixels; };
Image read(const std::filesystem::path& file) {
    png_image p{}; p.version=PNG_IMAGE_VERSION; expect(png_image_begin_read_from_file(&p,file.string().c_str()),"read PNG header"); p.format=PNG_FORMAT_RGBA; Image out{int(p.width),int(p.height),std::vector<uint8_t>(PNG_IMAGE_SIZE(p))}; expect(png_image_finish_read(&p,nullptr,out.pixels.data(),0,nullptr),"decode PNG"); png_image_free(&p); return out;
}
void save(const std::filesystem::path& file, const std::vector<uint8_t>& pixels) { png_image p{}; p.version=PNG_IMAGE_VERSION; p.width=p.height=8; p.format=PNG_FORMAT_RGBA; expect(png_image_write_to_file(&p,file.string().c_str(),0,pixels.data(),0,nullptr),"write fixture PNG"); }
Json recipe() {
    return {{"nodes",{
        {"path",{{"op","curve"},{"points",{{0,0,0},{0,2,0}}}}},
        {"plane",{{"op","card"},{"height",2},{"width",1},{"material","paint"}}}
    }},{"materials",{{"paint",{{"base_color",{.5,1,1,1}},{"roughness",.7},{"metallic",.25},{"base_color_texture","source.png"}}}}},
    {"outputs",{{"ordinary.glb","plane"}}},{"card_bakes",{{"Test",{{"source","plane"},{"paths","path"},{"output","baked.glb"},{"segments",2},{"planes",1},{"cell_width",64},{"cell_height",128},{"atlas_size",128},{"padding",4},{"samples",1},{"margin",0},{"max_distance",.6},{"translucency",0}}}}}};
}
uint32_t u32(const std::string& s, size_t offset) { uint32_t n=0; for(int i=0;i<4;++i) n|=uint32_t(uint8_t(s.at(offset+i)))<<(8*i); return n; }
Json glb(const std::filesystem::path& file) { auto s=bytes(file); expect(u32(s,0)==0x46546c67,"GLB magic"); return Json::parse(s.substr(20,u32(s,12))); }
}
int main(int argc, char** argv) {
    try {
        expect(argc==2,"output directory argument"); auto root=std::filesystem::absolute(argv[1]); std::filesystem::create_directories(root);
        std::vector<uint8_t> source(8*8*4,255);
        for(int y=0;y<8;++y) for(int x=0;x<8;++x) { size_t i=size_t(y*8+x)*4; source[i]=y<4?255:0; source[i+1]=y<4?0:255; source[i+2]=x<4?0:255; }
        save(root/"source.png",source); auto d=recipe(); auto graph=parse(d,root); auto r=exportCardBake(graph,"Test",root/"first");
        expect(r["cards"]==1&&r["card_triangles"]==4&&r["source_triangles"]==2,"fitted card counts"); expect(r["atlas_pages"]==1,"one page");
        expect(!std::filesystem::exists(root/"first/ordinary.glb"),"only requested CardBake exports");
        auto color=read(root/"first/baked.cardbake/0-color.png"); auto normal=read(root/"first/baked.cardbake/0-normal.png"); auto packed=read(root/"first/baked.cardbake/0-metallic-roughness.png");
        auto pixel=[](const Image& image,int x,int y,int c) { return image.pixels[(size_t(y)*image.width+x)*4+c]; };
        expect(std::abs(pixel(color,16,24,0)-188)<=1&&pixel(color,16,24,1)==0,"linear material factor baked once into sRGB");
        expect(pixel(color,16,104,0)==0&&pixel(color,16,104,1)==255,"top-left PNG and bottom-left mesh UV orientation");
        expect(pixel(color,48,24,2)==255&&pixel(color,16,24,2)==0,"horizontal UV orientation");
        expect(pixel(color,2,24,3)==0&&pixel(color,2,24,0)>0,"RGB dilation retains transparent gutter");
        expect(pixel(color,80,24,3)==0,"empty atlas cells stay transparent");
        expect(std::abs(pixel(normal,16,24,0)-128)<=1&&std::abs(pixel(normal,16,24,1)-128)<=1&&pixel(normal,16,24,2)==255,"flat normal projection");
        expect(std::abs(pixel(packed,16,24,1)-179)<=1&&std::abs(pixel(packed,16,24,2)-64)<=1,"roughness and metalness scalars baked");
        auto json=glb(root/"first/baked.glb"); expect(json["images"].size()==3&&json["materials"][0]["alphaMode"]=="MASK","embedded PBR and MASK");
        expect(json["materials"][0]["pbrMetallicRoughness"]["baseColorFactor"]==Json::array({1,1,1,1}),"material factors reset after bake");
        exportCardBake(graph,"Test",root/"repeat"); expect(bytes(root/"first/baked.glb")==bytes(root/"repeat/baked.glb"),"repeatable GLB and baked PNG bytes");
        auto twice=d; twice["nodes"]["second"]={{"op","transform"},{"input","plane"},{"translation",{3,0,0}}}; twice["nodes"]["second_path"]={{"op","transform"},{"input","path"},{"translation",{3,0,0}}}; twice["nodes"]["both"]={{"op","merge"},{"inputs",{"plane","second"}}}; twice["nodes"]["paths"]={{"op","merge"},{"inputs",{"path","second_path"}}}; twice["card_bakes"]["Test"].update({{"source","both"},{"paths","paths"},{"cell_width",128}});
        auto multipage=exportCardBake(parse(twice,root),"Test",root/"pages"); expect(multipage["cards"]==2&&multipage["atlas_pages"]==2,"unique cells spill into pages"); expect(multipage["cells"][0]["path"]!=multipage["cells"][1]["path"],"separate strand assignment");
        auto hole=d; hole["materials"]["paint"]["alpha_mode"]="MASK";
        for(int y=2;y<6;++y) for(int x=2;x<6;++x) source[(y*8+x)*4+3]=0; save(root/"source.png",source);
        exportCardBake(parse(hole,root),"Test",root/"hole"); auto masked=read(root/"hole/baked.cardbake/0-color.png"); expect(pixel(masked,32,64,3)==0&&pixel(masked,10,16,3)==255,"source alpha holes retained");
        auto layered=hole; layered["materials"]["blue"]={{"base_color",{0,0,1,1}}}; layered["nodes"]["back"]={{"op","card"},{"height",2},{"width",1},{"material","blue"}}; layered["nodes"]["behind"]={{"op","transform"},{"input","back"},{"translation",{0,0,-.1}}}; layered["nodes"]["layers"]={{"op","merge"},{"inputs",{"plane","behind"}}}; layered["card_bakes"]["Test"]["source"]="layers";
        exportCardBake(parse(layered,root),"Test",root/"layers"); auto composite=read(root/"layers/baked.cardbake/0-color.png"); expect(pixel(composite,32,64,2)==255&&pixel(composite,32,64,3)==255,"frontmost capture sees through MASK to underlying geometry");
        auto normalMap=source; for(size_t i=0;i<normalMap.size();i+=4) { normalMap[i]=128; normalMap[i+1]=204; normalMap[i+2]=230; normalMap[i+3]=255; } save(root/"normal.png",normalMap);
        auto detailed=d; detailed["materials"]["paint"]["normal_texture"]="normal.png";
        exportCardBake(parse(detailed,root),"Test",root/"normal"); auto detail=read(root/"normal/baked.cardbake/0-normal.png"); expect(pixel(detail,16,24,1)>195,"OpenGL positive green normal retained");
        for(auto bad:std::vector<Json>{{{"segments",0}},{{"samples",4}},{{"unknown",1}},{{"padding",32}},{{"output","../bad.glb"}},{{"paths","plane"}}}) { auto invalid=d; invalid["card_bakes"]["Test"].update(bad); fails([&]{parse(invalid,root);},"CardBake"); }
        auto noMatch=d; noMatch["card_bakes"]["Test"]["max_distance"]=.01; fails([&]{exportCardBake(parse(noMatch,root),"Test",root/"bad-distance");},"max_distance");
        auto blend=d; blend["materials"]["paint"]["alpha_mode"]="BLEND"; fails([&]{exportCardBake(parse(blend,root),"Test",root/"bad-blend");},"BLEND"); expect(!std::filesystem::exists(root/"bad-blend"),"unsupported material fails before output");
        for(auto property:{"transmission","ior"}) { auto refractive=d; refractive["materials"]["paint"][property]=std::string(property)=="ior"?2.42:1; fails([&]{exportCardBake(parse(refractive,root),"Test",root/"bad-refraction");},"does not preserve refractive"); expect(!std::filesystem::exists(root/"bad-refraction"),"unsupported refraction fails before output"); }
        auto big=d; big["card_bakes"]["Test"].update({{"atlas_size",8192},{"memory_mb",64}}); fails([&]{exportCardBake(parse(big,root),"Test",root/"bad-memory");},"memory budget");
        Options limited; limited.limits.triangles=3; fails([&]{exportCardBake(graph,"Test",root/"bad-geometry",limited);},"triangle budget");
        Options growing; growing.growth=.5; expect(exportCardBake(graph,"Test",root/"growth",growing)["cards"]==1,"unstaged CardBake source remains unchanged at partial progress");
        auto protectedDoc=d; protectedDoc["materials"]["paint"]["base_color_texture"]="first/baked.cardbake/0-color.png"; fails([&]{exportCardBake(parse(protectedDoc,root),"Test",root/"first");},"overwrite input asset");
        auto badPage=d; badPage["card_bakes"]["Test"]["output"]="escape/baked.glb"; std::error_code error; std::filesystem::create_directories(root/"symlink"); std::filesystem::create_directory_symlink(root,root/"symlink/escape",error); if(!error) fails([&]{exportCardBake(parse(badPage,root),"Test",root/"symlink");},"outside output directory");
        auto retained=d; retained["card_bakes"]["Test"]["keep"]="plane"; auto withKeep=exportCardBake(parse(retained,root),"Test",root/"retained"); expect(withKeep["retained_triangles"]==2&&withKeep["outputs"][0]["triangles"]==6,"keep mesh remains separate from baked cards");
        auto retainedGlb=glb(root/"retained/baked.glb"); expect(retainedGlb["materials"].size()==2&&retainedGlb["images"].size()==4,"keep material and original texture survive");
        auto crossed=d; crossed["card_bakes"]["Test"].update({{"planes",2},{"margin",.01}}); auto crossing=exportCardBake(parse(crossed,root),"Test",root/"crossed"); expect(crossing["cards"]==2&&crossing["cells"][0]["uv_rect"]!=crossing["cells"][1]["uv_rect"],"crossed planes receive distinct unique cells");
        auto curved=d; curved["nodes"]["path"]["points"]={{0,0,0},{.15,1,.1},{.25,2,.4}}; auto curveResult=exportCardBake(parse(curved,root),"Test",root/"curved"); expect(curveResult["outputs"][0]["triangles"]==4,"curved guide export succeeds at configured topology");
        auto collision=d; collision["materials"]["cardbake_Test_0"]={{"base_color",{1,1,1,1}}}; fails([&]{exportCardBake(parse(collision,root),"Test",root/"collision");},"material name collides"); expect(!std::filesystem::exists(root/"collision"),"material collision rejected before writes");
        auto invisible=d; invisible["materials"]["paint"].update({{"alpha_mode","MASK"},{"base_color",{1,1,1,0}}}); fails([&]{exportCardBake(parse(invisible,root),"Test",root/"invisible");},"no visible source coverage");
        expect(parse(graph.document,root).document==graph.document,"normalized CardBake profile reparses identically");
        auto ordinary=generate(graph); expect(ordinary.outputs.size()==1&&ordinary.outputs.begin()->second->mesh.triangles.size()==2,"saved bake profiles do not alter ordinary generation");
        std::cout<<checks<<" CardBake checks passed\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<"CardBake test failed: "<<e.what()<<'\n'; return 1; }
}
