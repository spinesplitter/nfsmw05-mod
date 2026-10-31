# Telemetry driver-input check

Toggle the existing telemetry overlay with its configured binding (Right Shift in the installed
configuration), or enable it in F4. The input row follows the panel's size, position and opacity.

1. Centre the physical wheel: its top marker should be at twelve o'clock and the angle near zero.
2. Turn right, then left: the graphic must rotate clockwise, then anticlockwise, with a positive,
   then negative angle. At 450 degrees it must show the extra quarter turn, not clamp at 360.
3. Slowly press and release gas, brake and clutch separately. Each matching bar should track
   0–100%, including simultaneous presses, without the chassis telemetry's smoothing delay.
4. Hold the handbrake button: H-BRAKE should show 100%, then zero when released. An axis binding
   should instead show its partial travel.
5. Unbind a control: it should show N/A, not a misleading released value. Classic input also shows
   N/A for steering, gas, brake and handbrake because those game-owned inputs aren't sampled here;
   the mod-bound clutch still works.
6. Check the panel at the intended resolution and in VR: no overlap between the old charts, the
   input row and the bottom statistics; the panel should stay within the screen edges.

These are driver/binding inputs before traction control, not the game's applied engine/brake
forces. No physics or grip values are changed by this display.