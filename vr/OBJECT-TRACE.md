# Object workload frame tracing

Press the configured `[VR] KeyTrace` (Delete by default) during the slowdown.
After 30 seconds the mod writes `nfsmw_frametrace_NNN.csv` and
`nfsmw_frametrace_NNN_models.csv` beside speed.exe. Existing frame CSVs are not
overwritten when restarting the game.

The frame CSV adds player-car and other-car render-hook CPU timings, nested
solid-cache CPU time, cache rebuilds, and submission/triangle counts by category.
These timings overlap eye rendering; do not add them to the existing phases.
Counts cover submitted entries across views/eyes, not unique cars, actual D3D draw
calls, simulation population, collisions, or break events. Repeated submissions
are intentional. Model samples contain up to eight distinct names per category
per frame; excess submissions increment `model_overflow_submits`. Category totals
are not truncated. Samples store only copied names and counts, never game pointers.
Frame index in the model file is zero-based; `t_s` joins it to the frame CSV.

`player` uses the existing player render-connection context. `traffic_hint`
(`TRAF*`), `cop_hint` (`COP*`) and `breakable_hint` (`BREAK` or `DEBRIS` in the
name) are explicitly **name heuristics**, not verified vehicle roles or a complete
breakable classifier. Remaining car submissions are `other_car_unknown_role`:
opponents can be investigated here by their actual model names, but cannot safely
be distinguished from other non-player vehicles with the currently verified
hooks. Other props, including breakables without recognizable names, remain
`other_world` and retain their sampled model names. No guessed game offsets are
used to claim AI roles or impacts.

Sampling is bounded, allocation-free and file-I/O-free on the render thread.
CPU clocks are taken only during a trace; CSV output uses the existing worker.
This is diagnostic instrumentation, not a demonstrated framedrop fix. It does
not change eye resolution, independent stereo, traffic density or physics.