// Driver inputs, not the physics-smoothed needles or traction-control output.
#pragma once
#include "input.h"
#include <math.h>

struct TelemetryInputs {
    bool steering = false;
    float degrees = 0.0f;
    bool bound[4] = {};          // gas, brake, clutch, handbrake
    float pedal[4] = {};
};

inline TelemetryInputs ReadTelemetryInputs(const InputSnapshot& in, bool classic) {
    TelemetryInputs out;
    out.steering = !classic && in.steerBound && isfinite(in.wheelDeg);
    out.degrees = out.steering ? in.wheelDeg : 0.0f;
    out.bound[0] = !classic && in.pedalBound[0];
    out.bound[1] = !classic && in.pedalBound[1];
    out.bound[2] = in.clutchBound;
    out.bound[3] = !classic && (in.buttonBound & (1u << BTN_HANDBRAKE));
    const float values[4] = {in.pedal[0], in.pedal[1], in.clutchPedal, in.handbrake};
    for (int i = 0; i < 4; i++) {
        out.bound[i] = out.bound[i] && isfinite(values[i]);
        out.pedal[i] = out.bound[i] ? Clamp(values[i], 0.0f, 1.0f) : 0.0f;
    }
    return out;
}