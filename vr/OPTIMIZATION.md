# VR optimization

Based on 7441e82 (traffic solid-cache optimization), without the parallel driver-animation branch.

- Batch native mirror UV mapping/restoration into one validated buffer lock each.
- Cache deterministic rejected wheel geometry (bounded, owning copies; invalidated by wheel edits).
- Add bounded object submission sampling, nested CPU timings and render-boundary CSV diagnostics.
- Preserve the original render calls and x87/SSE state around twenty verified render call sites.

No eye-buffer size, stereo policy, INI tuning, gameplay or driver pose changes are included.
No live FPS improvement has been established. Diagnostic timings are CPU wall time,
may include driver waits, and overlap existing phases. Even with tracing disabled,
boundary wrappers execute FXSAVE/FXRSTOR; synthetic tests are not live overhead measurements.

Press Delete (default KeyTrace) and drive for 30 seconds. The usual frame CSV is
accompanied by _models.csv and _passes.csv. Before/after call IDs differ by bit
0x80000000; 00000000 and FFFFFFFF identify scene start/end. Storage is bounded to
128 distinct edges per frame, with overflow reported. Do not add nested timings.

Analyze with `python vr/tools/analyze-pass-trace.py <frame.csv> --output <report.json>`.
Run `python vr/tools/test-pass-analysis.py` and `vr/tests/optimization_run.cmd`.
The analysis rejects incomplete/overflowing partitions and excessive eye-time residuals.
No successful live capture or GPU-pass attribution is claimed.
