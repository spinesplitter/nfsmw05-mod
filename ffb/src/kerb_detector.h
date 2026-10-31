#pragma once
#include <math.h>
#include <stdint.h>

// Runs on every physics sample, not on the asynchronous force-output thread.
// Independent wheel edges: holding one wheel up must not mask the other wheel.
struct KerbDetector {
    bool armed[2] = {true, true};
    double last[2] = {-1, -1};
    uint32_t seq = 0;
    float hit = 0, dir = 0;
    double time = -1;

    void Sample(const float bump[2], bool continuous, double now) {
        if (!continuous) {
            armed[0] = armed[1] = true;
            last[0] = last[1] = -1;
            return;
        }
        float peak = 0, side = 0;
        for (int i = 0; i < 2; ++i) {
            const float v = bump[i];
            if (!isfinite(v)) continue;
            if (v < 0.025f) armed[i] = true;
            if (v > 0.06f && armed[i] && (last[i] < 0 || now - last[i] >= 0.06)) {
                armed[i] = false;
                last[i] = now;
                side += i ? v : -v;
                if (v > peak) peak = v;
            }
        }
        if (peak > 0) {
            ++seq;
            hit = peak;
            dir = fabsf(side) < 0.02f ? 0 : (side > 0 ? 1.0f : -1.0f);
            time = now;
        }
    }
};