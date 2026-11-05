# Native marked-arm handbrake / shifter test

The imported DRIVERMOD/driver-ik.bin remains in driver model space. No torso
marking is required: the right shoulder retracts up to 12 cm with a stationary chest. This is a simple nearest-segment
binding for testing, not artist-authored skin weights or finger animation.

1. In the Gallardo cockpit, mark the full right arm and hand with **Mark as arms**.
   Both arms may be marked: the left chain is deliberately kept at rest.
2. Mark the brake lever with **Mark as handbrake**, and the gear lever with **Mark as shifter**. Keep the native arms visible (Hide arms
   off, or restore their visibility with the existing editor). Do not enable the
   separate custom-driver overlay for this test.
3. Apply the existing bound handbrake control. The arm travels at 2.0625 m/s
   backward and right toward a pose above the lever, irrespective of input percentage.
   Release returns it at the same speed. The mod never rotates the brake lever itself.
   Wrist and hand translate rigidly with the forearm; there is no wrist IK or gripping.
   Try sequential shifts and H gates, including failed clutch/misshift attempts and
   reselecting the same gate. Every raw press triggers a shifter reach, held gates
   do not repeat. H gates retain forward/backward lever travel (now a 0.074667 s cycle).
   Arm and lever animation speeds are 50% faster than the previous build;
   shift envelope duration is 0.146667 s and arrival-based hold is 0.24 s.
   Shifts temporarily take priority over a held handbrake: transfer immediately
   to the shifter, play the shift after arrival, then return to the handbrake
   without requiring brake release. If brake was released, return to rest instead.
   The visual shifter pivot waits until the wrist reaches its approved target
   (within 5 mm); actual gear input is not delayed. Verify up/down direction,
   repeated presses, and failed shifts. Rapid pending presses use the latest direction.
   Apply handbrake during a shift reach/hold and verify the shift finishes before
   returning continuously to the brake with no intermediate steering-wheel pose.
   Hold brake throughout repeated up/down shifts and verify each interrupts the
   brake pose, the lever waits for contact, and the arm returns after completion.
   The shifting pose is unchanged. Handbrake wrist target retains 8 cm backward,
   8 cm right and 10 cm down offsets. The actual elbow joint (shoulder tail /
   elbow head, not wrist head) separately moves 8 cm right and 10 cm down.
   Verify the hand is no longer displaced the extra 8 cm right, and that the
   visible elbow now moves. Both joint adjustments must blend without snapping.
   Check clearance above the native lever and geometry breaks in-game.
   Show IK is optional and displays the same pose as the mesh.
4. Check short taps, held input, analogue input, repeated pulls, both eyes, car
   switches, and another driver's car. Check sleeves at the shoulder/elbow for
   seams and the hand against the grip. Only marked vertices should move.

Native textures/shaders are retained by drawing a private copy of stream 0.
Original CPU geometry and GPU buffers are never edited. Unsupported layouts or
failed buffer locks fall back to the original static draw. This deliberately
prioritizes elbow placement over anatomical precision; shoulder/elbow seams and
control alignment need in-game review.

Automated focused suite: tests\marked_arm_regression.cmd. The legacy full suite
also requires the old, separate driver-runtime.bin overlay asset, which is not
needed by this native marked-arm test.