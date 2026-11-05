# Native steering acceptance

Implementation: `vr/src/driver_steering.h` and the marked native arm path in
`vr/src/cockpit.cpp`. No overlay assets are required. Retain the local
`DRIVERMOD/driver-ik.bin`, arm markings, and existing input bindings.

Use `HideArms=0`, `ShowCustomDriverModel=0`. A 900 degree input range supplies
-450 to +450 degrees; the wheel itself is not limited by the animation.

1. Confirm the fitted neutral grips are unchanged. Check both eyes.
2. Sweep both directions slowly: 90, 100, 165, 180, 270, 310, 325, 360, 450
   degrees. The left hand tracks the wheel; the right hand releases, waits
   bottom-right, and regrips as detailed in `right-transfer-manual.md`.
3. Positive steering releases over +90 to +165 and regrips +270 to +325.
   Negative steering releases diagonally right/down over -100 to -180 and
   regrips -270 to -310. Pause and reverse at each boundary and check for
   snapping. Waiting must be independent of wheel rotation. Mesh collision
   avoidance still requires visual validation; there is no collision solver.
4. Shift and pull/release the handbrake during every stage. Hold the brake, shift
   repeatedly, then release it. The right hand must return to the current wheel
   pose, not neutral; actual gear input must never wait for animation.
5. Check wrist roll, hand clearance, elbow reach and seams. Sweep slowly through
   60–90 degrees in both directions, then reverse: elbows must not snap at reach
   boundaries. A soft reach limit retains an elbow bend and fixed bone lengths,
   including shifter/handbrake reaches. Unreachable targets intentionally lose
   contact rather than stretching the arm. There is no collision solver.
6. Change cars, reset the graphics device, and repeat in both eyes.

Run `vr/tests/marked_arm_regression.cmd` before building with `vr/build.cmd`.
Visual acceptance must be recorded in-game; automated tests cannot establish
that the native mesh's rigid nearest-segment bindings have invisible seams.