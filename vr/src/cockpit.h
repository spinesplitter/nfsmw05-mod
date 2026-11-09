// Cockpit: driver's seat and the driver model's head.
#pragma once
#include "common.h"
#include "vrmath.h"
#include <vector>

// Captures the values a car model with no saved per-car settings starts from: the mod's built-in
// defaults (Config's own), not the [VR] values. Call once before LoadConfig, so a model you have
// never set up never inherits the last car's HUD or dashboard placement.
void Cockpit_PerCarDefaults();
void Cockpit_Install();
// Called with the game camera's world->view matrix before the eyes are placed; moves it to the
// driver's seat in the bumper and hood cameras. Returns true if it did.
bool Cockpit_ApplySeat(M4& view);
// After Present (including flat-screen frames): refresh the observed car and clear frame state.
void Cockpit_EndFrame();
// Seat adjustment keys, once per frame.
void Cockpit_Keys();
// The physical wheel's angle in degrees (+ = right), set by the gameplay mod through
// NfsmwVr_Setting("SteeringWheelAngle"); the player car's steering wheel is drawn turned by it.
extern float g_steeringWheelAngle;
// Independent prop input bridge; shift values are wrapping press counters, not held buttons.
extern float g_cockpitHandbrake, g_cockpitShiftUp, g_cockpitShiftDown;
// Seat movement from the gameplay mod's bindings while held: right, up, forward (-1, 0, 1) and
// reset (1), through NfsmwVr_Setting("SeatRight" / "SeatUp" / "SeatForward" / "SeatReset").
extern float g_seatInput[4];

// Mirrors: as the player car's mirror glass (side mirrors, the rear view mirror inside) is
// submitted for the current eye, it is handed to the d3d9 side (D3D_AddMirror) to be drawn with
// the rear view right after its model. Before each eye's scene: forget the previous eye's.
void Cockpit_BeginEye();
// Whether `matrix` is a local-to-world matrix the player's own car (body, parts, driver, steering
// wheel) was submitted with for the player view in the current eye.
bool Cockpit_IsPlayerCarMatrix(const void* matrix);
// Whether this index buffer belongs to one of the player car's windows: its draws get two-sided
// culling so the outside glass is visible from inside the cabin.
bool Cockpit_IsWindowDraw(void* indexBuffer);
// The edited wiper triangles of this draw (raw vertex indices), the unique vertices to rotate, and
// the hinge/normal/sweep angle for the current rain. False when this draw has none.
bool Cockpit_WiperDraw(void* ib, unsigned first, unsigned count, std::vector<uint16_t>& indices,
                       std::vector<unsigned>& verts, float pivot[3], float axis[3], float* angle);

// Wiper state for the gameplay mod's sound: the current sweep angle, and a count that goes up once
// per wiper stroke (the game thread plays the two stitch-static samples on it).
extern float g_wiperSweep;
extern float g_wiperStrikes;
// The player car's damage (0..1, 1 = totaled), written by the gameplay mod; the windows crack
// from 75% (car damage, not engine heat).
extern float g_carDamage;
// How many such matrices this eye (0: the car was not drawn in the player view).
int Cockpit_PlayerCarMatrices();
// Deform a private copy of stream 0 for a registered player-driver draw. Never writes game assets.
bool Cockpit_PoseMarkedArms(void* indexBuffer, unsigned first, unsigned triangles, int base,
                           unsigned stride, unsigned positionOffset, int normalOffset,
                           unsigned char* vertices, size_t bytes);
bool Cockpit_HasMarkedArmDraw(void* indexBuffer);
// Cheap draw-hook rejection before any D3D surface queries. Includes UV-only driver draws.
bool Cockpit_IsRegisteredDriverDraw(void* indexBuffer);
// True only for wholly skin-selected ranges; never changes mixed/body draws.
bool Cockpit_IsSkinDraw(void* indexBuffer, unsigned first, unsigned triangles);
// Reject unaffected ranges before touching GPU buffers; copy indices from the live CPU mesh.
bool Cockpit_DriverDrawWork(void* indexBuffer, unsigned first, unsigned triangles, int base,
                            std::vector<uint16_t>& indices);
// Decode negative atlas V on skin-selected driver vertices in a private stream copy.
bool Cockpit_RepairSkinUV(void* indexBuffer, unsigned first, unsigned triangles, int base,
                          unsigned stride, unsigned uvOffset, unsigned char* vertices, size_t bytes);
// Diagnostic identification only; independent of whether arm motion is active.
bool Cockpit_DriverDrawInfo(void* indexBuffer, unsigned first, unsigned triangles,
                           char* name, size_t capacity, int* entry);
// The headset's gaze in the world (centre of the eyes, forward), for the triangle editor's picking
// in the next frame.
void Cockpit_SetGaze(const float pos[3], const float dir[3]);
// After each eye's scene: the triangle editor's selection and reticle drawn over it.
void Cockpit_EndEye();
