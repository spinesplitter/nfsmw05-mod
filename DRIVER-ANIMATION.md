# Reversible automatic finger merge

Lift/regrip elbow correction: retain the transported fitted arm bend plane.
The previous lift-only blend toward a fixed model-space elbow could splay the
control elbow as the wrist moved. Hand paths and bone lengths are unchanged;
all fitted rigs now test lifted poses against the transported-pole solution.

Detected native finger pieces automatically use the driver's skin material and
arm animation, including when Hide driver hands is enabled. Restore clears the
selected triangles' Arms/Skin/Show overrides and saves a `Native` opt-out so the
automatic merge cannot immediately reapply them. Merge with fingers explicitly
removes this opt-out. Undo restores the previous complete edit state.

For batch marking without triangle picking, run the local
`vr/tools/merge_driver_fingers.py` with `--game`, `--reader-dir` (the existing
geometry-reader directory), and `--report`. The default is a read-only audit;
`--apply` requires the game closed and creates a byte-verified edits-INI backup.
For fitted full-detail drivers it checks two DRIVER_PLAYER materials, the second
168-triangle submesh, index bounds and the reference hand vertex island with at
least 90% matching faces (some cars triangulate ten faces differently). It writes
Arms/Skin/Show exactly like Merge fingers, clears conflicting Hide/Black only on
those triangles, and preserves Native opt-outs and unrelated edits. This is not
an undo-stack command: reverse a batch using its INI backup with the game closed.
Unverified layouts and missing models are reported and skipped, not guessed.

Native hand targets now include the wheel submesh's configured position offset,
transformed through its submitted matrix into vehicle and driver space. Both
hands follow this relocation without rotating the translation during steering.

# Driver animation: native model implementation

## Per-car native rig transfer

`vr/tools/fit_native_drivers.py` reads the installed full-detail driver geometry,
checks vertex/triangle correspondence against Gallardo, and transfers the supplied
rig using local corresponding-vertex displacements. It writes private generated
assets to `DRIVERMOD/rigs/<CAR>.bin` with a JSON coverage report. Existing generated
rigs are backed up before replacement; the original Gallardo rig is unchanged.
Run with `--game <absolute installation path> --reader-dir <absolute directory
containing inspect_cockpit_gaps.py>`. The reader is an existing local audit tool,
not a new package dependency or a bundled proprietary asset.

With `--reset-hidden`, the edits INI is backed up and byte-verified before clearing
only `Hide=` values. Every other line is preserved, including manual arm, skin,
show, black, wheel, shifter and handbrake marks. Global HideHead/HideArms settings
are deliberately not changed. Backup filenames include `before-all-car-ik`.

The native full-detail driver uses its own fitted rig and automatic classified arm
ranges, without converting sleeve materials to skin. The Clio and Monaro handedness
rules mirror the steering transfer and select the left hand for both left-side
controls (Monaro still needs a compatible rig fit). Unsupported cars do
not silently fall back to the Gallardo rig. Other LODs are not automatically bound
to full-detail rig coordinates. Geometry fitting is approximate, not a substitute
for an in-game visual review of wheel contact, cuffs, controls and both locks.

Current installed-geometry coverage: 38 compatible full-detail car drivers. Monaro
has a different topology and remains unsupported, as do lower-detail police and
traffic driver meshes. Cars without a full-detail driver are not synthesized.

Automatic visibility ranges are not treated as unconditional skinning weights:
fitted-rig arm capsules restrict influence, shoulder-root weights fade in, and
automatic vertices shared with non-arm ranges remain fixed. Position and normal
weights match. Explicit manual arm marks still override automatic membership.
Per-rig tests deliberately include torso/jeans in an automatic range and assert
their vertex bytes remain unchanged while wrist vertices steer.

Control reaches now measure each full-detail interior's console geometry when
no manual Shifter/Handbrake marks exist. The detector welds disconnected seams,
selects compact knob geometry and longitudinal brake handles, and targets the
knob top / forward grip with 5 cm hover clearance (lowered by 3 cm). Manual marks
override detection independently for each control. Unknown or merged geometry
can require manual marks; detection is heuristic, not universal recognition.
The existing reach speed, arrival hold, shift/brake interruption and elbow
clearance remain. Fixed wrist-position clamps and brake hand displacement have
been removed so they cannot pull the hand away from the measured control.
Targets are retained in vehicle space to avoid vehicle-motion lag. Only marked
shifters receive the mod's visible lever travel; detected static knobs do not
receive fictitious target travel. This does not add rigs for unsupported drivers.

Validation: `all-car-validate.cmd` runs per-rig steering sweeps, control-hand and
membership checks, the existing marked-arm regressions, then builds the runtime.
`python vr/tests/fit_native_drivers_tests.py` checks displacement transfer and
hidden-only INI persistence/backup behavior using temporary synthetic data.

This branch supersedes the separate overlay experiment. The intended runtime
uses the game's native driver mesh, textures and shaders, with explicitly marked
arm triangles and a model-space rig. Keep `ShowCustomDriverModel=0` and
`HideArms=0` in `[VR]`. Existing settings are not silently overwritten.

## Included implementation

- Native marked-arm deformation and optional IK visualization, with safe static
  fallback for unsupported vertex layouts or unavailable buffers.
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