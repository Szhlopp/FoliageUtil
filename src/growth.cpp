#include "internal.hpp"
#include <limits>

namespace foliage {
namespace {
double curve(const Json& keys, double t) {
    for(size_t i=1;i<keys.size();++i) if(t<=keys[i][0].get<double>()) {
        double a=keys[i-1][0],b=keys[i][0],x=keys[i-1][1],y=keys[i][1];
        return x+(y-x)*std::clamp((t-a)/(b-a),0.0,1.0);
    }
    return keys.back()[1];
}
double age(double progress, double birth, double end, bool smooth) {
    double t=std::clamp((progress-birth)/(end-birth),0.0,1.0);
    return smooth?t*t*(3-2*t):t;
}
double birthTime(const Json* stage, double available) {
    double end=stage?stage->at("end").get<double>():1;
    double birth=std::max(available,stage?stage->at("start").get<double>():0);
    require(birth<end,"developmental stage ends before its supporting attachment emerges; extend end or grow the parent earlier");
    return birth+(end-birth)*(stage?stage->at("attachment_delay").get<double>():0);
}
void validateCurve(const Json& keys, const std::string& label, bool zeroStart) {
    require(keys.is_array()&&keys.size()>=2&&keys.size()<=32,label+" must have 2..32 keys");
    double previous=-1,amount=-1;
    for(const auto& key:keys) {
        require(key.is_array()&&key.size()==2,label+" keys must be [age, amount]");
        for(const auto& n:key) require(n.is_number()&&std::isfinite(n.get<double>())&&n>=0&&n<=1,label+" values must be in 0..1");
        require(key[0].get<double>()>previous,label+" ages must strictly increase");
        require(key[1].get<double>()>=amount,label+" amounts must not decrease");
        previous=key[0]; amount=key[1];
    }
    require(keys.front()[0]==0&&keys.back()[0]==1&&keys.back()[1]==1,label+" must span age 0..1 and finish at amount 1");
    if(zeroStart) require(keys.front()[1]==0,label+" must begin at amount 0");
}
}
Json growthFields() {
    return {
        {"mode",{{"type","enum"},{"default","scale"},{"values",{"scale","developmental"}},{"description","scale preserves legacy per-node scaling. developmental advances tips along mature guides, thickens radii independently and activates offspring when support reaches each attachment."}}},
        {"steps",{{"type","integer"},{"default",5},{"min",2},{"max",32},{"description","Number of evenly spaced snapshots including progress zero and mature at one. Steps are zero-based."}}},
        {"lods",{{"type","boolean"},{"default",false},{"description","Also export configured LODs at each growth step. Maximum 128 total variants per batch."}}},
        {"stages",{{"type","object"},{"description","Required node-name map: start/end in 0..1 (defaults 0/1), easing linear or smoothstep, optional mesh pivot. In developmental mode skeleton stages accept monotone length_profile and radius_profile keys [local_age, fraction], tip_length (0.001..1, default 0.15 of mature length) and tip_radius (0.01..1, default 0.12 multiplier). Instance/point/mesh stages accept scale_profile. All channels finish at 1. attachment_delay (0..0.95) delays birth by a fraction of time remaining before end. Defaults: length [[0,0],[0.75,1],[1,1]], radius [[0,0.04],[1,1]], scale [[0,0],[1,1]]. Unstaged children of developing stems inherit an automatic developmental schedule. Successive scale-mode stages compound."}}}
    };
}
void validateGrowth(Graph& graph) {
    if(!graph.document.contains("growth")) return;
    auto& config=graph.document["growth"]; require(config.is_object(),"growth must be an object");
    for(auto it=config.begin();it!=config.end();++it) require(growthFields().contains(it.key()),"unknown growth field: "+it.key());
    if(!config.contains("mode")) config["mode"]="scale";
    require(config["mode"]=="scale"||config["mode"]=="developmental","growth.mode must be scale or developmental");
    bool developmental=config["mode"]=="developmental";
    if(!config.contains("steps")) config["steps"]=5;
    if(!config.contains("lods")) config["lods"]=false;
    require(config["steps"].is_number_integer()&&config["steps"]>=2&&config["steps"]<=32,"growth.steps must be an integer in 2..32");
    require(config["lods"].is_boolean(),"growth.lods must be boolean");
    require(config.contains("stages")&&config["stages"].is_object()&&!config["stages"].empty(),"growth.stages must be a nonempty node map");
    for(auto it=config["stages"].begin();it!=config["stages"].end();++it) {
        require(graph.kinds.count(it.key()),"unknown growth stage node: "+it.key()); auto& stage=it.value();
        require(stage.is_object(),"growth stage must be an object: "+it.key());
        for(auto f=stage.begin();f!=stage.end();++f) require(f.key()=="start"||f.key()=="end"||f.key()=="easing"||f.key()=="pivot"||(developmental&&(f.key()=="length_profile"||f.key()=="radius_profile"||f.key()=="scale_profile"||f.key()=="attachment_delay"||f.key()=="tip_length"||f.key()=="tip_radius")),"unknown growth stage field: "+f.key());
        if(!stage.contains("start")) stage["start"]=0;
        if(!stage.contains("end")) stage["end"]=1;
        if(!stage.contains("easing")) stage["easing"]="linear";
        if(!stage.contains("pivot")) stage["pivot"]={0,0,0};
        for(auto key:{"start","end"}) require(stage[key].is_number()&&std::isfinite(stage[key].get<double>())&&stage[key]>=0&&stage[key]<=1,"growth stage times must be within 0..1");
        require(stage["start"]<stage["end"],"growth stage start must precede end");
        require(stage["easing"]=="linear"||stage["easing"]=="smoothstep","growth easing must be linear or smoothstep");
        require(stage["pivot"].is_array()&&stage["pivot"].size()==3,"growth pivot must be a vector3");
        for(const auto& v:stage["pivot"]) require(v.is_number()&&std::isfinite(v.get<double>())&&std::abs(v.get<double>())<=10000,"growth pivot outside bounds");
        auto op=graph.document["nodes"][it.key()]["op"];
        if(graph.kinds.at(it.key())!=Kind::mesh||op=="instance") require(length(vector3(stage["pivot"]))==0,"growth pivot applies only to mesh stages other than instance");
        if(developmental) {
            if(!stage.contains("attachment_delay")) stage["attachment_delay"]=0;
            require(stage["attachment_delay"].is_number()&&std::isfinite(stage["attachment_delay"].get<double>())&&stage["attachment_delay"]>=0&&stage["attachment_delay"]<=.95,"attachment_delay must be in 0..0.95");
            if(graph.kinds.at(it.key())==Kind::skeleton) {
                require(!stage.contains("scale_profile"),"skeleton stages use length_profile and radius_profile");
                if(!stage.contains("length_profile")) stage["length_profile"]={{0,0},{.75,1},{1,1}};
                if(!stage.contains("radius_profile")) stage["radius_profile"]={{0,.04},{1,1}};
                validateCurve(stage["length_profile"],"length_profile",true);
                validateCurve(stage["radius_profile"],"radius_profile",false);
                if(!stage.contains("tip_length")) stage["tip_length"]=.15;
                if(!stage.contains("tip_radius")) stage["tip_radius"]=.12;
                for(const auto* field:{"tip_length","tip_radius"}) require(stage[field].is_number()&&std::isfinite(stage[field].get<double>())&&stage[field]>=(std::string(field)=="tip_length"?.001:.01)&&stage[field]<=1,std::string(field)+" outside bounds");
            } else {
                require(!stage.contains("length_profile")&&!stage.contains("radius_profile")&&!stage.contains("tip_length")&&!stage.contains("tip_radius"),"length_profile, radius_profile and tip controls require a skeleton stage");
                if(!stage.contains("scale_profile")) stage["scale_profile"]={{0,0},{1,1}};
                validateCurve(stage["scale_profile"],"scale_profile",true);
            }
        }
    }
}
float growthFactor(const Graph& graph, const std::string& id, double progress) {
    if(!graph.document.contains("growth")||!graph.document["growth"]["stages"].contains(id)) return 1;
    const auto& stage=graph.document["growth"]["stages"][id];
    double f=std::clamp((progress-stage["start"].get<double>())/(stage["end"].get<double>()-stage["start"].get<double>()),0.0,1.0);
    if(stage["easing"]=="smoothstep") f=f*f*(3-2*f);
    return float(f);
}
void growValue(Value& value, const Json& stage, float factor) {
    for(auto& stem:value.stems) if(!stem.samples.empty()) {
        Vec3 base=stem.samples.front().position;
        for(auto& sample:stem.samples) { sample.position=base+(sample.position-base)*factor; sample.radius*=factor; }
    }
    for(auto& point:value.points) point.scale*=factor;
    Vec3 pivot=vector3(stage["pivot"]);
    for(auto& vertex:value.mesh.vertices) vertex.position=pivot+(vertex.position-pivot)*factor;
}
double growthReach(const std::shared_ptr<const GrowthState>& state, float attachment) {
    if(!state) return 0;
    double low=0,high=1;
    for(int i=0;i<48;++i) {
        double middle=(low+high)*.5,t=state->smooth?middle*middle*(3-2*middle):middle;
        if(curve(state->length_profile,t)<attachment) low=middle; else high=middle;
    }
    return state->birth+(state->end-state->birth)*high;
}
float growthRadius(const std::shared_ptr<const GrowthState>& state, float attachment) {
    if(!state) return 1;
    if(state->progress<=state->birth) return 0;
    double t=age(state->progress,state->birth,state->end,state->smooth);
    float reach=float(curve(state->length_profile,t));
    if(attachment>reach+1e-6f) return 0;
    float radius=float(curve(state->radius_profile,t));
    // A young advancing tip is narrow; the established stem behind it thickens.
    // Fade this extra taper out as the mature tip is reached.
    float span=std::min(state->tip_length,reach),tip=span>0?clamp01((attachment-reach+span)/span):0;
    radius*=1-(1-state->tip_radius)*tip*tip*(3-2*tip)*std::min(1.0f,(1-reach)/state->tip_length);
    if(state->support) radius=std::min(radius,growthRadius(state->support,state->attachment));
    return radius;
}
float geometryResolution(Vec3 position) {
    return std::max(1e-6f,std::max({std::abs(position.x),std::abs(position.y),std::abs(position.z)})*std::numeric_limits<float>::epsilon()*16);
}
std::vector<Sample> visibleSamples(const Stem& stem) {
    if(!stem.growth) return stem.samples;
    const auto& state=*stem.growth;
    if(state.progress<=state.birth||(state.support&&(!state.support->visible||state.attachment>state.support->visible_length))) return {};
    std::vector<float> distances(stem.samples.size());
    for(size_t i=1;i<distances.size();++i) distances[i]=distances[i-1]+length(stem.samples[i].position-stem.samples[i-1].position);
    float extent=distances.back();
    float reach=float(curve(state.length_profile,age(state.progress,state.birth,state.end,state.smooth))),target=extent*reach;
    std::vector<Sample> samples;
    for(size_t i=0;i<stem.samples.size();++i) {
        Sample sample=stem.samples[i]; float distance=distances[i];
        if(distance>target&&i) {
            float t=(target-distances[i-1])/(distance-distances[i-1]);
            sample.position=mix(stem.samples[i-1].position,sample.position,t);
            sample.radius=stem.samples[i-1].radius+(sample.radius-stem.samples[i-1].radius)*t;
            distance=target;
        }
        sample.radius*=growthRadius(stem.growth,distance/extent);
        float epsilon=geometryResolution(sample.position);
        if(sample.radius<epsilon*2) break;
        if(samples.empty()||length(sample.position-samples.back().position)>epsilon*2) samples.push_back(sample);
        if(distances[i]>=target) break;
    }
    if(samples.size()<2) samples.clear();
    return samples;
}
float growthScale(const Json* stage, double progress, double available) {
    double birth=birthTime(stage,available);
    if(progress<=birth) return 0;
    double t=age(progress,birth,stage?stage->at("end").get<double>():1,stage&&stage->at("easing")=="smoothstep");
    return stage?float(curve(stage->at("scale_profile"),t)):float(t);
}
void developValue(Value& value, const Json& node, const GrowthContext& context, Json& diagnostics) {
    size_t visible=0;
    for(auto& stem:value.stems) {
        bool creates=node["op"]=="trunk"||node["op"]=="curve"||node["op"]=="branch"||node["op"]=="roots";
        if(context.stage||(creates&&stem.support)) {
            auto state=std::make_shared<GrowthState>();
            state->support=stem.support; state->attachment=stem.attachment;
            state->birth=birthTime(context.stage,growthReach(stem.support,stem.attachment));
            state->end=context.stage?context.stage->at("end").get<double>():1;
            state->progress=context.progress; state->smooth=context.stage&&context.stage->at("easing")=="smoothstep";
            state->length_profile=context.stage?context.stage->at("length_profile"):Json{{0,0},{.75,1},{1,1}};
            state->radius_profile=context.stage?context.stage->at("radius_profile"):Json{{0,.04},{1,1}};
            if(context.stage) { state->tip_length=context.stage->at("tip_length"); state->tip_radius=context.stage->at("tip_radius"); }
            stem.growth=state;
        }
        if(stem.growth) {
            // Recompute visibility after grow/transform without changing the full guide.
            auto state=std::make_shared<GrowthState>(*stem.growth); stem.growth=state;
            auto samples=visibleSamples(stem); state->visible=!samples.empty();
            float full=0,partial=0;
            for(size_t i=1;i<stem.samples.size();++i) full+=length(stem.samples[i].position-stem.samples[i-1].position);
            for(size_t i=1;i<samples.size();++i) partial+=length(samples[i].position-samples[i-1].position);
            state->visible_length=full>0?partial/full:0;
            visible+=state->visible;
        } else ++visible;
    }
    if(value.kind==Kind::skeleton) diagnostics["development"]={{"guides",value.stems.size()},{"visible",visible}};
    if(value.kind==Kind::points&&context.stage) for(auto& point:value.points) {
        point.scale*=growthScale(context.stage,context.progress,point.available_at);
        point.developing=false;
    }
    if(value.kind==Kind::mesh&&context.stage&&node["op"]!="instance") growValue(value,*context.stage,growthScale(context.stage,context.progress,0));
}
}
