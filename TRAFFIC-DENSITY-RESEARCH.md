# More traffic: findings, 2026-10-31

## Independently checked in this installation's speed.exe

`traffic-density-disasm.txt` shows the Quick Race traffic selection mapping:

| Selection | Value | Immediate-byte address |
| --- | ---: | --- |
| Off | 0 | 0x56DD92 |
| Low | 10 | 0x56DD9E |
| Medium | 30 | 0x56DD9A |
| High / Maximum | 50 | 0x56DD96 |

The branches pass that integer to 0x5E9A90. `traffic-density-setter.txt`
independently shows this setter clamping negative inputs to 0 and inputs above
100 to 100, then passing the value with attribute hash 0xC64BC341 to 0x5E74C0.
These are density units, NOT a verified number of simultaneously active cars.

Extra Options uses precisely the three nonzero immediate-byte patches above:
https://github.com/ExOptsTeam/NFSMWExOpts/blob/master/NFSMWExtraOptions/ExtraOptionsStuff.h
Its supported settings are [TrafficControllers] Low/Medium/High, 0..100:
https://github.com/ExOptsTeam/NFSMWExOpts/blob/master/NFSMWExtraOptionsSettings.ini

Consequences:
- Stock Maximum only requests 50. Requesting 100 is an evidence-backed first
  experiment, but does not guarantee twice the cars.
- Writing 200 to the selection does not overcome the setter's 100 clamp.
- Patching the Quick Race selection alone does not establish a change to career
  races or free roam; their density producers must be traced separately.
- No Extra Options module was present in the inspected scripts directory.
  Adding it wholesale risks unrelated patches/hotkeys (including F4) conflicting
  with this installation. Prefer a narrow, signature-checked implementation in
  the existing gameplay mod if this feature is implemented.

## Other evidence and limits

SDK trafficpattern layout declares per-vehicle mRate, mMaxInstances and mPercent:
https://github.com/s-b-repo/nfsmw-2005-re/blob/main/docs/nfsplugin_sdk_mw05/Types/Attrib/Layouts/trafficpatternlayout.h
This establishes that per-pattern/per-type data exists, not that increasing any
one field expands a global traffic pool. Actual database values and consumers
have not been verified in this installation.

The reverse-engineering project's traffic report is explicitly PARTIAL and
static-only, with unverified spawn/density questions:
https://github.com/s-b-repo/nfsmw-2005-re/blob/main/docs/wave10_ai_traffic.json
Its skip-FE density globals are for a debug/bootstrap path, not proof of a live
free-roam density override. Its spawn-slot addresses are investigation leads,
not safe patch targets without independent disassembly and runtime checks.

## Recommended implementation/research sequence

1. Add a narrow, default-off density override bounded 0..100. Verify original
   instruction bytes and call ABI before patching. Start with Quick Race High=100.
2. Compare fixed routes at 50 and 100; log active civilian count separately from
   police/racers and attempted/successful spawns. Test career and free roam too.
3. Trace the consumers of density hash 0xC64BC341 and trafficpattern constraints;
   identify which input/limit actually saturates when count stops increasing.
4. Trace global spawn-slot capacity, allocations and every indexed user before
   expanding a pool. Never raise a loop bound without expanding its backing data.
5. Test race transitions, restarts, pursuits, streaming, frame time and memory.
   Keep signature checks, conservative limits and a rollback switch.

No traffic patches or database edits were installed in this task. A specific
global cap (for example 8 or 16 cars) has NOT been established; claiming one now
would be speculation. Extra Options' generic ExpandMemoryPools option is not
evidence of a traffic-cap fix.