#pragma once
#include "stereo_calculation_cache.h"
#include "vrmath.h"
#include <cmath>

// Keys contain values, not addresses. Include the floating-point control mode;
// an altered rounding/precision/denormal mode must not reuse a prior result.
struct StereoTransformKey { M4 local,view; uint32_t control[2]; };
struct StereoBoundsKey { float planes[24]; V3 minimum,maximum; uint32_t control[2]; };
static_assert(sizeof(StereoTransformKey)==136,"key must have no padding");
static_assert(sizeof(StereoBoundsKey)==128,"key must have no padding");

class StereoNativeCalculations {
    StereoCalculationCache<StereoTransformKey,M4,2048> transforms_;
    StereoCalculationCache<StereoBoundsKey,int,2048> bounds_;
    bool active_=false,verify_=false,failed_=false;
    template<class Key> static bool Finite(const Key& key,size_t floats) {
        const unsigned char* bytes=reinterpret_cast<const unsigned char*>(&key);
        for(size_t i=0;i<floats;++i) {
            float value;
            memcpy(&value,bytes+i*sizeof(float),sizeof(value));
            if(!std::isfinite(value)) return false;
        }
        return true;
    }
public:
    uint64_t TransformHits() const { return transforms_.hits; }
    uint64_t BoundsHits() const { return bounds_.hits; }
    uint64_t TransformMisses() const { return transforms_.misses; }
    uint64_t BoundsMisses() const { return bounds_.misses; }
    bool Failed() const { return failed_; }
    void Begin(bool enabled,bool verify) {
        transforms_.Begin(); bounds_.Begin();
        active_=enabled && !failed_; verify_=verify;
    }
    void End() { active_=false; transforms_.End(); bounds_.End(); }
    template<class Native> void Transform(const StereoTransformKey& key,M4& out,Native native) {
        if(!active_ || !Finite(key,32)) { native(out); return; }
        M4 cached;
        if(transforms_.Get(key,cached)) {
            if(verify_) {
                native(out);
                if(memcmp(&out,&cached,sizeof(out))) { failed_=true; End(); }
            } else out=cached;
            return;
        }
        native(out);
        // NaN/overflow outputs are not reusable either.
        if(Finite(out,16)) transforms_.Put(key,out);
    }
    template<class Native> int Bounds(const StereoBoundsKey& key,Native native) {
        if(!active_ || !Finite(key,30) || key.minimum.x>key.maximum.x ||
           key.minimum.y>key.maximum.y || key.minimum.z>key.maximum.z) return native();
        int cached;
        if(bounds_.Get(key,cached)) {
            if(!verify_) return cached;
            int actual=native();
            if(actual!=cached) { failed_=true; End(); }
            return actual;
        }
        int actual=native();
        if(actual>=0 && actual<=2) bounds_.Put(key,actual);
        return actual;
    }
};