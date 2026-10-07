#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace foliage {
constexpr float pi = 3.14159265358979323846f;
struct Vec2 { float x=0, y=0; };
struct Vec3 { float x=0, y=0, z=0; };
inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline Vec3 operator*(Vec3 a, float b) { return {a.x*b,a.y*b,a.z*b}; }
inline Vec3 operator/(Vec3 a, float b) { return a*(1/b); }
inline float dot(Vec3 a, Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline float length(Vec3 a) { return std::sqrt(dot(a,a)); }
inline Vec3 normalized(Vec3 a) { float n=length(a); return n>1e-15f?a/n:Vec3{0,1,0}; }
inline Vec3 mix(Vec3 a, Vec3 b, float t) { return a*(1-t)+b*t; }
inline float radians(float degrees) { return degrees*pi/180; }
inline float clamp01(float x) { return std::clamp(x,0.0f,1.0f); }
inline bool finite(Vec3 p) { return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z); }
struct Frame {
    Vec3 x{1,0,0}, y{0,1,0}, z{0,0,1};
    Vec3 apply(Vec3 p) const { return x*p.x+y*p.y+z*p.z; }
};
inline Frame frame(Vec3 direction, float roll=0) {
    Vec3 y=normalized(direction), x=normalized(cross(y,std::abs(y.z)<0.9f?Vec3{0,0,1}:Vec3{1,0,0})), z=cross(x,y);
    return {x*std::cos(roll)+z*std::sin(roll),y,z*std::cos(roll)-x*std::sin(roll)};
}
inline Vec3 rotate(Vec3 p, Vec3 degrees) {
    float x=radians(degrees.x),y=radians(degrees.y),z=radians(degrees.z);
    p={p.x,p.y*std::cos(x)-p.z*std::sin(x),p.y*std::sin(x)+p.z*std::cos(x)};
    p={p.x*std::cos(y)+p.z*std::sin(y),p.y,-p.x*std::sin(y)+p.z*std::cos(y)};
    return {p.x*std::cos(z)-p.y*std::sin(z),p.x*std::sin(z)+p.y*std::cos(z),p.z};
}
inline uint64_t hashName(const std::string& name) { uint64_t h=14695981039346656037ull; for(unsigned char c:name) { h^=c; h*=1099511628211ull; } return h; }
struct Random {
    uint64_t state;
    uint64_t next() { uint64_t z=(state+=0x9e3779b97f4a7c15ull); z=(z^(z>>30))*0xbf58476d1ce4e5b9ull; z=(z^(z>>27))*0x94d049bb133111ebull; return z^(z>>31); }
    float unit() { return static_cast<float>(next()>>40)*(1.0f/16777216.0f); }
    float signedUnit() { return 2*unit()-1; }
};
}
