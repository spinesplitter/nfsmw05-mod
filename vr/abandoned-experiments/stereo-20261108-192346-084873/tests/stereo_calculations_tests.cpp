#include "../src/stereo_native_calculations.h"
#include <assert.h>
#include <stdio.h>
#include <limits>

int main() {
    static StereoNativeCalculations cache;
    StereoTransformKey key{};
    key.local.m[0][0]=2; key.view.m[0][0]=3;
    int calls=0;
    auto multiply=[&](M4& out) { ++calls; out={}; out.m[0][0]=key.local.m[0][0]*key.view.m[0][0]; };
    M4 out;
    cache.Begin(true,false);
    cache.Transform(key,out,multiply); assert(out.m[0][0]==6 && calls==1);
    cache.Transform(key,out,multiply); assert(out.m[0][0]==6 && calls==1);
    key.view.m[0][0]=4; // other eye must not reuse the first eye's product
    cache.Transform(key,out,multiply); assert(out.m[0][0]==8 && calls==2);
    key.local.m[0][0]=5; // mutated matrix at the same address
    cache.Transform(key,out,multiply); assert(out.m[0][0]==20 && calls==3);
    key.control[1]=1;
    cache.Transform(key,out,multiply); assert(calls==4);
    cache.End(); cache.Transform(key,out,multiply); assert(calls==5);
    cache.Begin(true,false); cache.Transform(key,out,multiply); assert(calls==6);
    key.local.m[0][0]=std::numeric_limits<float>::quiet_NaN();
    cache.Transform(key,out,multiply); cache.Transform(key,out,multiply); assert(calls==8);

    StereoBoundsKey bounds{};
    bounds.minimum={-1,-1,-1}; bounds.maximum={1,1,1};
    int boundsCalls=0,answer=1;
    auto native=[&]() { ++boundsCalls; return answer; };
    assert(cache.Bounds(bounds,native)==1 && boundsCalls==1);
    assert(cache.Bounds(bounds,native)==1 && boundsCalls==1);
    bounds.planes[0]=1; answer=0; // distinct eye frustum
    assert(cache.Bounds(bounds,native)==0 && boundsCalls==2);
    bounds.minimum.x=2; // invalid bounds always native
    cache.Bounds(bounds,native); cache.Bounds(bounds,native); assert(boundsCalls==4);
    bounds.minimum.x=-1;
    cache.Begin(true,true); answer=2;
    cache.Bounds(bounds,native); cache.Bounds(bounds,native); assert(boundsCalls==6);
    answer=0;
    assert(cache.Bounds(bounds,native)==0 && cache.Failed());
    cache.Bounds(bounds,native); assert(boundsCalls==8); // latched native fallback
    cache.Begin(true,false); cache.Bounds(bounds,native); assert(boundsCalls==9);

    static StereoNativeCalculations transformVerifier;
    key={}; transformVerifier.Begin(true,true);
    transformVerifier.Transform(key,out,multiply);
    auto disagree=[&](M4& value) { value={}; value.m[0][0]=1; };
    transformVerifier.Transform(key,out,disagree);
    assert(transformVerifier.Failed() && out.m[0][0]==1);

    StereoCalculationCache<uint32_t,uint32_t,1> collision;
    collision.Begin(); uint32_t value=0;
    collision.Put(1,7); assert(collision.Get(1,value) && value==7);
    collision.Put(2,9); assert(!collision.Get(1,value));
    assert(collision.Get(2,value) && value==9);
    collision.End(); assert(!collision.Get(2,value));
    collision.Begin(); assert(!collision.Get(2,value));
    puts("stereo calculations: all tests passed");
}