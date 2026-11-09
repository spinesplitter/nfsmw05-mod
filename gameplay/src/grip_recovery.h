#pragma once
#include <math.h>

// Off-throttle slide recovery.
//
// The tyre curve and the wheelspin model already give grip back when the engine
// stops driving a tyre. This only sets how quickly that happens: while the
// throttle is closed (coasting or engine braking) a sliding tyre's configured
// TyreSlideGrip deficit returns toward the game's own 1.0.
//
// It never adds grip above the stock curve. It is a recovery rate, not a grip
// multiplier: 0% is exactly the old behaviour.
namespace grip_recovery {
constexpr float kRateAtFull = 10.0f; // 1/s at 100%: 95% of the deficit in about 0.3 s
constexpr float kMaxStep = 0.1f;     // the cockpit's physics step is far below this

inline float Amount(float setting) {
    if (!(setting > 0.0f)) return 0.0f; // also rejects NaN
    return setting < 1.0f ? setting : 1.0f;
}

// Exponential approach of progress (0..1) toward 1. Frame-rate independent.
inline float Step(float progress, float setting, float dt) {
    float amount = Amount(setting);
    if (amount <= 0.0f) return 0.0f;
    float step = dt > 0.0f ? (dt < kMaxStep ? dt : kMaxStep) : 0.0f;
    float alpha = 1.0f - expf(-step * kRateAtFull * amount);
    return progress + (1.0f - progress) * alpha;
}

// The configured sliding grip, recovered toward the game's own 1.0.
inline float SlideGrip(float configured, float progress) {
    float c = configured > 0.0f ? (configured < 1.0f ? configured : 1.0f) : 0.0f;
    float p = progress > 0.0f ? (progress < 1.0f ? progress : 1.0f) : 0.0f;
    return c + (1.0f - c) * p;
}
} // namespace grip_recovery
