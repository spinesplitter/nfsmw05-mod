#pragma once
#include <algorithm>
#include <vector>

// Draw-local bookkeeping only: no mesh/editor/animation state survives a call.
// The source mesh uses 16-bit indices, irrespective of the output base vertex.
class DriverPoseSelection {
    enum { Selected=1, Manual=2, Automatic=4 };
public:
    struct BoneOverride { int bone=-1, side=-1; };
private:
    std::vector<unsigned char> flags_;
    std::vector<unsigned> vertices_;
    std::vector<BoneOverride> boneOverrides_;
    std::vector<unsigned char> boneOverridePriority_;
public:
    explicit DriverPoseSelection(unsigned count) : flags_(std::min(count,65536u),0) {}
    void Add(unsigned vertex, bool manual) {
        if (vertex>=flags_.size()) return;
        auto& flags=flags_[vertex];
        if (!(flags&Selected)) vertices_.push_back(vertex);
        flags|=Selected | (manual ? Manual : Automatic);
    }
    void ExcludeAutomatic(unsigned vertex) {
        if (vertex<flags_.size() && !(flags_[vertex]&Manual)) flags_[vertex]&=~Selected;
    }
    void SetBoneOverride(unsigned vertex, int bone, int side, unsigned char priority = 0) {
        if(vertex>=flags_.size() || bone<0 || (side!=0 && side!=1)) return;
        if(boneOverrides_.empty()) {
            boneOverrides_.resize(flags_.size());
            boneOverridePriority_.resize(flags_.size());
        }
        if(boneOverrides_[vertex].bone>=0 && boneOverridePriority_[vertex]>priority) return;
        boneOverrides_[vertex]={bone,side};
        boneOverridePriority_[vertex]=priority;
    }
    bool GetBoneOverride(unsigned vertex, BoneOverride& result) const {
        if(vertex>=boneOverrides_.size() || boneOverrides_[vertex].bone<0) return false;
        result=boneOverrides_[vertex];
        return true;
    }
    bool IsSelected(unsigned vertex) const { return (flags_[vertex]&Selected)!=0; }
    bool IsAutomatic(unsigned vertex) const { return !(flags_[vertex]&Manual); }
    const std::vector<unsigned>& Ordered() {
        std::sort(vertices_.begin(),vertices_.end());
        return vertices_;
    }
};