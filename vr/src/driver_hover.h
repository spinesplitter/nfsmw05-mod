#pragma once
#include "custom_driver_animation.h"

// Model-local motion: vehicle motion and stereo rendering must not advance the reach.
struct DriverHoverMotion {
    CustomDriver::Point wrist;
    CustomDriver::Point brakeElbowOffset{};
    bool initialized = false;
    float progress = 0;
    static constexpr float speed = 2.0625f; // 50% faster than 1.375 m/s; fixed reach/release speed
    static constexpr float clearance = 0.08f;

    void Reset() { initialized = false; progress = 0; brakeElbowOffset = {}; }
    void Update(float dt, bool engaged, CustomDriver::Point rest, CustomDriver::Point hover) {
        if (!initialized) { wrist = rest; initialized = true; }
        auto goal = engaged ? hover : rest;
        auto delta = goal - wrist;
        float distance = CustomDriver::Length(delta);
        float step = speed * std::max(0.0f, std::min(0.1f, dt));
        if (distance <= step) wrist = goal;
        else if (distance > 1e-7f) wrist = wrist + delta * (step / distance);
        float reach = CustomDriver::Length(hover - rest);
        progress = reach > 1e-6f ? std::min(1.0f, CustomDriver::Length(wrist-rest)/reach) : 0;
    }
};