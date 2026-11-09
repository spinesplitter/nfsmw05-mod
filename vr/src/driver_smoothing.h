#pragma once
#include <math.h>

// Driver animation smoothing.
//
// The visible steering wheel always uses the game's raw angle. Only the angle
// handed to DriverSteering::Evaluate is filtered: the wrist is still solved on
// the wheel rim, so hand contact is kept, while the arm and hand targets lag
// with inertia. Release and regrip phases are driven by the same filtered
// angle, so those transitions are smoothed too.
namespace DriverSmoothing {
constexpr float kMaxTimeConstant = 0.30f; // seconds at 100% smoothing
constexpr float kMaxStep = 0.1f;          // cockpit.cpp clamps its frame time to this
constexpr float kSnapDegrees = 180.0f;    // larger jump = car/angle reset, not steering

inline float Amount(float smoothing) {
    if (!(smoothing > 0.0f)) return 0.0f; // also rejects NaN
    return smoothing < 1.0f ? smoothing : 1.0f;
}

struct Filter {
    float value = 0.0f;
    bool initialized = false;

    void Reset() { initialized = false; }

    // Exponential smoothing: alpha = 1 - exp(-dt / tau), so the result for a
    // given elapsed time does not depend on the frame rate. dt = 0 holds the
    // previous angle; 0% smoothing passes the target through exactly.
    float Update(float target, float smoothing, float dt) {
        if (!std::isfinite(target)) return value;
        if (!initialized) {
            value = target;
            initialized = true;
            return value;
        }
        float amount = Amount(smoothing);
        if (amount <= 0.0f) {
            value = target;
            return value;
        }
        float delta = target - value;
        if (fabsf(delta) > kSnapDegrees) {
            value = target; // respawn / car change: never sweep the arms across
            return value;
        }
        float step = dt > 0.0f ? (dt < kMaxStep ? dt : kMaxStep) : 0.0f;
        float alpha = 1.0f - expf(-step / (amount * kMaxTimeConstant));
        value += delta * alpha;
        return value;
    }
};
} // namespace DriverSmoothing
