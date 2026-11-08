#include "../src/mirror_vertex_map.h"
#include "../src/mirror_stream_range.h"
#include <assert.h>
#include <math.h>
#include <random>
#include <stdio.h>
#include <string.h>
#include <vector>

int main() {
    MirrorStreamRange range;
    assert(MirrorDrawRange(100,20,4,16,32,8192,true,range));
    assert(range.begin==3856 && range.bytes==128 && range.base==-20);
    assert(MirrorVertexInRange(range,100,20,16,32));
    assert(!MirrorVertexInRange(range,100,19,16,32));
    assert(!MirrorVertexInRange(range,100,24,16,32));
    assert(MirrorDrawRange(-10,20,4,16,32,8192,true,range));
    assert(range.begin==336 && range.base==-20);
    assert(MirrorDrawRange(100,20,4,16,32,8192,false,range));
    assert(range.begin==0 && range.bytes==8192 && range.base==100);
    assert(!MirrorDrawRange(-30,20,4,16,32,8192,true,range));
    assert(!MirrorDrawRange(0,0,0,0,32,8192,true,range));
    assert(!MirrorDrawRange(0,0,0xffffffffU,0,32,8192,true,range));
    assert(!MirrorDrawRange(0,0,4,0,0,8192,true,range));
    std::mt19937 rng(1729);
    for (unsigned i=0;i<10000;++i) {
        const unsigned minimum=rng()%1000, count=1+rng()%100;
        const int base=(int)(rng()%1000)-(int)minimum;
        const unsigned offset=rng()%64, stride=20+(rng()%20)*4;
        assert(MirrorDrawRange(base,minimum,count,offset,stride,200000,true,range));
        std::vector<unsigned char> original(200000);
        for (size_t b=0;b<original.size();++b) original[b]=(unsigned char)(b*37+i);
        std::vector<unsigned char> compact(original.begin()+range.begin,
                                           original.begin()+range.begin+range.bytes);
        for (unsigned v=minimum;v<minimum+count;++v) {
            const uint64_t oldAddress=offset+((int64_t)base+v)*stride;
            const uint64_t newAddress=((int64_t)range.base+v)*stride;
            assert(oldAddress==range.begin+newAddress);
            assert(MirrorVertexInRange(range,base,v,offset,stride));
            assert(!memcmp(original.data()+oldAddress,compact.data()+newAddress,stride));
        }
    }
    puts("PASS: compact mirror spans, multi-stream fallback, bounds and 10000 index-rebasing cases");
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