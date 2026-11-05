// Independent cockpit prop animation. Model space: X forward, Y left, Z up.
#pragma once
#include <cmath>
#include <algorithm>

struct CockpitControlMotion {
    float up = 0, down = 0;
    double last = -1, shiftAt = -1;
    int direction = 0;
    bool initialized = false;
    bool pending = false;

    void Update(double now, float ups, float downs, bool active, bool handReady = true) {
        if (!active || !initialized || now < last) {
            up = ups;
            down = downs;
            shiftAt = -1;
            direction = 0;
            pending = false;
            initialized = active;
        } else if (ups != up || downs != down) {
            // Opposing presses in one frame cancel. Counters wrap without losing an edge.
            direction = ups != up ? (downs != down ? 0 : -1) : 1;
            up = ups;
            down = downs;
            shiftAt = -1;
            pending = direction != 0;
        }
        // Input counters remain immediate; only the visual prop waits for contact.
        if (pending && handReady) { shiftAt = now; pending = false; }
        last = now;
    }

    float Shift(double now) const {
        double t = (now - shiftAt) / (0.112 / 1.5);
        if (shiftAt < 0 || t <= 0 || t >= 1) return 0;
        // Zero velocity at neutral and at full travel, no looping or held-key drift.
        float s = (float)sin(t * 3.141592653589793);
        return direction * 0.045f * s * s;
    }
};

inline float CockpitHandbrakeAngle(float travel) {
    return std::isfinite(travel) ? std::max(0.0f, std::min(1.0f, travel)) * 0.35f : 0.0f;
}