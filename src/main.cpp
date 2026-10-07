#include "foliage/foliage.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
void help() {
    std::cout<<"FoliageUtil 0.1.0: seeded procedural foliage meshes from JSON graphs\n\n"
        "  foliageutil [generate] recipe.json [--out DIR] [--seed N] [--json]\n"
        "  foliageutil validate recipe.json [--json]\n"
        "  foliageutil nodes [--json]\n"
        "  foliageutil materials [--json]\n"
        "  foliageutil lods [--json]\n"
        "  foliageutil growth [--json]\n"
        "  foliageutil card-bakes [--json]\n"
        "  foliageutil exports [--json]\n"
        "  foliageutil describe OP\n\n"
        "Use - to read a recipe from stdin. Outputs are OBJ+MTL or GLB.\n"
        "--max-vertices N / --max-triangles N / --max-points N limit cumulative\n"
        "generated data across evaluated nodes (defaults 4000000/4000000/500000).\n"
        "The output directory defaults to out. Files are overwritten.\n";
    std::cout<<"--card-bake NAME exports a saved fitted-card bake instead of regular outputs.\n";
    std::cout<<"--export NAME selects a saved Alembic or mesh/CardBake stage package.\nNative Alembic support: "<<(foliage::alembicAvailable()?"enabled":"disabled")<<".\n";
    std::cout<<"Configured LODs export automatically. --no-lods skips lower levels;\n--lod NAME exports one configured level.\nConfigured growth exports steps automatically. --no-growth exports mature meshes;\n--growth-step N exports one zero-based step.\n";
}
long long integer(const std::string& value) { size_t end=0; long long n=std::stoll(value,&end); if(end!=value.size()) throw std::runtime_error("expected integer: "+value); return n; }
}
int main(int argc, char** argv) {
    try {
        using namespace foliage;
        if(argc==1||std::string(argv[1])=="--help"||std::string(argv[1])=="-h") { help(); return 0; }
        if(std::string(argv[1])=="--version") { std::cout<<"0.1.0\n"; return 0; }
        std::string command=argv[1];
        if(command=="materials"||command=="lods"||command=="growth"||command=="card-bakes"||command=="exports") {
            if(argc>3||(argc==3&&std::string(argv[2])!="--json")) throw std::runtime_error("usage: foliageutil "+command+" [--json]");
            auto fields=command=="exports"?exportFields():command=="card-bakes"?cardBakeFields():command=="lods"?lodFields():command=="growth"?growthFields():materialFields(); if(argc==3) std::cout<<fields.dump(2)<<'\n'; else for(auto it=fields.begin();it!=fields.end();++it) std::cout<<it.key()<<" ("<<it.value()["type"].get<std::string>()<<"): "<<it.value()["description"].get<std::string>()<<'\n'; return 0;
        }
        if(command=="nodes") {
            if(argc>3||(argc==3&&std::string(argv[2])!="--json")) throw std::runtime_error("usage: foliageutil nodes [--json]");
            auto c=catalog(); if(argc==3) std::cout<<c.dump(2)<<'\n'; else for(auto it=c.begin();it!=c.end();++it) std::cout<<it.key()<<" -> "<<it.value()["output"].get<std::string>()<<": "<<it.value()["description"].get<std::string>()<<'\n'; return 0;
        }
        if(command=="describe") { if(argc!=3) throw std::runtime_error("usage: foliageutil describe OP"); auto c=catalog(); if(!c.contains(argv[2])) throw std::runtime_error("unknown node: "+std::string(argv[2])); std::cout<<c[argv[2]].dump(2)<<'\n'; return 0; }
        bool validate=command=="validate",json=false,noLods=false,noGrowth=false; std::optional<size_t> growthStep; std::string lod,cardBake,exportName; int arg=command=="generate"||validate?2:1;
        if(arg>=argc) throw std::runtime_error("missing recipe filename"); std::string filename=argv[arg++]; std::filesystem::path output="out"; Options options;
        for(;arg<argc;++arg) {
            std::string option=argv[arg]; if(option=="--json") { json=true; continue; }
            if(option=="--no-growth") { if(validate) throw std::runtime_error("validate supports only --json"); noGrowth=true; continue; }
            if(option=="--no-lods") { if(validate) throw std::runtime_error("validate supports only --json"); noLods=true; continue; }
            if(option!="--export"&&option!="--card-bake"&&option!="--growth-step"&&option!="--lod"&&option!="--out"&&option!="--seed"&&option!="--max-vertices"&&option!="--max-triangles"&&option!="--max-points") throw std::runtime_error("unknown option: "+option);
            if(validate) throw std::runtime_error("validate supports only --json");
            if(arg+1>=argc) throw std::runtime_error("missing value for "+option); std::string value=argv[++arg];
            if(option=="--out") output=value;
            else if(option=="--export") { if(value.empty()) throw std::runtime_error("export name cannot be empty"); exportName=value; }
            else if(option=="--card-bake") { if(value.empty()) throw std::runtime_error("CardBake name cannot be empty"); cardBake=value; }
            else if(option=="--growth-step") { auto n=integer(value); if(n<0||n>31) throw std::runtime_error("growth step must be 0..31"); growthStep=size_t(n); }
            else if(option=="--lod") { if(value.empty()) throw std::runtime_error("LOD name cannot be empty"); lod=value; }
            else { auto n=integer(value); if(option=="--seed") { if(n<-2147483648ll||n>2147483647ll) throw std::runtime_error("seed must be signed 32-bit"); options.seed=int32_t(n); }
                else { if(n<1||n>100000000) throw std::runtime_error("budgets must be 1..100000000"); if(option=="--max-vertices") options.limits.vertices=size_t(n); if(option=="--max-triangles") options.limits.triangles=size_t(n); if(option=="--max-points") options.limits.points=size_t(n); }
            }
        }
        Graph graph;
        if(filename=="-") {
            std::string data; char block[4096]; while(std::cin.read(block,sizeof(block))||std::cin.gcount()) { data.append(block,size_t(std::cin.gcount())); if(data.size()>8*1024*1024) throw std::runtime_error("recipe exceeds 8 MiB"); }
            graph=parse(Json::parse(data));
        } else graph=load(filename);
        if(validate) { if(json) std::cout<<Json({{"valid",true},{"nodes",graph.order.size()},{"outputs",graph.document["outputs"].size()}}).dump(2)<<'\n'; else std::cout<<"Valid graph: "<<graph.order.size()<<" nodes\n"; return 0; }
        if(noLods&&!lod.empty()) throw std::runtime_error("--lod and --no-lods cannot be combined");
        if(noGrowth&&growthStep) throw std::runtime_error("--growth-step and --no-growth cannot be combined");
        if(!exportName.empty()&&(!cardBake.empty()||!lod.empty()||growthStep||noLods||noGrowth)) throw std::runtime_error("--export cannot combine with --card-bake or LOD/growth selection flags; configure the saved export profile");
        if(!cardBake.empty()&&(!lod.empty()||growthStep||noLods||noGrowth)) throw std::runtime_error("--card-bake cannot be combined with LOD/growth flags; it bakes the mature source graph");
        if(!lod.empty()) graph=selectLod(graph,lod);
        auto report=!exportName.empty()?exportProfile(graph,exportName,output,options):cardBake.empty()?exportGraph(graph,output,options,!noLods&&lod.empty(),!noGrowth,growthStep):exportCardBake(graph,cardBake,output,options); if(!lod.empty()) report["level"]=lod;
        if(json) std::cout<<report.dump(2)<<'\n';
        else {
            auto print=[](const Json& level) { for(const auto& file:level.value("outputs",Json::array())) std::cout<<file["path"].get<std::string>()<<" ("<<file["triangles"]<<" triangles)\n"; };
            print(report); if(report.contains("lods")) for(const auto& level:report["lods"]) print(level); if(report.contains("growth")) for(const auto& level:report["growth"]) print(level);
            if(report.contains("manifest")) std::cout<<report["manifest"].get<std::string>()<<'\n';
        }
        return 0;
    } catch(const std::exception& e) { std::cerr<<"foliageutil: "<<e.what()<<'\n'; return 1; }
}
