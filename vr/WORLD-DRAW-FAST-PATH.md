# World-draw device-query fast path

The skin-state predicate in `DeviceWrap::DrawIndexedPrimitive` (the device
wrapper in `src/d3d9proxy.cpp`) formerly called `DrawingPlayerView()` before
testing `Cockpit_IsSkinDraw()`. The former queries render target, depth stencil,
back buffer and two surface descriptions, with COM reference cleanup. This
work was repeated on ordinary triangle-list world draws despite their index
buffers not belonging to the registered driver.

Test skin membership first. Unregistered index buffers return false at the
existing map lookup; registered skin still requires the original player-view
check before any render-state override. No draw, view, geometry, reflection,
depth split, hand pose, or trace event is removed. This is predicate
short-circuiting, not device-state caching, so resets/state blocks require no
new cache invalidation logic.

The patch does not assert that the 0x6DED37 queue contains only these draws.
It removes avoidable work across all affected indexed world draws, including
city queues. The actual milliseconds saved and elimination of missed VR
deadlines must be measured in gameplay; synthetic/source tests cannot prove
the FPS outcome.

Validation/install entry point:
`T:\Games\ModWanted\vr-optimization-install\vr\tests\install_world_draw_fast_path.py`

Runs the new predicate tests, existing finger/skin preservation tests, full
available optimization regressions and release build. Installation reuses the
existing guarded installer: rejects a running game/host, backs up ASI/PDB,
verifies copied hashes, verifies all scripts INI files unchanged, and restores
the old files on copy/verification failure. See `world-draw-validation.json`
and `world-draw-installation.json` in this directory for actual results.

After a successful installation, rollback with the game and host closed by
copying `nfsmw_vr.asi` and `nfsmw_vr.pdb` from the exact backup directory in
the installation report into the game's `scripts` directory.