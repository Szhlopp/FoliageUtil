#pragma once
#include "foliage/foliage.hpp"
#include <stdexcept>

namespace foliage {
inline void require(bool ok, const std::string& message) { if(!ok) throw std::runtime_error(message); }
inline Vec3 vector3(const Json& j) { return {j.at(0).get<float>(),j.at(1).get<float>(),j.at(2).get<float>()}; }
struct Budget {
    Limits limit;
    size_t vertices=0, triangles=0, points=0;
    void charge(size_t v, size_t t, size_t p) {
        require(v<=limit.vertices-vertices,"vertex budget exceeded");
        require(t<=limit.triangles-triangles,"triangle budget exceeded");
        require(p<=limit.points-points,"growth/scatter point budget exceeded");
        vertices+=v; triangles+=t; points+=p;
    }
};
using Inputs = std::map<std::string,std::shared_ptr<const Value>>;
struct GrowthState {
    double birth=0, end=1, progress=1;
    bool smooth=false, visible=true;
    float visible_length=1, attachment=0, tip_length=.15f, tip_radius=.12f;
    Json length_profile, radius_profile;
    std::shared_ptr<const GrowthState> support;
};
struct GrowthContext { double progress=1; const Json* stage=nullptr; const Mesh* mature_proximity=nullptr; };
Value evaluate(const Json& node, const Inputs& inputs, Random& random, Budget& budget, const std::filesystem::path& base, Json* diagnostics=nullptr, const GrowthContext* growth=nullptr);
Mesh solidify(const Mesh& mesh, const Json& node, Budget& budget, Json* diagnostics);
Mesh booleanMesh(const Mesh& input, const Mesh& tool, const Json& node, Budget& budget, Json* diagnostics);
Mesh mineralMesh(const Json& node, Random& random, Budget& budget);
void validateLods(Graph& graph);
void validateGrowth(Graph& graph);
void validateCardBakes(Graph& graph);
void validateExportProfiles(Graph& graph);
Json exportAlembicSamples(const Graph& graph, const Json& profile, const std::filesystem::path& directory, const Options& options);
Result generateNodes(const Graph& graph, const Options& options, const std::vector<std::string>& names);
float growthFactor(const Graph& graph, const std::string& id, double progress);
void growValue(Value& value, const Json& stage, float factor);
void developValue(Value& value, const Json& node, const GrowthContext& context, Json& diagnostics);
double growthReach(const std::shared_ptr<const GrowthState>& state, float attachment);
float growthRadius(const std::shared_ptr<const GrowthState>& state, float attachment);
float growthScale(const Json* stage, double progress, double available);
std::vector<Sample> visibleSamples(const Stem& stem);
float geometryResolution(Vec3 position);
void preflightExports(const std::vector<Graph>& graphs, const std::filesystem::path& directory, const std::vector<std::string>& manifests={});
std::vector<std::string> references(const Json& node);
void validateFields(Json& object, const Json& fields, const std::string& context);
Json materialFields();
struct Atlas { std::vector<std::array<float,4>> regions; std::vector<std::filesystem::path> files; };
Atlas readAtlas(const std::filesystem::path& path);
}
