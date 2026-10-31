// The telemetry panel: the G-force circle with a trail, each tyre's grip, the suspension's travel
// and the downforce, plus a live steering wheel and gas/brake/clutch/handbrake inputs.
// The chassis data is the player's car as the game simulates it
// (physics.h, CarSample), nothing estimated. Settings in [Telemetry]; [Controls] ToggleTelemetry
// shows or hides it.
//
// Drawn with the game's HUD. With the VR mod, which takes the HUD apart for its in-car display,
// the VR mod draws it instead once that is done (NfsmwVr_SetOverlay), so it stays on the HUD layer
// in the headset; while the menu is open it is drawn here, under the menu.
#pragma once

struct IDirect3DDevice9;

// Once a frame (game thread): the binding, and the needles moved towards the car's last step.
void Telemetry_Frame(bool driving);
// The panel is to be drawn this frame.
bool Telemetry_Shown();
// Diagnostic collection/writes are opt-in and only active with the live panel visible.
// This does not gate the car samples used to draw the panel.
inline bool PhysicsLogEnabled(int logging, bool shown) { return logging != 0 && shown; }
// Draws the panel into the current render target, w x h. Game thread.
void Telemetry_Draw(IDirect3DDevice9* dev, int w, int h);
