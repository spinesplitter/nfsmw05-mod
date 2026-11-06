#include "../src/mirror_vertex_map.h"
#include <assert.h>
#include <math.h>
#include <random>
#include <stdio.h>
#include <string.h>

int main() {
    std::mt19937 rng(1729);
    std::uniform_real_distribution<float> random(-4,4);
    for (int i=0;i<10000;++i) {
        float b[13], vertex[8], copy[8];
        for (float& x:b) x=random(rng);
        for (float& x:vertex) x=random(rng);
        memcpy(copy,vertex,sizeof(copy));
        float cu=b[0]*b[3]+b[1]*b[4]+b[2]*b[5], cv=b[0]*b[6]+b[1]*b[7]+b[2]*b[8];
        float pu=vertex[0]*b[3]+vertex[1]*b[4]+vertex[2]*b[5];
        float pv=vertex[0]*b[6]+vertex[1]*b[7]+vertex[2]*b[8];
        float s=b[9]>0 ? (pu-(cu-b[9]))/(2*b[9]) : 0.5f;
        float t=b[10]>0 ? ((cv+b[10])-pv)/(2*b[10]) : 0.5f;
        s=s<0 ? 0 : s>1 ? 1 : s; t=t<0 ? 0 : t>1 ? 1 : t;
        MirrorVertexUV(copy,b,copy+6);
        assert(fabsf(copy[6]-(b[11]+(b[12]-b[11])*s))<1e-6f);
        assert(fabsf(copy[7]-t)<1e-6f);
        assert(!memcmp(copy,vertex,6*sizeof(float)));
        float repeated[8]; memcpy(repeated,copy,sizeof(copy));
        MirrorVertexUV(copy,b,copy+6);
        assert(!memcmp(repeated,copy,sizeof(copy)));
    }
    puts("PASS: 10000 mirror UV reference cases, non-UV preservation and overlapping-part idempotence");
}