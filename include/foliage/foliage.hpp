#pragma once
#include "math.hpp"
#include <nlohmann/json.hpp>
#include <array>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace foliage {
using Json = nlohmann::json;
struct Sample { Vec3 position; float radius=0; };
struct GrowthState;
// Developmental stems retain mature guides and opaque growth metadata; the
// surface generators reveal their reached, representable portions.
struct Stem { std::vector<Sample> samples; unsigned depth=0; std::shared_ptr<const GrowthState> growth, support; float attachment=0; };
struct Point { Vec3 position; Frame orientation; float scale=1, phase=0; double available_at=0; bool growth_visible=true, developing=false; };
struct Vertex { Vec3 position, normal; Vec2 uv; std::array<float,4> color{1,1,1,1}; Vec2 wind; };
struct Triangle { std::array<uint32_t,3> vertices; std::string material; };
struct Mesh { std::vector<Vertex> vertices; std::vector<Triangle> triangles; };
enum class Kind { skeleton, points, mesh };
std::string kindName(Kind kind);
struct Value { Kind kind=Kind::mesh; std::vector<Stem> stems; std::vector<Point> points; Mesh mesh; };
struct Limits { size_t vertices=4000000, triangles=4000000, points=500000; };
struct Options { std::optional<int32_t> seed; Limits limits; std::optional<double> growth; };
struct Graph { Json document; std::filesystem::path base; std::vector<std::string> order; std::map<std::string,Kind> kinds; std::string level="base"; };
struct Result { std::map<std::string,std::shared_ptr<const Value>> outputs; Json stats; };
struct CardBakeResult { Mesh mesh; Json materials, report; std::filesystem::path base; };
// Immutable mature bake and guide bindings. Safe for concurrent growth sampling.
struct CardGrowth;
Json catalog();
Json materialFields();
Json lodFields();
Json growthFields();
Json cardBakeFields();
Json exportFields();
bool alembicAvailable();
Json exportProfile(const Graph& graph, const std::string& name, const std::filesystem::path& directory, const Options& options={});
Json exportCardBake(const Graph& graph, const std::string& name, const std::filesystem::path& directory, const Options& options={});
CardBakeResult bakeCards(const Graph& graph, const std::string& name, const std::filesystem::path& directory, const Options& options={});
std::shared_ptr<const CardGrowth> prepareCardGrowth(const Graph& graph, const std::string& name, const std::filesystem::path& directory, const Options& options={});
CardBakeResult growCards(const CardGrowth& prepared, double progress, const Limits& limits={});
Graph parse(const Json& document, const std::filesystem::path& base={});
Graph load(const std::filesystem::path& filename);
Result generate(const Graph& graph, const Options& options={});
Graph selectLod(const Graph& graph, const std::string& name);
Json exportGraph(const Graph& graph, const std::filesystem::path& directory, const Options& options={}, bool includeLods=true, bool includeGrowth=true, std::optional<size_t> growthStep={});
Json meshStats(const Mesh& mesh);
void checkMesh(const Mesh& mesh);
Json write(const Graph& graph, const Result& result, const std::filesystem::path& directory);
void saveObj(const Mesh& mesh, const Json& materials, const std::filesystem::path& base, const std::filesystem::path& filename);
void saveGlb(const Mesh& mesh, const Json& materials, const std::filesystem::path& base, const std::filesystem::path& filename);
}
