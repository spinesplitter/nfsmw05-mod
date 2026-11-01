# Traffic and wheel-location recovery

## Traffic

The locally supplied reference traffic mod was inspected only to identify the
settings it affects: regional traffic-pattern density and default spawn timing.
Its script was not copied or applied. No VLT database files were changed.

Independent disassembly of this installation's executable shows that the traffic
manager at `0x439BB0` reads density at `0x439BBD`, then calls a bounded lookup
curve at `0x439BF0` (`0x6533B0`). The curve returns its last entry when the input
exceeds its range. Multiplying density before this curve therefore does not
provide a proportional increase in spawn rate.

The new hook scales the curve's output, only at that traffic-manager call site,
only in Career race/free-roam contexts. `CareerMultiplier=3` requests three times
the baseline curve output. It does not promise three times the visible cars:
existing pool limits, spawn conditions, pursuits and regional patterns remain.
The free-roam density floor behavior is retained. `1` bypasses the multiplier.
Both call-site signatures are checked before either hook is installed.
Context transitions log density and original/scaled spawn rates for live checks.
Boss/intro roster exclusions and opponent-speed controls are unchanged.

## Wheel location

The current shallow Git history lacks the wheel-location feature. A surviving
local source tree under `mod-builds/b2762ee-with-wheel` contains it, but its Git
worktree metadata refers to an old directory and cannot resolve as a repository.
The feature has been restored narrowly, without replacing newer source files.

F4 provides steering-wheel location left/right and up/down independently of its
animation pivot. Translation is applied after rotation, including at zero steering
angle. Per-model values load/save in `[WheelPosition]` in `nfsmw_vr_wheel.ini`,
with migration from legacy sections in `nfsmw_vr.ini`. Existing installed values
must not be overwritten by package defaults. The installed INI still had saved
positions for IS300, GTI, ECLIPSEGT, CLIO, RX8, CTS, GTO and PUNTO at inspection.

Regression coverage includes translation at straight/left/right steering and
real temporary-INI save/reload with per-car isolation. In-game route comparisons,
boss/intro races and headset rendering still require live validation.