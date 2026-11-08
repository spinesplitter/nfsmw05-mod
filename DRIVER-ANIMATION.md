# Driver animation: native model implementation

This branch supersedes the separate overlay experiment. The intended runtime
uses the game's native driver mesh, textures and shaders, with explicitly marked
arm triangles and a model-space rig. Keep `ShowCustomDriverModel=0` and
`HideArms=0` in `[VR]`. Existing settings are not silently overwritten.

## Included implementation

- Native marked-arm deformation and optional IK visualization, with safe static
  fallback for unsupported vertex layouts or unavailable buffers.
- Marked sleeve cuff vertices remain bound to the upper-arm/forearm instead of
  inheriting the palm's wheel rotation.
- Corrected handbrake wrist and elbow poses and constant-speed transitions.
- Raw sequential/H-gate shift triggers, including failed shifts, without delaying
  actual gear input. Lever motion waits until wrist arrival within 5 mm.
- Shifts interrupt held handbrake poses and return directly to the brake if held.
- Arm speed 2.0625 m/s, lever cycle about 0.074667 s, shift envelope about
  0.146667 s, and arrival hold 0.24 s.
- Installed Gallardo arm/shifter/handbrake triangle markings, merged without
  replacing unrelated per-car edits.

## Assets and build

The game installation needs `DRIVERMOD/driver-ik.bin`, exported from your
separately supplied `DRIVERMOD/rigonly.blend` using
`vr/tools/export_driver_ik.py`. No proprietary mesh, Blender file or generated
third-party payload is distributed. Yesterday's `driver-runtime.bin` and
`driver-animation.bin` are not needed for the native path.

Build with `vr/build.cmd` and `gameplay/build.sh` using the repository's configured
VS2017 and 32-bit MSYS2/MinGW tools respectively. Deploy the VR runtime and the
matching gameplay input bridge together; retain your input bindings.

Run `vr/tests/marked_arm_regression.cmd` for self-contained native-path tests.
Optionally pass the absolute path to `driver-ik.bin` to validate your real rig.
The legacy full suite still includes retired overlay tests requiring old assets.

See `vr/tests/marked-arms-manual.md` for in-game acceptance. Automated tests do not
establish visual correctness: both eyes, car changes, repeated shift/brake
interruptions, device reset, joint seams and control clearance need review.