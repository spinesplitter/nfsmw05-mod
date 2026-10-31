# Low-speed tyre regression check

Enable custom physics manually in F4; no tuning values or configuration files are changed by
this fix. Keep the same car, surface and settings when comparing old and new plugins.

- On level pavement, release all controls in neutral for 30 seconds. Observe translation and
  yaw after suspension settling. Repeat with the wheel turned and with the brakes held.
- On a slope with brakes released, confirm the car still rolls: there is no position lock.
- Perform gentle and aggressive launches. Excess torque must still produce wheelspin; this
  change preserves stock low-speed longitudinal slip rather than forcing full traction.
- Repeat launches with steering and in reverse. Verify no abrupt step as ground speed crosses
  2 and 5 m/s (7.2 and 18 km/h). Wheelspin alone must not activate the high-speed additions.
- Above 18 km/h, compare cornering and power slides at identical tuning: custom tyre force,
  friction circle and wheelspin amplification must retain their previous full-strength response.
- Check a landing and a low-speed collision: there must be no snapping or artificial body lock.

The regression executable checks blend boundaries, continuity, reverse symmetry, signed-force
preservation, and stock launch traction versus full-strength wheelspin amplification at speed.
It cannot validate the game's contact solver or prove that every source of standstill creep is
fixed; those require the live checks above. No extra lateral/yaw damping has been introduced.