#pragma once
#include <algorithm>
#include <vector>

// Draw-local bookkeeping only: no mesh/editor/animation state survives a call.
// The source mesh uses 16-bit indices, irrespective of the output base vertex.
class DriverPoseSelection {
    enum { Selected=1, Manual=2, Automatic=4 };
    std::vector<unsigned char> flags_;
    std::vector<unsigned> vertices_;
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
    bool IsSelected(unsigned vertex) const { return (flags_[vertex]&Selected)!=0; }
    bool IsAutomatic(unsigned vertex) const { return !(flags_[vertex]&Manual); }
    const std::vector<unsigned>& Ordered() {
        std::sort(vertices_.begin(),vertices_.end());
        return vertices_;
    }
};