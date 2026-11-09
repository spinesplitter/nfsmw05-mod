// Physics sliders for the player's car: driving aids, grip, torque split, gravity.
#pragma once
#include <string>
#include <stdint.h>
#include <vector>

struct IniWrite;
struct CarCharacter;

// Hooks the player's suspension and rigid body simulation. Called from Game_Install.
void Physics_Install();
// Once per frame (game thread), reads tuning tables the game builds at startup.
void Physics_Frame();
// Make it rain: force the player view's rain intensity while on (the F4 testing button).
void Physics_ForceRain(bool on);
bool Physics_RainForced();
// The VR mod's wiper stroke counter: plays the stitch-static wiper sound when it goes up.
void Physics_WiperSound(float* strikes);

// Testing: lifts the player's car [Tires] DropHeight metres straight up at its next physics step
// (vertical and spin speed zeroed, forward speed kept) and lets it fall. Any thread.
void Physics_DropCar();
// The player's car's speed (m/s) at its last physics step.
float Physics_PlayerSpeed();
// The throttle the game's chassis took at the player's last physics step (0..1).
float Physics_GasInput();
// The player's car at its last suspension step, for the telemetry panel. Taken whatever the physics
// settings. Game thread. The world is the sim's (y up); wheels front left, front right, rear left,
// rear right.
struct CarSample {
    uint32_t step = 0;          // counts the player's suspension steps
    float dt = 0.0f;            // the last step's length (s)
    bool body = false;          // the body below came with the last step
    float right[3] = {}, up[3] = {}, fwd[3] = {};   // the car's axes
    float vel[3] = {};          // m/s
    float mass = 0.0f;          // kg
    float weightFront = 0.5f;   // FRONT_WEIGHT_BIAS: the front axle's share of the weight at rest
    float downforce = 0.0f;     // N, as applied (below 0: lift)
    float aeroFront = 0.5f;     // AERO_CG: the front axle's share of it
    bool wheels = false;        // the wheels below came with the last step
    struct Wheel {
        bool onGround = false;
        float compression = 0.0f;   // ISuspension::GetCompression
        float load = 0.0f;          // N
        float side = 0.0f;          // the tyre's side force (N)
        float along = 0.0f;         // its force along the wheel, (drive + brake torque) / radius (N)
        float grip = 0.0f;          // its grip limit (N), 0 = not known
        float kept = 1.0f;          // the share of its force it kept: below 1 it was over the limit
    } wheel[4];
};
const CarSample& Physics_Sample();
// Traction control: the share of the throttle to send now (1 = all of it; 1 with TC off).
float Physics_TcThrottle();
// ABS / traction control working right now (for the HUD icons). Game thread.
void Physics_Aids(bool* absActive, bool* tcActive);

// Preset settings (game thread). The player's car changed: its own or group values replace the
// [Physics] Shared values in g_cfg until another car is driven.
void Physics_SetCar(const char* vehicleName);
const char* Physics_Car();                  // "" until a car is known
// The current car's Character and real weight factors and stock data (cars.h), as of its last
// physics step; `known` false before one, or for a car the game's data doesn't have.
const CarCharacter& Physics_Character();
// Where the current car's physics values come from, and are saved: its group's [Group.<name>]
// section, or the shared [Physics].
const char* Physics_CarGroup();             // the group it is in, "" = Shared
std::string Physics_Section();              // where the current preset's values are saved
void Physics_UseShared();                   // the current car on [Physics], out of its group
void Physics_UseGroup(const std::string& name);    // into a group (its values from now on)
// Groups (game thread): names in the ini's order; their cars (CARS folder names).
std::vector<std::string> Physics_Groups();
std::vector<std::string> Physics_GroupCars(const std::string& name);
// A new group, "Group <n>", from the values in use (the car being driven), with no cars yet. Its name.
std::string Physics_NewGroup();
// A group's value of a physics setting (`value` = its g_cfg field), and setting it (written at
// once; the car being driven gets it too if it is in the group).
float Physics_GroupValue(const std::string& name, const void* value);
// A per-preset value changed in g_cfg: write it into the active car's preset now.
void Physics_CommitPreset(const void* value);
void Physics_SetGroupValue(const std::string& name, const void* value, float v);
float Physics_GroupTcMode(const std::string& name);
void Physics_SetGroupTcMode(const std::string& name, float mode);
// Renames a group; nullptr if done, else why not (empty, [ ] = ; , or taken).
const char* Physics_RenameGroup(const std::string& from, const std::string& to);
void Physics_DeleteGroup(const std::string& name);
// Profiles: a group's values and cars in a file of its own, in scripts\profiles\, to share.
// Export writes "Group - <name>.ini" (every value filled in) and returns its path ("" if there is
// nothing to export). Import makes the group or replaces the one with its name, moving its cars.
struct PhysicsProfile {
    std::string file;           // in the profiles folder
    std::string name;           // the group's name
    std::vector<std::string> cars;   // a group's cars
};
std::string Physics_ProfileDir();
std::string Physics_ExportGroup(const std::string& name);
std::vector<PhysicsProfile> Physics_Profiles();
std::string Physics_ImportProfile(const std::string& file);
// Adds a car to the group (out of any other) or takes it out; the car being driven switches now.
void Physics_SetInGroup(const std::string& group, const std::string& car, bool in);
bool Physics_IsPresetValue(const void* value);   // a g_cfg field saved with Shared or a group
// Removals of the old keys that are gone (FrontGrip, RearGrip: now GripBalance) from where the
// sliders are saved, for the menu's save.
void Physics_EraseOldKeys(std::vector<IniWrite>* writes);
// How far behind where the game puts it the current car's engine smoke comes out (m): the
// rear- and mid-engined cars have their engine behind the driver. 0 for the others.
float Physics_EngineSmokeBack();
float Physics_DefaultEngineSmokeBack();
