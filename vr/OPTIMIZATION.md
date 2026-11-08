# VR optimization

Integrated after 953eee7 (native animated driver update), including the subsequent
driver/mirror optimizations and companion-view split-depth correction.

- Batch native mirror UV mapping/restoration into one validated buffer lock each.
- Cache deterministic rejected wheel geometry (bounded, owning copies; invalidated by wheel edits).
- Add bounded object submission sampling, nested CPU timings and render-boundary CSV diagnostics.
- Preserve the original render calls and x87/SSE state around twenty verified render call sites.

Eye-buffer resolution is unchanged. The default world near plane is now 0.5 m;
the distributed cockpit NearZ remains 0.091 m. Companion views that share the
main camera use consistent split-depth mapping, including cockpit variants.
Invalid matrices and aliased storage are guarded, and variant storage is bounded.
Existing installations retain their saved settings unless explicitly changed.
Stock-traffic restoration is documented separately in gameplay/STOCK-TRAFFIC.md.
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
