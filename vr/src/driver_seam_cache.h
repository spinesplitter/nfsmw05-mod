#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <set>
#include <vector>

// Own all inputs: streamed meshes may reuse addresses, and editor changes may
// occur between draws. Exact comparisons avoid hash collisions/stale seams.
class DriverSeamCache {
    std::vector<uint16_t> indices_;
    std::vector<int> layout_;
    std::set<uint64_t> native_;
    std::vector<unsigned char> excluded_;
    bool valid_=false;
public:
    template<class Build>
    const std::vector<unsigned char>& Get(const uint16_t* indices, size_t count,
        unsigned vertices, const std::vector<int>& layout,
        const std::set<uint64_t>& native, Build build) {
        vertices=std::min(vertices,65536u);
        if(valid_ && indices_.size()==count && excluded_.size()==vertices &&
            layout_==layout && native_==native &&
            (!count || std::memcmp(indices_.data(),indices,count*sizeof(uint16_t))==0))
            return excluded_;
        // Leave invalid if allocation or construction fails.
        valid_=false;
        indices_.clear();
        if(count) indices_.assign(indices,indices+count);
        layout_=layout; native_=native;
        excluded_.assign(vertices,0);
        build(excluded_);
        valid_=true;
        return excluded_;
    }
};