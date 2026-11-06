#pragma once
#include "driver_ik.h"
#include "custom_driver_animation.h"
#include <cstring>
#include <vector>

// Only rest-space binding survives draws. Never retain posed vertices, normals,
// world matrices, steering/control state, editor selections or mesh pointers.
class DriverBindingCache {
public:
    struct Binding { float weight = 0; int bone = -1, side = -1; };
private:
    struct Entry {
        CustomDriver::Point point{};
        Binding binding;
        bool valid = false;
    };
    std::vector<float> rest_, nextRest_;
    std::vector<int> layout_, nextLayout_;
    std::vector<Entry> entries_;
public:
    void Prepare(const DriverIK::Rig& rig) {
        nextRest_.clear(); nextLayout_.clear();
        for (const auto& bone : rig.bones) {
            nextRest_.insert(nextRest_.end(),bone.head,bone.head+3);
            nextRest_.insert(nextRest_.end(),bone.tail,bone.tail+3);
        }
        nextLayout_.insert(nextLayout_.end(),rig.arms,rig.arms+DriverIK::kArmCount);
        nextLayout_.push_back(rig.Find("JNT_hand_tip.L"));
        nextLayout_.push_back(rig.Find("JNT_hand_tip.R"));
        if (rest_.size()!=nextRest_.size() || layout_!=nextLayout_ ||
            (!rest_.empty() && std::memcmp(rest_.data(),nextRest_.data(),rest_.size()*sizeof(float)))) {
            rest_.swap(nextRest_); layout_.swap(nextLayout_);
            entries_.clear();
        }
    }
    template<class Build>
    Binding Get(unsigned vertex, CustomDriver::Point point, Build build) {
        if (vertex>=65536) return build();
        if (entries_.size()<=vertex) entries_.resize((size_t)vertex+1);
        auto& entry=entries_[vertex];
        if (!entry.valid || std::memcmp(&entry.point,&point,sizeof(point))) {
            entry.binding=build(); entry.point=point; entry.valid=true;
        }
        return entry.binding;
    }
};