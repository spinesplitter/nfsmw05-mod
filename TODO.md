# To do

## Plan: simcade physics rework for wheel and pedal users

**Status 2026-10-20:** implemented, then reverted at the user's request (they preferred the old
model). The whole rework is kept in nfsmw05-mod/physics-rework-2026-10-20.patch (uncommitted).

Ground rules: audit first; one feature at a time, each built, tested (gameplay/tests/run.sh) and
checked in game before the next; no memory offset used unless found in speed.exe and logged as
verified; where real tyre-level control is missing, say so and use the closest approximation. Every
new slider in F4 > Physics, with "Settings for this car only" (the existing Physics_IsPerCar /
[Car.<name>] mechanism in physics.cpp / ui.cpp). Build gameplay in the scratch copy with the
committed ui.cpp + patches while the user's own ui.cpp edits are unfinished (see the save patch).

### 0. Audit (read only, ~first hour)
- What physics.cpp already hooks: SuspensionRacer::OnTaskSimulate 0x6B1DD0, RigidBody::OnBeginFrame
  0x6A7050, GlobalGripScale 0x8AAC94, TractionForwardSlip 0x8AAD94, MaxYawBonus 0x8F8D3C,
  DriftRearFrictionTable 0x8F8FB0, DriftStabilizerTable 0x938210, tyre force ellipse 0x69DF61 /
  0x69E03A, DoDrifting call 0x6AA006, sliding friction 0x69DD22, surface LATERAL_GRIP read 0x69E098,
  blown tyre branch 0x6A9ECA. Map each to the features below.
- Per tyre: what the Tire struct exposes (slip angle +0x118 is known from controls.cpp; find slip
  ratio, load, wheel angular velocity, "sliding" state, driven flag). Decides whether wheelspin and
  slide grip can be per tyre or only per axle.
- Drivetrain data: the car's attributes (TORQUE_SPLIT or equivalent in the transmission/engine
  Attrib layout, kER_TranyLayout +...), how the torque reaches each wheel (where TorqueSplit acts).
- RPM / limiter: EngineRacer rpm +0x180, RED_LINE / MAX_RPM in the engine layout (+0x58 / +0x5C
  known), the clamp(s) that hold rpm at the limit.
- Surfaces: how grass differs from asphalt (its LATERAL_GRIP / drive grip values, rolling drag),
  and whether a tyre's surface pointer or only its values can be swapped for one step.
- Inputs: how gas/brake/handbrake reach InputPlayer (controls.cpp queues ActionQueue values) and
  what the game does with them afterwards (filtering, ramps, the "gamepad" brake/throttle curves).

### 1. Remove old sliders (small, first)
Remove the sliders and what they drive: Front torque share (TorqueSplit), Drivetrain multiplication
(Drivetrain), Rear grip (oversteer) (RearGrip), Slide grip (SlideGrip), Slide oversteer
(SlideOversteer), and **Spin limit (MaxSpin)** (confirmed by the user: that is the "spin radius"). Keep old ini keys harmless (ignored, not deleted); update the defaults fingerprint
(ini_migration.h) and the README / FIXES.

### 2. Pedals: RPM, throttle and brake response, analogue handbrake
- Log raw pedal value -> what the game uses (throttle/brake after its own filtering) -> engine rpm
  response, to see the lag and curves the game adds for keyboards / pads.
- Bypass or flatten the game's input ramps for bound analogue pedals (linear, no smoothing beyond
  a tiny filter), keep the keyboard path as it is.
- Analogue handbrake: bind an axis to Handbrake (BINDING currently a button), feed a value; if the
  game's handbrake is on/off only, approximate by modulating rear-wheel brake torque / the rear grip
  the handbrake applies, proportional to the axis.

### 3. Drivetrain classification (needed by 4-7)
FWD / RWD / AWD per car from the game's own data (torque split attribute), logged per car;
per-car override in F4 ("Drivetrain: Auto / FWD / RWD / AWD", saved for this car only).

### 4. Rev limiter removal (fix: "currently not working")
Today Hook_UseRevLimiter (game.cpp 694) only returns false when WearApplies (engine wear on, manual
shifting, not staging), so with automatic or wear off the limiter stays, and the engine may also be
clamped elsewhere (rpm capped at MAX_RPM, a fuel cut in the gearbox / shift logic). Find every clamp,
lift them when [Engine] RevLimiter=0 regardless of wear, keep engine wear's heat model in charge of
over-revving; verify in the log that rpm passes the redline.

### 5. Speed & cornering grip (Low / Normal / High slider)
Grip falls progressively once the lateral demand (speed^2 x curvature, or the tyres' slip angle
against their peak) exceeds what they hold, front vs rear by load and drivetrain, so the car
understeers or oversteers believably. Reuse SpeedGripLoss / Grip scaling where possible; per axle if
per-tyre is not available.

### 6. Grass physics as slide feel (feasibility test first)
Test whether, for a sliding car on asphalt, the grass surface's lateral values can be used for the
sliding tyres for that step (the LATERAL_GRIP read at 0x69E098 already has a hook point) without
touching the real surface, drive or braking grip. Measure the effect in the log; if drive/brake are
affected or it is unstable, fall back to scaling only the lateral sliding friction (0x69DD22).

### 7. Slides: initiation and recovery (sliders)
- Initiation: excess cornering speed (from 5), handbrake (rear lateral grip drop while held,
  analogue from 2), power oversteer on RWD (and AWD by its rear share) when throttle x rpm exceeds
  rear grip. Slider: slide initiation Low / Normal / High.
- Recovery: grip restored progressively as the slip angle falls (a time constant, never a snap),
  plus drift assist Off / Low / High that helps countersteer (reuse DriftStabilizerTable /
  MaxYawBonus / DoDrifting; FWD pulls straight with throttle, RWD needs countersteer and lift).

### 8. High-RPM launch wheelspin (slider)
At low speed with high rpm and throttle beyond the driven tyres' grip: driven wheels spin up
(lower drive grip / TractionForwardSlip on the driven axle) and grip returns as wheel speed and
road speed converge. Scaled by throttle, rpm, speed, grip and drivetrain (RWD rear, FWD front, AWD
by split). Bounded in time and strength: no permanent spin, no extra acceleration.

### 9. Lift-off behaviour (keep the game's own engine braking)
The game's engine braking stays as the deceleration source; nothing new slows the car. Add only the
handling side of lifting the throttle: weight moves forward (see 11), so the rear lightens and the
front bites; in a corner a lift tucks the nose in, and on RWD (and AWD by its rear share) a sharp
lift at high slip starts a gentle, progressive rotation. Scaled by how fast the throttle closes,
speed, lateral load and drivetrain (3). Audit: where the game applies its engine braking torque
(EngineRacer, driven wheels), so the lift is detected from the same throttle / torque values.

### 10. Wall bounce
Cars bounce off walls with too little energy lost ("pinball"). Audit the collision response for
the player's car against the world (DamageVehicle::OnImpact 0x69C030 is already hooked for damage:
it gives the impact normal, force and speed; find where the rebound velocity / restitution is set).
Damp the rebound along the wall's normal, more at shallow angles (a scrape slides along the wall
instead of springing off), keep the speed along the wall mostly, never add energy. Player car only;
props and other cars unchanged. Slider: wall bounce (game's / less / none).

### 11. Simple weight transfer, with an engine position per car
Front / rear grip follows the load: braking and lifting move load to the front, accelerating to the
rear, cornering to the outside. Approximated per axle (front and rear grip multipliers from the
car's longitudinal and lateral acceleration, from RigidBody), smoothed so it builds and fades over
a few tenths of a second, never stepping. F4 > Physics: **Engine position Front / Mid / Rear**,
setting the static front/rear balance the transfer starts from (front-heavy understeers and pushes
under braking less; rear-engined cars get more traction and snappier lift-off), plus a weight
transfer strength slider; both saveable for this car only. Order: implement 11 before 9, which uses
its load shift.
The same Engine position also places the mod's engine smoke (vfx.cpp, the damaged / hot engine
smoke trailing the car): from the front of the car for Front, the middle for Mid, the rear for
Rear, so a rear-engined car smokes from the back.

Each step ends with: build, tests, install with a backup, one line in release/FIXES.md, and a note
here of what the log showed in game.

## Session 2026-10-20 16:21-16:48: three frame traces read (nfsmw_frametrace_001-003.csv)

- **Frame time is mostly flat now:** median 13.9 ms, p95 15.0-15.3 ms, 12-26 frames over 20 ms per
  30 s (0.6-1.2%). Render thread MMCSS + 1 ms timer took effect ("render thread registered").
- **Physics step does not move frame time:** CPU work 8.44 ms with 1 step vs 8.48 ms with 2. The
  1/144 s idea is only about motion smoothness now (still parked).
- Reflection face pairs: +-0.3 ms on a 3-frame cycle (eye 0 3.65-4.5 ms by pair). HUD probe frames:
  +0.5-0.75 ms. Both minor; leave unless the rest is done.
- **The late frames left are three kinds:** (a) headset wait ~19 ms with normal work: the runtime
  held the frame a slot (SteamVR / Link side; try Motion Smoothing off, fixed throttling, VDXR);
  (b) "update" spikes 15-23 ms in the game's own update (streaming?); (c) rare GPU spikes, eye 0 GPU
  up to 21 ms, frame GPU up to 25 ms.
- **Shared shadows still end:** "differ" lines at 16:22-16:23 with differences of 1-25% (projection
  [3][0] 2.132 vs 2.099, [3][3] -0.448 vs -0.357, [3][2] -4.447 vs -4.338) on frames that reused the
  map, so the 1% drift tolerance does not cover them; off at 16:23:52, all three traces per eye.
  **DONE, installed** (backup mod-backup-vr-asi-oneshadow-20261020): hkShadowPass in vr/src/game.cpp
  always drops the second eye's pass and writes the first eye's state over it (no pause, no
  switch-off; only non-finite first-eye values skip it for that frame). Check the log for "the
  second eye uses the first eye's shadow matrices" and the trace's shadow column = 1.
  Proposal (stereo-safe by construction): on a shared frame always write the first eye's shadow
  matrices and light over the second eye's, whatever the difference (keep pausing only on NaN /
  infinite values), since the map and those matrices are a matched pair made from the head-free
  seat camera. Still worth finding why the second eye's pass differs that much (the pass may fit
  near/far to the casters it culls).
- **Smoke moves with the head (reported 16:48), not fixed:** not sure of the cause. Particles use
  the world depth range, so probably not the depth split. Likely the game builds particle quads once
  a frame (before the first eye) from the camera current then, so both eyes share quads tied to
  that camera. Tests next session: F4 > Menu and testing > Depth split off (does it change?); tyre
  smoke vs the mod's engine smoke (vfx.cpp); then find the game's particle quad build and which
  camera it reads. Fix only once the cause is known; both eyes must keep identical particles.

## Fixed 2026-10-20 15:35 (installed, backup mod-backup-ffbnames-shadowdrift-20261020) - check the next log

- Items 1 and 2 below: the second eye takes the first eye's shadow matrices and light on shared
  frames (look for "the second eye uses the first eye's shadow matrices" and no "shared shadow map
  off"); the FFB device names are listed without g_diLock (no ~200 ms freeze after "ui: menu opened").
- Still open: the physics step (left for later), the per-frame trace (now the Delete key) not yet recorded. Also installed: render thread as an MMCSS "Games" task + 1 ms timer, GPU wait yields after 1.5 ms (look for "render thread registered" in the log).

## Next session: lag in the race of 2026-10-20 05:23-05:35 (logs read, nothing coded)

Builds in the game: VR with the shadow pause fix + per-frame trace (mod-backup-vr-asi-trace-20261020),
gameplay with the background settings save + background FFB device names
(mod-backup-shadows-save-20261020). 109 driving intervals, median 71.3 fps, 171 hitches, 91 of 109
five-second intervals had a frame over 20 ms.

1. **Shared shadow map still turns itself off, now after ~70 s.** The pause fix works (sharing
   started at 05:24:29), but real mismatches on frames that reused the map come every 10-20 s and
   the 5th ended sharing at 05:25:36; the rest of the race drew the shadow map per eye. The values
   differ slightly: view 15 projection [3][0] 6.19969 vs 6.1623, [3][1] -21.6439 vs -21.6462,
   [3][2] -1071.8 vs -1071.69, light [2] 0.0195127 vs 0.019615. So the pass still reads something
   eye- or time-dependent (the light moving between the two eye passes? the player view's own
   matrices, see "Shared shadow map: still eye-dependent" below). Next: find what sub_6E44C0 reads
   besides the camera, or capture the first eye's pass outputs and force them on the second eye
   instead of comparing. Stereo rule: both eyes must keep the same map and matrices.
2. **~200 ms freeze at every F4 menu open is still there** (05:30:54, 05:32:01, 05:32:14, 05:32:19,
   each ~0.2 s after "ui: menu opened"), now in the "update" phase (the game's own update). The
   device names are fetched on a background thread now, so the likely cause is lock contention:
   that thread's EnumDevices holds the FFB mod's g_diLock (ffb_device.cpp Ffb_DeviceName) while
   the game's input polling through the FFB proxy waits on it. Next: cache the device list inside
   the FFB mod (enumerate once, again on WM_DEVICECHANGE) and answer NfsmwFfb_DeviceName from the
   cache without the lock; confirm with the trace that the freeze is gone.
3. **Background settings save works:** 193 settings written in 528-539 ms off the game thread, no
   hitch at the three saves.
4. **No frame trace was recorded** (no nfsmw_frametrace_*.csv). Press Scroll Lock while driving in
   traffic next time; it is needed before the physics step / reflection face order / HUD probe
   decisions.
5. **Missed headset slots:** 9366 display slots skipped over the session in the host log (includes
   loading and pauses). Headset wait 4.7 ms average left; eyes CPU 7.2 ms average. Read together
   with the trace.
6. Long frames at 05:24:09 (4.7 s) and 05:27:37 (101 s) are loading / pause, not driving.

## Done 2026-10-20 (gameplay build installed, backup mod-backup-gameplay-asi-crashfix-20261020)

- Meter: fills the whole arc to 100% (up to the arrow); the arrow keeps its own colour (no glow).
- Blown-engine crash (speed.exe+0x1FDEB5): the car is destroyed only in a race with its parameters.
- Car kept by the AI in free roam: handed back after 10 s AI-driven + 2 s throttle held at a
  standstill; "input:" lines log every block and unblock. Check the next log after a rival race.

## VR: remaining hitches (session 2026-10-20 00:54-00:58, smooth 71-72 fps otherwise)

63 hitches in about 4 minutes, in three kinds:

- **Shared shadow map never ran.** The per-frame guard turned it off at race start both sessions
  ("second eye's shadow matrices differ ... by 2.8452", then "... by 0.488938"), so the shadow map
  is still drawn per eye (1,000-3,500 draws a frame). Likely cause: only the camera's position is
  pinned to the seat during the shadow pass (`hkShadowPass`, game.cpp), but each eye also moves
  the camera's matrix, direction and target. Fix: give the shadow pass the whole head-free seat
  camera (`base` in hkRenderScene) for both eyes; compare only what the pass writes (view 15's
  eViewPlatInfo +0x00..+0xC0, 0x983E00 shadow projection, 0x988000 light), not view 16; log which
  value differs and its two values; tolerate an isolated mismatch while a scene loads and only
  turn sharing off for repeated ones.
  **Status:** done and installed (VR build 2026-10-20, backup mod-backup-vr-asi-shadowguard-20261020).
  Check the next log for "shadow map rendered once a frame", and any "shadow matrices differ" lines.
- **36 missed headset slots.** The frame's own work is normal (eyes 6-9 ms, ~9 ms in all) but
  xrWaitFrame returns a slot late (headset wait 10-20 ms, frame 21-29 ms). Not the game thread:
  something on the runtime / GPU / compositor side. Next: time xrWaitFrame and xrEndFrame in the
  64-bit host and count skipped predicted display times; check SteamVR's Advanced Frame Timing
  graph during a hitch; the runtime is "SteamVR/OpenXR in Meta compatibility mode" (Quest through
  SteamVR), so a native runtime (e.g. Virtual Desktop's VDXR) is worth one test.
  **Status:** host timing added and installed: nfsmw_vr_host.log gets "XR timing" every 5 s (display
  slots skipped, xrWaitFrame/xrEndFrame times). Next: read it after a session.
- **21 game-time spikes of 14-27 ms** (outside the measured render phases), plus loading spikes.
  Next: split the log's "game" bucket into the game update (Present to the next scene render),
  the work before the eyes and the HUD phase, to see where they land. The FFB plugin runs on its
  own thread, so it is not a direct cause.
  **Status:** the update part is split out and installed ("update" in the perf and hitch lines).
  Next: read it after a session.
- 5 one-off eye-render spikes (36-253 ms) when new things first appear: textures or shaders
  created on first use; possibly pre-warm later.

## Rear view mirrors don't show the ground (2026-10-20) - FIXED, check in game

Cause: c6dc3b3's sun glare skip skipped the pass's closing SetRenderState(D3DRS_ZENABLE, TRUE); the
rear view then drew without depth testing and the sky covered the road. The skip restores it now.
Earlier notes:

Reported in the session at 04:40. The rear view still renders (about 100-120 draws each time, every
other frame with RearViewEvery=2) but draws about 40% less than in the 00:04 session (about 410 per
render then, every frame), as if the ground's pieces were left out. RearViewEvery is set back to 1 in
the installed ini; F4 > Menu and testing now has live switches for the rear view interval, the depth
split and the shared shadow map, to find which one (if any) takes the ground away.

## Shared shadow map: still eye-dependent

With the whole seat camera given to the shadow pass, the check still finds view 15's texture matrix
translation ([3][0]) differing between the eyes every frame by 0.03-0.2 (e.g. 9.42 vs 9.59), so it
ends sharing within a second. The pass must read something else of the eye's: likely the player
view's own matrices (eViewPlatInfo of view 1, set per eye by SetupPlayerView) to fit or snap the
shadow area. Next: find what sub_6E44C0 reads from the player view besides Camera +0x40.
