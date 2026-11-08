# Conservative stereo visibility foundation

Status: stereo union math remains standalone. An opt-in, exact-input native
calculation cache is runtime-integrated in source; NOT installed.
No live performance improvement or headset correctness is claimed.

`src/stereo_visibility.h` extracts both eye frusta from original D3D row-vector
world-to-clip matrices, before depth remapping. One world-space AABB query returns
left/right candidate bits. Zero means outside both eyes; nonzero is conservative,
not a guarantee of visibility. No occlusion, transparent sorting or LOD is shared.
Invalid matrices/bounds fail open. There is no cross-frame pointer cache.

Run `tests\stereo_visibility_run.cmd`. Tests include eye-exclusive close objects,
invalid inputs and 100,000 bounds compared with independent clip-space corner
tests across translated/rotated cameras and asymmetric projections.

## Runtime integration gate

The current scene renderer still runs per eye. Existing render-boundary hooks
are not verified candidate-collection hooks. Native scenery culling also collects
per-view flare data. Skipping that traversal or reusing left-eye lists would not
preserve the existing stereo behavior.

Before integration:

1. Identify the native static-scenery traversal and list ownership/lifetime.
2. Verify bounds coordinate spaces, transforms, streaming generations and writes.
3. Separate candidate collection from flares, LOD and other per-eye side effects.
4. Supply both actual eye projections, not a guessed centre-camera FOV.
5. Compare proposed masks with native candidates without suppressing native work.
6. Only then reuse static candidates within one frame, retaining native fallback
   for unsupported inputs, overflow, dynamic geometry and mismatched view types.
7. Measure CPU time and test both eyes at frustum edges and near objects.

Hands/cockpit, mirrors, reflections, shadows and dynamic objects remain on their
existing paths. The helper is intentionally not included in game.cpp until the
integration gate is met; compiling a helper alone does not optimize the game.

## Integrated exact-input calculation sharing

`src/stereo_native_calculations.h` and `game.cpp` now integrate bounded,
frame-local value-keyed caches at the existing WorldView/WorldViewProj hooks
and two verified native scenery bounds call sites (0x7291FD, 0x7296C1).
The latter call 0x4FCA70, a jump to 0x6CF2B0. That routine reads six float4
planes at offset 0x140 of the view platform. Its complete 331-byte body is
fingerprinted at startup; altered implementations disable both caches.

This is NOT shared stereo candidate traversal. Native TreeCull, DrawAScenery,
DoCulling, per-eye LOD, preculling, flare collection, list allocation and draws
still execute. Different eye inputs miss the cache. Only genuinely identical
calculations, including stable-reflection matrices and repeated same-frustum
bounds queries, can be reused. Hashing/copying overhead may exceed savings;
no CPU or FPS improvement has been demonstrated.

All key data is copied: matrices, planes, bounds and floating-point control
modes. No streamed object pointer is retained. Collisions replace entries,
never reuse mismatching keys. Transformed bounds, invalid inputs, nonfinite
products, unsupported executable code, disabled/flat-screen rendering and a
latched verification failure take the native path. Existing cockpit depth
selection and stable-reflection substitution occur BEFORE cache lookup.

Experimental configuration (restart required):

```ini
[VR]
SharedStereoCalculations=1
VerifyStereoCalculations=1
```

Both defaults are conservative: sharing is off; verification is on. Verification
recomputes EVERY hit using native code, compares the result, returns the native
result, and latches both caches off for the session on disagreement. It does not
save calculation work. Every 300 stereo frames the log reports hits/misses.
Only after a controlled headset comparison should `VerifyStereoCalculations=0`
be considered for profiling. Set `SharedStereoCalculations=0` and restart to
restore the native calculation path. No INIs, installed DLLs or hand meshes are
modified by this work. Keep DepthSplit=1 and the installed NearZ=0.091.

Run `python tests\validate_stereo_calculations.py` from this source tree. It
records subprocess exits, executable fingerprints and separate test, regression
and full-build logs under `stereo-calculations-*`. Tests exercise distinct eye
inputs, same-address mutations, FP-mode changes, collisions, frame expiry,
invalid inputs and automatic fallback, plus the existing 100,000-bound suite.

Native reference inspected: upstream `dbalatoni13/nfsmw`, `Scenery.cpp`,
`Scenery.hpp`, and PC 1.3 symbols (commit 80993f35224c61d2f1b135beba76c6f8234c7940).
Console structure layouts are NOT used as PC layouts. PC DrawAScenery and
TreeCull were disassembled directly. Shared candidate collection still requires
the integration gates above; copying the first eye's LOD-tagged draw list would
not satisfy them.