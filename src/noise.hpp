#pragma once
#include "internal.hpp"

namespace foliage {
inline float coherentNoise(Vec3 position, double frequency, uint64_t seed) {
    int64_t cell[3]; float blend[3]; double values[3]={position.x,position.y,position.z};
    for(int axis=0;axis<3;++axis) {
        require(std::isfinite(values[axis]),"nonfinite noise coordinate");
        // Periodic wrapping also keeps large, transformed coordinates safe for integer conversion.
        double value=std::fmod(values[axis]*frequency,1048576.0),floor=std::floor(value);
        cell[axis]=static_cast<int64_t>(floor); float t=float(value-floor);
        blend[axis]=t*t*t*(t*(t*6-15)+10);
    }
    float result=0;
    for(int z=0;z<2;++z) for(int y=0;y<2;++y) for(int x=0;x<2;++x) {
        Random random{seed^((uint64_t(cell[0]+x)&1048575)*0x9e3779b97f4a7c15ull)^((uint64_t(cell[1]+y)&1048575)*0xbf58476d1ce4e5b9ull)^((uint64_t(cell[2]+z)&1048575)*0x94d049bb133111ebull)};
        result+=random.signedUnit()*(x?blend[0]:1-blend[0])*(y?blend[1]:1-blend[1])*(z?blend[2]:1-blend[2]);
    }
    return result;
}
inline float layeredNoise(Vec3 position, float frequency, int octaves, uint64_t seed) {
    float result=0,weight=1,total=0; double scale=frequency;
    for(int i=0;i<octaves;++i) { result+=weight*coherentNoise(position,scale,seed+uint64_t(i)); total+=weight; weight*=.5f; scale*=2; }
    return result/total;
}
}
