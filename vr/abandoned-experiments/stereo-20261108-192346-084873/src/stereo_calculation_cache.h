#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Bounded, value-keyed memoization of pure native calculations. No game pointers
// survive a call, no result survives End(), and collisions are cache misses.
// Keep the caller's native calculation on misses: do not substitute VR math.
template<class Key, class Value, size_t Capacity> class StereoCalculationCache {
    struct Entry { uint32_t generation=0; Key key{}; Value value{}; };
    Entry entries_[Capacity]{};
    uint32_t generation_=0;
    bool active_=false;
    static size_t Index(const Key& key) {
        uint32_t hash=2166136261u;
        const auto* bytes=reinterpret_cast<const unsigned char*>(&key);
        for(size_t i=0;i<sizeof(key);++i) hash=(hash^bytes[i])*16777619u;
        return hash%Capacity;
    }
public:
    uint64_t hits=0,misses=0;
    void Begin() {
        if(++generation_==0) {
            for(auto& entry:entries_) entry.generation=0;
            generation_=1;
        }
        hits=misses=0;
        active_=true;
    }
    void End() { active_=false; }
    bool Get(const Key& key,Value& value) {
        if(!active_) return false;
        const Entry& entry=entries_[Index(key)];
        if(entry.generation!=generation_ || memcmp(&key,&entry.key,sizeof(key))) {
            ++misses;
            return false;
        }
        value=entry.value;
        ++hits;
        return true;
    }
    void Put(const Key& key,const Value& value) {
        if(!active_) return;
        Entry& entry=entries_[Index(key)];
        entry.key=key;
        entry.value=value;
        entry.generation=generation_;
    }
};