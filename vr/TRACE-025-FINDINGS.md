# City slowdown: capture 025

Capture: `T:\Games\ModWanted\driverNeed For Speed Most Wanted Black Edition - Copy\nfsmw_frametrace_025.csv`

The 30.0021-second capture contains 1648 active stereo frames. The pass analyzer
accepted the partitions: maximum disagreement with the eye timer is 0.0046 ms.
Groups contain 1119 fast frames (<16 ms) and 513 slow frames (>20 ms).

| Measurement (mean ms) | Fast | Slow |
| --- | ---: | ---: |
| Frame | 14.0447 | 27.2861 |
| Headset wait | 0.6282 | 12.5245 |
| Both eyes, CPU wall time | 11.1770 | 12.2645 |
| GPU frame timestamp span | 10.9696 | 12.0125 |
| Update | 0.9599 | 1.1963 |
| Driver draw, nested | 1.8794 | 1.8972 |
| Mirror mapping, nested | 0.6470 | 0.7322 |

Nested timings overlap their parent and must not be added to them. CPU wall
time and GPU timestamp spans do not by themselves distinguish active CPU
execution, driver stalls, or GPU dependency waits.

## Located interval

The largest queued-render increase is the call at 0x6DED37 to 0x6E2F50:

- Eye 0: 1.02779 -> 1.39024 ms (+0.36245).
- Eye 1: 1.01054 -> 1.32905 ms (+0.31851).
- Combined increase: 0.68096 ms, about 63% of the eye CPU-wall-time increase.

Read-only disassembly of the installed speed.exe shows the preceding code
passing pointer 0x919650 and flag 0x20 to 0x723FA0,
then calling the queue routine. This locates a specific render queue; it
does not establish the meaning of flag 0x20 or identify individual objects.
Do not bypass the queue, reuse the other eye, or label it an opaque/shadow/
reflection pass without further verification.

Slow frames submit 4952.125 other-world entries versus 4329.699 on fast
frames (+14.4%). Combined eye draw counts rise from 3634.554 to 4311.317
(+18.6%). Driver draw time changes only 0.0178 ms.

## Reflection budget confirmed

`env_face_count` is 1 in both groups. Rear-view refresh averages 0.536 on
fast frames and 0.413 on slow frames. This confirms the reduced cadence is
represented in the trace; slow frames do not preferentially refresh mirrors.
It is not a matched mirrors-off comparison or proof mirrors are inexpensive.

## Limits and next isolation

The extra ~1.09 ms of eye work accompanies ~11.90 ms more headset wait.
This supports deadline amplification, not proof the runtime is defective.
The bounded model table reports substantial overflow; per-model samples
cannot identify the expensive object or prove a specific mod is responsible.
Active instrumentation has overhead, so compare normal play as well.

No rendering code or installed settings were changed during this analysis.
Next: isolate the 0x6DED37 queue and its build interval with
per-queue draw/category counters or a native CPU/GPU profiler; verify flag
semantics before attempting batching/culling changes. A runtime frame-timing
comparison can separately establish compositor deadline/reprojection behavior.