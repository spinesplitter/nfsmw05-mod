# Traffic bottleneck investigation

The live log confirms the previous plugin loaded and its build/installed SHA256
matched. At 10x, samples recorded 303–537 attempts but only 3–9 successes per
15 seconds. A larger multiplier alone is not evidence of more visible vehicles.

This diagnostic revision preserves every original spawn decision. It redirects
three signature-checked direct calls in `0x439A10`, invoking each original once:

- `0x439A39 -> 0x437DC0`: road-position search.
- `0x439A44 -> 0x426230`: traffic-type selection (includes rate and type limits).
- `0x439A75 -> 0x426380`: reuse/create vehicle, with original creation permission.

The `traffic: stages` line reports successes/calls for each stage, creation
permission count, attempts stopped before road search, and failures after a
vehicle was returned. All counters reset with the existing 15-second sample.
The initial sample is partial; use subsequent complete samples. Drag/online
decisions are observed but never changed by these wrappers.

## Next drive

Restart the game and confirm `road/type/vehicle rejection diagnostics installed`
in the gameplay log. Drive the same route at 1x, 3x, and 10x, allowing at least
30 seconds at each setting, then repeat a non-drag race and a drag race.

Interpret the first stage with a large drop:

- Road: investigate position eligibility, spacing and visibility.
- Type: investigate live per-pattern rate/count restrictions.
- Vehicle: investigate reuse availability, creation permission/timer and pool.
- Post-vehicle: investigate activation and AI placement rejection.

No fixed bounds, pools, collision checks, or spawn visibility rules have been
expanded or disabled. This build identifies the bottleneck; it does not yet
claim to fix traffic density. Keep the game closed for installation/rollback.