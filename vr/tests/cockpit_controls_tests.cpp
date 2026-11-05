#include "../src/cockpit_controls.h"
#include <cassert>
#include <limits>
#include <cstdio>

int main() {
    CockpitControlMotion m;
    m.Update(1, 10, 20, true);
    assert(m.Shift(1.1) == 0); // initial counters cannot generate a phantom shift
    m.Update(2, 11, 20, true);
    assert(fabsf(m.Shift(2+0.056/1.5) + 0.045f) < 0.00001f);
    assert(m.Shift(2) == 0 && m.Shift(2.3) == 0);
    m.Update(3, 11, 21, true);
    assert(fabsf(m.Shift(3+0.056/1.5) - 0.045f) < 0.00001f);
    assert(m.Shift(3.075) == 0); // 50% faster means duration divided by 1.5
    m.Update(3.1, 11, 21, true);
    assert(m.Shift(3.4) == 0); // held input does not repeat
    m.Update(4, 12, 22, true);
    assert(m.Shift(4.07) == 0); // simultaneous opposing events cancel
    m.Update(5, 65535, 22, false);
    m.Update(6, 65535, 22, true);
    m.Update(7, 0, 22, true);
    assert(m.Shift(7.07) < 0); // counter wrap
    m.Update(8, 0, 22, false);
    assert(m.Shift(8.07) == 0);
    m.Update(9, 0, 22, true, false);
    m.Update(10, 1, 22, true, false);
    assert(m.pending && m.shiftAt < 0 && m.Shift(10.056) == 0);
    m.Update(11, 1, 22, true, false);
    assert(m.pending && m.Shift(11.056) == 0); // no click-time pivot during reach
    m.Update(12, 1, 22, true, true);
    assert(!m.pending && fabsf(m.Shift(12+0.056/1.5)+0.045f)<0.00001f);
    m.Update(13, 1, 23, true, false);
    m.Update(14, 1, 23, false, false);
    assert(!m.pending && m.Shift(14.056)==0); // unload cancels waiting animation
    assert(CockpitHandbrakeAngle(-1) == 0);
    assert(CockpitHandbrakeAngle(2) == 0.35f);
    assert(CockpitHandbrakeAngle(std::numeric_limits<float>::quiet_NaN()) == 0);
    puts("cockpit control motion tests passed");
}