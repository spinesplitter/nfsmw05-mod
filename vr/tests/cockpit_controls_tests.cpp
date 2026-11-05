#include "../src/cockpit_controls.h"
#include <cassert>
#include <limits>
#include <cstdio>

int main() {
    CockpitControlMotion m;
    m.Update(1, 10, 20, true);
    assert(m.Shift(1.1) == 0); // initial counters cannot generate a phantom shift
    m.Update(2, 11, 20, true);
    assert(fabsf(m.Shift(2.14) + 0.045f) < 0.00001f);
    assert(m.Shift(2) == 0 && m.Shift(2.3) == 0);
    m.Update(3, 11, 21, true);
    assert(fabsf(m.Shift(3.14) - 0.045f) < 0.00001f);
    m.Update(3.1, 11, 21, true);
    assert(m.Shift(3.4) == 0); // held input does not repeat
    m.Update(4, 12, 22, true);
    assert(m.Shift(4.14) == 0); // simultaneous opposing events cancel
    m.Update(5, 65535, 22, false);
    m.Update(6, 65535, 22, true);
    m.Update(7, 0, 22, true);
    assert(m.Shift(7.14) < 0); // counter wrap
    m.Update(8, 0, 22, false);
    assert(m.Shift(8.14) == 0);
    assert(CockpitHandbrakeAngle(-1) == 0);
    assert(CockpitHandbrakeAngle(2) == 0.35f);
    assert(CockpitHandbrakeAngle(std::numeric_limits<float>::quiet_NaN()) == 0);
    puts("cockpit control motion tests passed");
}