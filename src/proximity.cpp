#include "proximity.hpp"
#include <algorithm>
#include <numeric>

namespace foliage {
namespace {
struct DVec { double x, y, z; };
DVec precise(Vec3 p) { return {p.x,p.y,p.z}; }
DVec operator+(DVec a, DVec b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
DVec operator-(DVec a, DVec b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
DVec operator*(DVec a, double s) { return {a.x*s,a.y*s,a.z*s}; }
double dot(DVec a, DVec b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
DVec cross(DVec a, DVec b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
double distanceSquared(DVec a, DVec b) { auto d=a-b; return dot(d,d); }
DVec segmentPoint(DVec p, DVec a, DVec b) { auto edge=b-a; double d=dot(edge,edge); return a+edge*(d>0?std::clamp(dot(p-a,edge)/d,0.0,1.0):0); }
DVec trianglePoint(Vec3 position, Vec3 va, Vec3 vb, Vec3 vc) {
    auto p=precise(position),a=precise(va),b=precise(vb),c=precise(vc),normal=cross(b-a,c-a);
    double area=dot(normal,normal);
    if(area>0) {
        auto q=p-normal*(dot(p-a,normal)/area);
        if(dot(cross(b-a,q-a),normal)>=0&&dot(cross(c-b,q-b),normal)>=0&&dot(cross(a-c,q-c),normal)>=0) return q;
    }
    auto ab=segmentPoint(p,a,b),bc=segmentPoint(p,b,c),ca=segmentPoint(p,c,a);
    auto best=distanceSquared(p,ab)<=distanceSquared(p,bc)?ab:bc;
    return distanceSquared(p,ca)<distanceSquared(p,best)?ca:best;
}
double coordinate(Vec3 v, int axis) { return axis==0?v.x:axis==1?v.y:v.z; }
double boxDistance(Vec3 p, Vec3 lo, Vec3 hi) {
    double result=0;
    for(int axis=0;axis<3;++axis) { double x=coordinate(p,axis),d=std::max({coordinate(lo,axis)-x,0.0,x-coordinate(hi,axis)}); result+=d*d; }
    return result;
}
}
MeshProximity::MeshProximity(const Mesh& mesh) : mesh_(mesh), order_(mesh.triangles.size()) {
    // The mesh was already charged against the triangle budget before evaluation.
    // Only indices and bounding boxes are allocated here, not replicated geometry.
    std::iota(order_.begin(),order_.end(),size_t(0));
    nodes_.reserve(order_.size()/4+1);
    if(!order_.empty()) build(0,order_.size());
}
size_t MeshProximity::build(size_t begin, size_t end) {
    Node node; node.begin=begin; node.end=end;
    node.lo=node.hi=mesh_.vertices[mesh_.triangles[order_[begin]].vertices[0]].position;
    for(size_t i=begin;i<end;++i) for(auto vertex:mesh_.triangles[order_[i]].vertices) {
        auto p=mesh_.vertices[vertex].position;
        node.lo={std::min(node.lo.x,p.x),std::min(node.lo.y,p.y),std::min(node.lo.z,p.z)};
        node.hi={std::max(node.hi.x,p.x),std::max(node.hi.y,p.y),std::max(node.hi.z,p.z)};
    }
    size_t index=nodes_.size(); nodes_.push_back(node);
    if(end-begin<=8) return index;
    int axis=0;
    for(int a=1;a<3;++a) if(coordinate(node.hi,a)-coordinate(node.lo,a)>coordinate(node.hi,axis)-coordinate(node.lo,axis)) axis=a;
    auto center=[&](size_t triangle) { double sum=0; for(auto v:mesh_.triangles[triangle].vertices) sum+=coordinate(mesh_.vertices[v].position,axis); return sum; };
    size_t middle=begin+(end-begin)/2;
    std::nth_element(order_.begin()+begin,order_.begin()+middle,order_.begin()+end,[&](size_t a,size_t b) { double x=center(a),y=center(b); return x==y?a<b:x<y; });
    size_t left=build(begin,middle),right=build(middle,end);
    nodes_[index].left=left; nodes_[index].right=right;
    return index;
}
void MeshProximity::visit(size_t index, Vec3 position, double& best, std::optional<SurfaceHit>& hit) const {
    const auto& node=nodes_[index];
    if(boxDistance(position,node.lo,node.hi)>best) return;
    if(node.left) {
        auto a=node.left,b=node.right;
        if(boxDistance(position,nodes_[b].lo,nodes_[b].hi)<boxDistance(position,nodes_[a].lo,nodes_[a].hi)) std::swap(a,b);
        visit(a,position,best,hit); visit(b,position,best,hit); return;
    }
    for(size_t i=node.begin;i<node.end;++i) {
        size_t triangle=order_[i]; const auto& face=mesh_.triangles[triangle];
        auto q=trianglePoint(position,mesh_.vertices[face.vertices[0]].position,mesh_.vertices[face.vertices[1]].position,mesh_.vertices[face.vertices[2]].position);
        double d=distanceSquared(precise(position),q);
        if(d<=best&&(!hit||d<best||triangle<hit->triangle)) { best=d; hit=SurfaceHit{{float(q.x),float(q.y),float(q.z)},d,triangle}; }
    }
}
std::optional<SurfaceHit> MeshProximity::nearest(Vec3 position, float maxDistance) const {
    std::optional<SurfaceHit> hit; double best=double(maxDistance)*maxDistance;
    if(!nodes_.empty()) visit(0,position,best,hit);
    return hit;
}
}
