#pragma once
#include "foliage/foliage.hpp"

namespace foliage {
struct SurfaceHit { Vec3 position; double distanceSquared; size_t triangle; };
// A bounded, balanced triangle index, shared by every candidate in one scatter node.
class MeshProximity {
public:
    explicit MeshProximity(const Mesh& mesh);
    std::optional<SurfaceHit> nearest(Vec3 position, float maxDistance) const;
private:
    struct Node { Vec3 lo, hi; size_t begin=0, end=0, left=0, right=0; };
    const Mesh& mesh_;
    std::vector<size_t> order_;
    std::vector<Node> nodes_;
    size_t build(size_t begin, size_t end);
    void visit(size_t node, Vec3 position, double& best, std::optional<SurfaceHit>& hit) const;
};
}
