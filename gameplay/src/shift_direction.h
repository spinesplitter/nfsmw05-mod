#pragma once
#include <math.h>

inline bool WrongDirectionShiftAllowed(int gear, float speed, float wheelAngularVelocity) {
    constexpr float tolerance = 0.001f; // absorb float conversion around the exact km/h thresholds
    return (gear == 2 && wheelAngularVelocity < -3.0f && speed <= 25.0f / 3.6f + tolerance) ||
           (gear == 0 && wheelAngularVelocity > 3.0f && speed <= 20.0f / 3.6f + tolerance);
}