// Slip-angle based additions are unreliable near rest. Keep the game's low-speed contact
// solver, including its longitudinal slip, and introduce the additions using ground speed.
#pragma once
#include <math.h>

namespace low_speed_tyres {
inline float Share(float groundSpeed) {
    float x = (fabsf(groundSpeed) - 2.0f) / 3.0f;
    if (!(x > 0.0f)) return 0.0f;
    if (x >= 1.0f) return 1.0f;
    return x * x * (3.0f - 2.0f * x);
}

inline float Blend(float stock, float custom, float groundSpeed) {
    float share = Share(groundSpeed);
    if (share == 0.0f) return stock;
    if (share == 1.0f) return custom;
    return stock + (custom - stock) * share;
}

inline float WheelTie(float traction, float strength, float groundSpeed) {
    if (!(traction > 0.0f && traction < 1.0f) || !(strength > 0.0f)) return traction;
    float share = Share(groundSpeed);
    if (share == 0.0f) return traction;
    return powf(traction, 1.0f + 15.0f * strength * share);
}
}  // namespace low_speed_tyres