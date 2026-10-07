#include <foliage/foliage.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace foliage;
namespace {
int checks=0;
void expect(bool condition, const char* message) { ++checks; if(!condition) throw std::runtime_error(message); }
template<class F> void fails(F action) { bool failed=false; try { action(); } catch(const std::exception&) { failed=true; } expect(failed,"invalid developmental configuration accepted"); }
Json recipe() {
    return Json::parse(R"({
      "nodes": {
        "trunk": {"op":"trunk","length":4,"radius":0.2,"segments":16,"taper":0,"flare":0,"noise":0},
        "branches": {"op":"branch","input":"trunk","count":2,"range":[0.25,0.75],"length":1,"length_decay":0,"length_variation":0,"radius_scale":0.5,"taper":0.2,"angle":90,"angle_jitter":0,"noise":0,"segments":8},
        "trunk_mesh": {"op":"tube","input":"trunk","sides":12},
        "branch_mesh": {"op":"tube","input":"branches","sides":8},
        "paths": {"op":"merge","inputs":["trunk","branches"]},
        "wood_mesh": {"op":"tube","input":"paths","sides":8},
        "solid": {"op":"solidify","input":"wood_mesh"},
        "sites": {"op":"scatter","input":"branches","count":2,"range":[0.25,0.75],"jitter":0,"scale":[1,1],"angle":0},
        "leaf": {"op":"card","width":0.2,"height":0.4},
        "leaves": {"op":"instance","input":"leaf","points":"sites","color_variation":0.3}
      },
      "outputs":{"trunk.glb":"trunk_mesh","branches.glb":"branch_mesh","leaves.glb":"leaves","wood.glb":"solid"},
      "growth":{"mode":"developmental","steps":9,"stages":{
        "trunk":{"start":0,"end":1,"length_profile":[[0,0],[0.5,1],[1,1]],"radius_profile":[[0,0.1],[1,1]]},
        "branches":{"start":0,"end":1,"radius_profile":[[0,0.1],[1,1]]},
        "leaves":{"start":0,"end":1,"scale_profile":[[0,0],[0.25,0.15],[0.6,0.65],[1,1]]}
      }}
    })");
}
Result at(const Graph& graph, double progress) { Options options; options.growth=progress; return generate(graph,options); }
const Mesh& output(const Result& result, const char* name) { return result.outputs.at(name)->mesh; }
Json node(const Result& result, const char* name) { for(const auto& item:result.stats["nodes"]) if(item["node"]==name) return item; throw std::runtime_error("missing node report"); }
float highY(const Mesh& mesh) { float value=-1e30f; for(const auto& v:mesh.vertices) value=std::max(value,v.position.y); return value; }
std::string bytes(const std::filesystem::path& path) { std::ifstream f(path,std::ios::binary); return {std::istreambuf_iterator<char>(f),{}}; }
}
int main(int argc, char** argv) {
    try {
        if(argc!=3) throw std::runtime_error("usage: foliage_development_tests FIXTURES OUTPUT");
        std::filesystem::path fixtures=argv[1],work=argv[2]; auto source=recipe(); auto graph=parse(source);
        auto zero=at(graph,0),young=at(graph,.3),adult=at(graph,1);
        expect(output(zero,"wood.glb").triangles.empty(),"growth at zero must be empty");
        expect(node(at(graph,.12),"branches")["development"]["visible"]==0,"branches appeared before trunk reached their anchors");
        expect(node(young,"branches")["development"]["visible"]==1,"lower branch must emerge before upper branch");
        expect(node(at(graph,.4),"branches")["development"]["visible"]==2,"upper branch did not emerge at its own anchor");
        expect(std::abs(highY(output(young,"trunk.glb"))-2.4f)<1e-5f,"trunk tip must extend over the mature path");
        float radius=length(output(young,"branches.glb").vertices.front().position-Vec3{0,1,0});
        expect(std::abs(radius-.028f)<1e-5f,"branch thickness must not multiply parent and child maturity");
        auto thick1=at(graph,.8),thick2=at(graph,.9);
        expect(highY(output(thick1,"trunk.glb"))==highY(output(thick2,"trunk.glb")),"completed trunk should retain height during thickening");
        expect(length(output(thick2,"trunk.glb").vertices[0].position)>length(output(thick1,"trunk.glb").vertices[0].position),"trunk must thicken independently of length");
        auto baby=at(graph,.31),larger=at(graph,.4);
        expect(output(at(graph,.28),"leaves.glb").vertices.empty(),"leaf appeared before branch reached its attachment");
        expect(output(baby,"leaves.glb").vertices.size()==4,"one baby leaf should emerge independently");
        float small=length(output(baby,"leaves.glb").vertices[2].position-output(baby,"leaves.glb").vertices[0].position);
        float large=length(output(larger,"leaves.glb").vertices[2].position-output(larger,"leaves.glb").vertices[0].position);
        expect(small>0&&small<large*.4f,"baby phase should precede leaf expansion");
        expect(output(baby,"leaves.glb").vertices[0].color==output(adult,"leaves.glb").vertices[0].color,"hidden offspring must not shift surviving random variation");
        auto pointsStage=source;
        pointsStage["growth"]["stages"]["sites"]=pointsStage["growth"]["stages"]["leaves"];
        pointsStage["growth"]["stages"].erase("leaves");
        auto stagedPoints=at(parse(pointsStage),.31);
        for(size_t i=0;i<4;++i) expect(length(output(stagedPoints,"leaves.glb").vertices[i].position-output(baby,"leaves.glb").vertices[i].position)<1e-6f,"point stages must not be scaled again by an unstaged instance");
        write(graph,generate(graph),work/"mature"); write(graph,adult,work/"final");
        for(auto key:{"trunk.glb","branches.glb","leaves.glb","wood.glb"}) expect(bytes(work/"mature"/key)==bytes(work/"final"/key),"mature endpoint changed");
        for(double progress:{1e-14,1e-10,1e-7,.125-1e-9,.125,.125+1e-9,.375-1e-9,.375,.375+1e-9,.5-1e-9,.5,.5+1e-9,.999999}) at(graph,progress);
        for(int frame=0;frame<=128;++frame) {
            auto result=at(graph,double(frame)/128);
            for(const auto& [name,value]:result.outputs) checkMesh(value->mesh);
            auto report=node(result,"solid");
            if(report.contains("solid")) expect(report["solid"]["components"]==1&&report["solid"]["watertight"]==true,"overlapping stages must retain connected solid wood");
        }
        auto curved=source; curved["nodes"]["trunk"]={{"op","curve"},{"points",{{0,0,0},{0,2,0},{2,4,0}}},{"radius",.2},{"taper",0}};
        curved["outputs"]={{"trunk.glb","trunk_mesh"}};
        auto extension=at(parse(curved),.25);
        expect(highY(output(extension,"trunk.glb"))>2.2f,"curved stem was uniformly squashed instead of extended along arc length");
        auto invalid=source; invalid["growth"]["stages"]["trunk"]["length_profile"]={{0,0},{.5,.8},{.7,.4},{1,1}}; fails([&]{parse(invalid);});
        invalid=source; invalid["growth"]["stages"]["trunk"]["radius_profile"]={{0,-.1},{1,1}}; fails([&]{parse(invalid);});
        invalid=source; invalid["growth"]["stages"]["leaves"]["length_profile"]={{0,0},{1,1}}; fails([&]{parse(invalid);});
        invalid=source; invalid["growth"]["mode"]="unknown"; fails([&]{parse(invalid);});
        invalid=source; invalid["growth"]["stages"]["branches"]["end"]=.1; fails([&]{at(parse(invalid),.5);});
        invalid=source; invalid["growth"]["stages"]["leaves"]["attachment_delay"]=1; fails([&]{parse(invalid);});
        Options limited; limited.growth=0; limited.limits.points=1; fails([&]{generate(graph,limited);});
        auto atlas=source; atlas["nodes"]["leaves"]["atlas"]=(fixtures/"assets/spritesheets/leaves.atlas.json").string();
        auto atlasGraph=parse(atlas); auto atlasBaby=at(atlasGraph,.31),atlasFull=at(atlasGraph,1);
        for(size_t i=0;i<4;++i) expect(output(atlasBaby,"leaves.glb").vertices[i].uv.x==output(atlasFull,"leaves.glb").vertices[i].uv.x&&output(atlasBaby,"leaves.glb").vertices[i].uv.y==output(atlasFull,"leaves.glb").vertices[i].uv.y,"growth changed a surviving atlas cell");
        atlas["nodes"]["upper"]={{"op","prune"},{"input","branches"},{"center",{0,1,0}},{"radius",2},{"inside",false}};
        atlas["nodes"]["upper_mesh"]={{"op","tube"},{"input","upper"},{"sides",8}};
        atlas["nodes"]["sites"]["proximity_mesh"]="upper_mesh";
        atlas["nodes"]["sites"]["max_distance"]=.025;
        atlas["nodes"]["sites"]["snap_to_mesh"]=true;
        auto filteredGraph=parse(atlas); auto filteredYoung=at(filteredGraph,.65),filteredAdult=at(filteredGraph,1);
        expect(node(filteredYoung,"leaves")["instances"]["candidates"]==2&&node(filteredAdult,"leaves")["instances"]["candidates"]==2,"proximity must retain the mature candidate membership during development");
        expect(node(filteredYoung,"sites")["proximity"]["visible"]==1&&node(filteredYoung,"sites")["proximity"]["snapped"]==1,"proximity diagnostics must distinguish visible attachments from hidden candidate slots");
        expect(node(at(filteredGraph,0),"sites")["proximity"]["snapped"]==0,"unborn attachments must not be reported as visibly snapped");
        expect(output(filteredYoung,"leaves.glb").vertices.size()==4,"upper baby leaf did not emerge after unsupported lower candidates were removed");
        for(size_t i=0;i<4;++i) {
            const auto& a=output(filteredYoung,"leaves.glb").vertices[i]; const auto& b=output(filteredAdult,"leaves.glb").vertices[i];
            expect(a.uv.x==b.uv.x&&a.uv.y==b.uv.y&&a.color==b.color,"proximity changed a surviving atlas cell or color stream");
        }
        std::cout<<checks<<" developmental checks passed\n";
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
