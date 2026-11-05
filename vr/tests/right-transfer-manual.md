# Right-arm steering transfer

## Per-car neutral hand fix

F4 > Animated Driver > Hand fix (down / up) previews the neutral grip position
along the wheel rim, from -60 to +60 degrees in 1-degree steps. Negative moves
both hands down; positive moves both up. Zero preserves the original fitted
grips. Radius and wheel-plane depth are unchanged; grips stop short of crossing
the top/bottom into the opposite side. IK may limit reach without stretching.
Use **Save for this car** to persist both Hand spread and Hand fix in
`nfsmw_vr_driver.ini`, section `[AnimatedDriver]`, as `car=spread,handFix`.
Old spread-only entries load with zero hand fix. Unsaved previews are discarded
when changing cars, and a car without a saved fit starts with zero hand fix.

The setting changes neutral wheel grips, not steering input or phase timing.
Both bottom-right waits and regrip contacts remain unchanged. Verify neutral,
slow full sweeps, reversals, shifts and held brake in both eyes after fitting;
extreme adjustments may reduce clearance or exceed the arm's reach.

## Positive steering: detached bottom-right wait (current)

- 0 to +90: original wheel tracking.
- +90 to +165: lift off and move right to a bottom-right waiting position.
- +165 to +270: hold that position independently of wheel rotation.
- +270 to +325: approach the bottom-right rim, completing regrip at +325.
- +325 to +450: follow the wheel from the new contact position.

The waiting target is outside the bottom-right rim and towards the driver.
No forced positive elbow-pole rotation is used. Quiet-elbow blending over
90 to 165 favors the fitted elbow while IK preserves both bone lengths.
Reversals retrace the angle-driven path; control overrides retain precedence.
Verify both eyes visually: centreline clearance does not prove mesh clearance.

## Negative steering: diagonal bottom-right wait

- 0 to -100: original wheel tracking.
- -100 to -180: detach and ease diagonally right/down to the bottom-right wait.
- -180 to -270: hold that position independently of wheel rotation.
- -270 to -310: approach the bottom-right rim, completing regrip at -310.
- -310 to -450: follow the wheel from the new contact position.

The waiting target is outside the bottom-right rim and towards the driver;
the contact target is on the bottom-right rim. After -310
the regripped hand follows the wheel, rather than staying fixed in that corner.
Driver-right is anchored to the fitted neutral right-hand grip, not the sign
of the measured wheel axis, so reversing that axis cannot mirror the regrip.
The old large negative detour and forced elbow twist are removed. Quiet-elbow blending over
-100 to -180 favors the fitted elbow; IK preserves bone lengths and may clamp
unreachable wrist targets. The positive path and left arm are unchanged.

All phases use angle-driven quintic easing. Pause and reverse in each phase;
there is no timer or reset at 360 degrees. Check both eyes at slow and fast
steering rates, and shifts/held handbrake during release, waiting and regrip.
Control overrides retain precedence. Confirm neutral poses and unchanged left
hand tracking. Centreline clearance is not a mesh collision guarantee.

Automated checks cover phase continuity, stationary waiting targets, release
weights, bottom-right waits and regrips, tracking after contact, full installed-rig
0.02-degree sweep, bone lengths, fitted elbow behavior and sampled arm clearance
from +90 through +450 and -100 through -450.