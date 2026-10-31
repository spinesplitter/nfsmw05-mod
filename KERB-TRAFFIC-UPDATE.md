# Kerb detection and F4 traffic multiplier

## Kerbs

Hits are now latched on the physics thread, preserving a short impact if the
force-output thread skips its original telemetry sample. Each front wheel has
independent hysteresis (trigger above 0.06 m/s, rearm below 0.025 m/s) and a 60 ms
debounce. Sustained compression cannot repeatedly trigger; extension only rearms.
The high-pass transient is measured before the slow filter is advanced, avoiding
extra attenuation on longer ticks. Discontinuities seed the detector without hits.
Reports older than 150 ms are not played. Close hits raise the current jolt
without repeatedly restarting its push at zero. Surface rumble remains disabled.

This still detects suspension impacts, not an exact kerb material classification:
sharp bumps can trigger too. Wheel feel and compression-sign behavior across cars
need in-game testing; software tests cannot establish physical wheel feel.

## Traffic

F4 -> Traffic and HUD -> Traffic -> Traffic multiplier.
Persisted as [Traffic] Multiplier in nfsmw_gameplay.ini; default 1.0, range 0..10.
0 disables new density requests; existing vehicles need not disappear immediately.
The signature-checked detour now scales the live normalized density getter at
0x41EC20, leaving stored parameters untouched. Repeated reads do not compound.
This replaces the earlier race-start getter at 0x5FABB0. The live trampoline
steals seven whole relocatable bytes and uses a float-returning cdecl ABI.

Results remain bounded to the normalized 0..1 scale (0..100%). Larger factors
help lower starting densities but do not expand car pools or override zero
requests. Changes apply on the next live density read. Race/free-roam spawning
and actual active-car count changes still require an in-game check.

## Driving checks

- Left, right, then alternating kerbs while the opposite wheel stays compressed.
- Small/large bumps; rebound; successive strip teeth; low FPS; hills and body roll.
- Pause/resume, teleport, car switch and Kerbs=0 (no stale kick).
- Road rumble remains absent; offroad, tyre scrub and collision effects still work.
- Traffic 0x/1x/2x on the same route, after restarting the event if necessary.
- Save/reopen F4, restore 1x, check races, free roam, pursuits and transitions.

Installation must wait until speed.exe is closed. Preserve customized INIs; the
new key has a code default and is saved by the menu when adjusted.