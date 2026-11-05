# Experimental MW animated driver and cockpit controls

This checkpoint contains the Most Wanted runtime and F4 menu changes, not the
separate STEERINGMOD preview project. No proprietary model, texture, animation,
generated asset payload, or local car-selection configuration is distributed.

## Features

- Triangle editor: Mark as handbrake, Mark as shifter, and Clear marks, with
  persistence and undo. See `vr/tests/cockpit-controls-manual.md`.
- Independent handbrake and sequential shifter prop motion and input bridge.
- Separate depth-tested rigged driver overlay, with device-reset cleanup.
- F4 > Animated Driver: global toggle, live 50–150% wheel-plane hand spread in
  1% steps, and explicit Save for this car.
- Per-model spread in `scripts/nfsmw_vr_driver.ini`, `[AnimatedDriver]`.
  Unsaved spread is discarded on car changes; missing models default to 100%.
- E30 sampled release/regrip paths fitted to detected wheel centre, axis and
  radius. Axial depth and control targets are not scaled by hand spread.

## Assets and validation

The installed game needs `drivermod/driver-runtime.bin` (CDR1) and
`drivermod/driver-animation.bin` (DRA2). Preserve your existing fitted assets;
this source-only commit intentionally does not supply third-party assets.
`vr/tools/prepare_rigged_overlay.py` is an optional exporter requiring Pillow,
your separately obtained preview project, fitted rig/model, texture, and AC
content. Pass its location with `--preview`; its dependencies are not vendored.
Do not run the obsolete static-driver exporter over the rigged payload.

Run `validate-rigged-overlay.cmd` from this repository on Windows with the
configured VS2017 Build Tools. It tests the rig, assets, controls, per-car
state and cockpit before building the VR runtime. Tests expect the installed
assets under the parent game directory. Gameplay regression/build commands:
`bash gameplay/tests/run.sh` and `bash gameplay/build.sh` in MSYS2 with the
32-bit MinGW compiler available. Validation does not install binaries.

## Experimental limitations

The source trajectory mapping is experimental (+450 to -450 degrees). It is
not full source wrist/finger animation; contact and grip timing remain visually
unverified. IK clamps unreachable targets. Wheel fitting does not establish
correct finger contact on all 36 cars. Check both locks, reversals, release and
regrip, shift/handbrake, car switches, persistence, both eyes and device resets.
The previous installation used an explicitly authorized gameplay INI-fingerprint
exception; consult fresh test results rather than assuming that failure remains
unchanged. No general regression bypass is provided by this checkpoint.