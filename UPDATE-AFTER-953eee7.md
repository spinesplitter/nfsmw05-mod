# Update after 953eee7

This update retains the native animated-driver implementation and adds:

- Driver binding, pose, seam and stream caching, with buffer-write invalidation
  and driver/mirror stream-range validation.
- Batched mirror vertex updates and bounded rejected-wheel caching.
- Object submission counters, nested CPU timings, render-boundary traces and
  a validating pass-trace analysis tool.
- Consistent split-depth projection for main and camera-sharing companion views,
  including guarded cockpit-depth variants and matrix/alias validation.
- A 0.5 m world near-plane default. The distributed cockpit NearZ stays 0.091 m;
  eye resolution is unchanged. Existing saved INIs are not automatically retuned.
- The previously developed stock-traffic restoration changes. These remove the
  custom traffic/spawn and forced Career roster overrides, not world scenery.

See DRIVER-ANIMATION.md, vr/OPTIMIZATION.md, vr/OBJECT-TRACE.md and
gameplay/STOCK-TRAFFIC.md for details. Local traces, generated assets, installation
reports, backups and compiled binaries are not part of this source update.

## Validation

Run vr/tests/optimization_run.cmd for the available optimization regressions and
the VR build, and python vr/tools/test-pass-analysis.py for analyzer tests.
Run gameplay/tests/run.sh and gameplay/build.sh under the documented MSYS2
mingw32 environment, plus python gameplay/tests/stock_traffic_source_tests.py.
The optional custom-driver asset tests require an external driver-runtime.bin;
the optimization runner explicitly reports that skip.

Headset visual validation of the depth/effects correction remains required.
The available VR optimization regressions, stereo hook tests and x86/x64 VR
build passed for this update, as did the pass-analyzer tests, stock-traffic
source checks and gameplay regression tests. The optional custom-driver asset
test was explicitly skipped because its external runtime asset is absent.
No guaranteed FPS gain, nitrous fix, GPU-pass attribution, upscaler, second-eye
threading or new world-geometry batching is claimed by this update.