# Stock automatic brake bypass

The byte-checked call at 0x6A8D92 is redirected, not the pedal-processing or engine-braking code.
The bypass is active only inside the non-classic Controls FetchInput call.

- On level ground coast to rest with both pedals released: no stock automatic brake application.
- Repeat in forward gears, reverse and neutral; on a slope the car may roll naturally.
- Apply light and full service brake, and the handbrake: each must still work.
- Compare in-gear coasting with clutch disengaged/neutral: engine braking must remain.
- Switch ClassicInput on: stock automatic braking must return. Switch off: bypass returns.
- Pause/resume and change cars: check pedal release and braking again.
- Verify startup log reports the bypass, or a safe unchanged-code warning on incompatible executables.

These require live game validation; the general regression suite does not emulate FetchInput.