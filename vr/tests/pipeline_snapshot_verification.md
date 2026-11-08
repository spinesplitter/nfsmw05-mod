# Rendering source-snapshot optimization

The native mirror-glass path previously read the entire source vertex buffer for
every draw. It now shares the existing frame-local source snapshot cache. Its key
uses offset zero and byte stride one because the mirror path preserves the whole
buffer. Driver-layout snapshots remain separate identities.

Only source bytes are shared. Every mirror draw still creates a private copy and
computes its own UVs. Writable locks, final release and ProcessVertices invalidate
source entries; existing frame/reset boundaries clear them. If buffer hooks cannot
be observed, the caller invalidates before reading, preserving per-draw fallback.
No cross-frame geometry cache, resolution reduction or mirror refresh change is
introduced. PresentEx and headset waits are unchanged.

Tests: run tests\optimization_run.cmd from the VR source directory. The buffer
hook test covers whole-buffer mirror identity, private-copy isolation, write
invalidation, frame clear, snapshot counters and failed-read accounting. The
existing mirror UV differential and live driver animation tests must also pass.
The optional custom-driver asset test is skipped by the existing runner when its
runtime asset is absent.

Runtime validation still required:

1. Restart the game with the tested binary. Check mirrors, body textures and
   steering/driver animation in both eyes. Confirm desktop RTSS remains visible.
2. Record the same route/car/settings as the baseline capture.
3. Inspect the source-snapshot log every 300 presented frames: hits, read attempts,
   copied bytes, failures, invalidated entries and mirror hook fallbacks. Counters
   aggregate driver and mirror snapshots; invalidations count entries, not locks.
4. Compare mirror map/draw mean and P95, driver readback, eye CPU/GPU elapsed
   timings and frame cadence. Do not claim a speedup from unit tests alone.

Additional changes included in this source update (headset validation pending):

- Fix the actual eye lifecycle: BeginEye(-1) is called between eyes and must NOT
  clear snapshots. BeginEye(0), Present and reset still clear them.
- Use a whole-buffer byte identity for driver and mirror sources, eliminating
  duplicate reads caused solely by different offsets or declaration strides.
  Driver transforms still operate on fresh private per-draw CPU copies.
- Copy/upload only the mirror draw's contiguous vertex span and rebase its draw
  base without altering the index buffer or MinVertexIndex. Multi-stream layouts
  and parts extending outside the span retain full-buffer fallback.
- Log actual mirror upload bytes versus the previous full-buffer equivalent.
- Add real between-eye cleanup sequence coverage and 10000 differential rebasing
  and byte-copy cases, including negative bases, bounds and multi-stream fallback.

Remaining costs: driver GPU readback on genuine misses, game draw submission and
scene/GPU work. Avoid
blind redundant-state suppression: state blocks and real-device internal calls
can change state without passing through the proxy. Existing shared rear-view,
environment-map and shadow work is already enabled. Skipping the second shadow
routine entirely is unsafe: it writes globals needed by subsequent rendering.