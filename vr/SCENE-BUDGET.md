# Reversible scene-submission budget

Proposed profile: `EnvMapFacesPerFrame=1`, `RearViewEvery=2`.
Application is blocked while the game or VR host is running; consult the
results report for actual application status, not this document.
Previous values: 2 and 1 respectively. This uses the existing production
view scheduler and does not change the binary or share player-eye visibility.
The regression suite checks all six face budgets, round-robin coverage,
half-rate rear-view updates, and that only known eye-independent views skip.

The rear-view scene is submitted on half the frames. Environment-map face
submissions drop from two to one per frame; all six faces still refresh in
rotation. This is not a 50% reduction in total rendering work. Main-eye
rendering, resolution, shadows, cockpit and hand fixes remain unchanged.
Reflection update rate is deliberately reduced: at 72 rendered FPS the rear
view updates at 36 Hz and each cube face at 12 Hz. Rates fall with actual FPS.

`tests/apply_scene_budget.py` runs regressions, refuses installation while
the game/host is running, backs up the original INI, and checks that exactly
these two settings change. See `scene-budget-results.json` for the backup.
To undo, restore the original INI while the game is closed, or restore just
the two previous values to retain any later personal edits.

No live performance improvement is established. Compare the same location,
camera, traffic and headset refresh rate without tracing first, then use
Delete for a matched capture if needed. A remaining bottleneck requires
per-pass profiling; this profile does not reduce main-eye world geometry.