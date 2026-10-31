# Physics diagnostics and telemetry visibility

- With `LogPhysics=1`, hide telemetry using its configured binding. Drive above
  30 km/h, launch from rest, and exercise ABS/TC. Periodic physics, launch and
  telemetry diagnostic lines must stop (independently enabled LogInputs may still
  report aids). Startup/car-change messages and safety warnings remain available.
- Show telemetry. Verify live forces, tyres, suspension and pedal indicators;
  physics diagnostic lines resume at their existing throttled frequencies.
- Hide it again: diagnostics stop without disabling physics or the panel binding.
- Set `LogPhysics=0`, restart, and show the panel: it remains fully functional
  without periodic physics diagnostic output.
- Pause/leave driving: a non-visible panel must not enable physics diagnostics.

The automated regression tests cover the logging/visibility gate truth table;
these live checks require the game.