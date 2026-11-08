// No heavy support ([Traffic] NoHeavySupport): heavy support is the game's ramming SUVs (copsuv /
// copsuvl, "Rhinos") that a pursuit sends at you head-on out of oncoming traffic. They are asked for
// by name in GetHeavySupportVehicles, through AICopManager::GetAvailableCopVehicleByName (reuses an
// idle cop of that name or creates one); that one call is redirected and gets nothing, so the
// game drops the request. Every other cop spawn is left alone: SUV patrols and SUVs in roadblocks
// still come.
#include "traffic.h"
#include "common.h"
#include "patch.h"
#include <math.h>

// Traffic density, spawn scheduling, type limits and race rosters remain stock.
namespace {
int RaceMode() {
    auto race = *(uint8_t**)0x91E000;
    return race ? *(int*)(race + 0x1964) : 1;
}

constexpr uintptr_t kCopByName = 0x41ECD0;          // IVehicle* AICopManager::GetAvailableCopVehicleByName(const char*)
constexpr uintptr_t kHeavySupportCall = 0x41F1CD;   // GetHeavySupportVehicles: copsuv / copsuvl

typedef void*(__thiscall* CopByName_t)(void* mgr, const char* name);

void* __fastcall HeavySupportByName(void* mgr, void*, const char* name) {
    if (g_cfg.noHeavySupport) {
        static int logged;
        if (logged < 5) {
            logged++;
            Log("traffic: heavy support (%s, head-on ramming SUV) not sent", name ? name : "?");
        }
        return nullptr;
    }
    return ((CopByName_t)kCopByName)(mgr, name);
}

void Cops_Install() {
    if (!RetargetCall(kHeavySupportCall, kCopByName, (void*)&HeavySupportByName)) {
        Log("traffic: unexpected heavy support code, heavy support stays");
        return;
    }
    Log("traffic: heavy support (head-on ramming SUVs) %s; SUV patrols and roadblocks as the game's",
        g_cfg.noHeavySupport ? "off" : "on");
}

}  // namespace

// Drag races: the race gives every car the drag driver style (STYLE_DRAG, PVehicle::SetDriverStyle),
// which swaps the car's parts for drag ones: InputPlayerDrag (steering becomes lane changes, the
// game steers), EngineDragster (its own heat and shift model), DamageDragster (totaled by a hit).
// None of that works with a wheel, the shifter and the gameplay mod's engine and damage, so with
// [Races] NormalDragRaces the player's car keeps the normal style: the game's normal input,
// engine and damage, and you steer. Opponents still race as drag cars, and the race itself (the
// drag HUD, staging, the finish) goes by the race type and is unchanged.
constexpr uintptr_t kSlotSetDriverStyle = 0x8AA854;   // PVehicle's IVehicle vtable: SetDriverStyle
constexpr uintptr_t kSetDriverStyle = 0x687690;       // thiscall(IVehicle*, DriverStyle)
constexpr int kIV_DriverClass = 0x94;                 // PVehicle+0x140 from its IVehicle: 0 = the player
constexpr int kStyleDrag = 1, kStyleRacing = 0;

static void __attribute__((thiscall)) HkSetDriverStyle(uint8_t* vehicle, int style) {
    if (style == kStyleDrag && g_cfg.normalDragRaces && *(int*)(vehicle + kIV_DriverClass) == 0) {
        Log("traffic: drag race - the player's car keeps the normal driving style");
        style = kStyleRacing;
    }
    ((void(__attribute__((thiscall))*)(uint8_t*, int))kSetDriverStyle)(vehicle, style);
}

// The drag HUD: the local player's SetHud(type) (0x6E99C0, thiscall, ret 4; slot 9 of both
// LocalPlayer vtables) builds the FEngHud for a HUD type, and the type picks everything: the package
// (0x5694E0: 2 = HUD_Drag.fng, else HUD_SingleRace.fng for 1), the elements the FEngHud constructor
// (0x5A6600) creates, and the texture pack it loads (0x5A6540: 2 = HUDTEXDRAG.BIN, 1 = HUDTEXRACE).
// With [Races] NormalDragHud a drag race's type 2 becomes 1, the normal race HUD (tachometer,
// speedometer, minimap, damage meter), as fits a drag race the player drives normally.
constexpr uintptr_t kSetHud = 0x6E99C0;
constexpr uintptr_t kSlotsSetHud[] = {0x8B0B34, 0x8B0BF4};
constexpr int kHudStandard = 1, kHudDrag = 2;

static void __fastcall HkSetHud(void* player, void* edx, int type) {
    if (type == kHudDrag && g_cfg.normalDragHud) {
        Log("traffic: drag race - the normal race HUD instead of the drag HUD");
        type = kHudStandard;
    }
    ((void(__fastcall*)(void*, void*, int))kSetHud)(player, edx, type);
}

// The turbo dial (its update at 0x57BB40) is shared by the race and drag HUDs and picks its needle's
// angle range by the race type (GRaceParameters::GetRaceType, 0x5FAA20; 2 = drag) at 0x57BB62. With
// the normal HUD in a drag race the drag range put the needle in the wrong place on the race HUD's
// dial: there it is told the race isn't a drag.
constexpr uintptr_t kGetRaceType = 0x5FAA20;          // int GRaceParameters::GetRaceType(), thiscall
constexpr uintptr_t kCallTurboRaceType = 0x57BB62;
constexpr int kRaceDrag = 2;

static int __fastcall HkTurboRaceType(void* parms, void* edx) {
    int type = ((int(__fastcall*)(void*, void*))kGetRaceType)(parms, edx);
    return type == kRaceDrag && g_cfg.normalDragHud ? 0 : type;
}

// The pause menu's Performance Tuning (Pause_Performance_Tuning.fng: steering, handling, brakes,
// ride height, aero, nitrous, turbo) is stored per car as Physics::Tunings, seven floats, 0 = stock,
// which the physics reads through PVehicle::GetTunings (IVehicle vtable +0xCC, 0x670630; it may
// return null, so every caller copes). With [Physics] GameTuning=0 the player's car reports a
// block of zeros: stock, whatever the menu holds. The menu itself still opens; it has no effect.
// AI cars keep theirs.
constexpr uintptr_t kSlotGetTunings = 0x8AA8F4;
constexpr uintptr_t kGetTunings = 0x670630;   // const Physics::Tunings* PVehicle::GetTunings(), thiscall (IVehicle this)

static const float* __fastcall HkGetTunings(uint8_t* vehicle, void* edx) {
    const float* t = ((const float*(__fastcall*)(uint8_t*, void*))kGetTunings)(vehicle, edx);
    if (!t || g_cfg.gameTuning || *(int*)(vehicle + kIV_DriverClass) != 0) return t;
    static const float kStock[16] = {};
    static bool logged;
    if (!logged) {
        logged = true;
        Log("traffic: the game's performance tuning ignored for the player's car (stock values)");
    }
    return kStock;
}

// ProcessStartRace calls GRaceStatus::StartRace (602010), which resets the master
// timer and mBonusTime through 5FFD80. Hook this call only: no frame polling,
// checkpoint awards, or stale race-pointer latch. A restart runs the reset again.
static void(__fastcall* s_startRace)(void*, void*) = (void(__fastcall*)(void*, void*))0x602010;
static int(__fastcall* s_raceType)(void*, void*) = (int(__fastcall*)(void*, void*))kGetRaceType;

static void ApplyTollboothBonus(uint8_t* race, int type) {
    if (type == 4 && *(int*)(race + 0x1964) != 1) // never online
        *(float*)(race + 0x1980) += g_cfg.tollboothTimeBonus;
}

static void __fastcall RaceStart(uint8_t* race, void* edx) {
    s_startRace(race, edx);
    void* parms = *(void**)(race + 0x1968);
    if (parms) ApplyTollboothBonus(race, s_raceType(parms, nullptr));
}


// Restore only the racer PID target adjustment, not the old catch-up hooks.
// Temporarily scale mDriveSpeed; leave the path planner's stored target unchanged.
static void(__fastcall* s_racerGasBrake)(void*, void*, float) =
    (void(__fastcall*)(void*, void*, float))0x42A0B0;

static void RacerGasBrakeTarget(uint8_t* ai, void* edx, float dt, bool player, int mode) {
    float& target = *(float*)(ai + 0x84);
    const float original = target;
    if (!player && mode == 2 && target > 0.0f)
        target *= Clamp(g_cfg.opponentSpeed, 0.25f, 2.0f);
    s_racerGasBrake(ai, edx, dt);
    target = original;
}

static void __fastcall RacerGasBrake(uint8_t* ai, void* edx, float dt) {
    int mode = RaceMode();
    if (mode != 2 || g_cfg.opponentSpeed == 1.0f) {
        s_racerGasBrake(ai, edx, dt);
        return;
    }
    void* iv = ai + 0x4C;
    uintptr_t* vt = *(uintptr_t**)iv;
    void* vehicle = ((void*(__fastcall*)(void*, void*))vt[2])(iv, nullptr);
    if (!vehicle) { s_racerGasBrake(ai, edx, dt); return; }
    bool player = *(int*)((uint8_t*)vehicle + kIV_DriverClass) == 0;
    RacerGasBrakeTarget(ai, edx, dt, player, mode);
}

void Traffic_Install() {
    // Validate both racer slots before changing either. Police/traffic tables are untouched.
    if (ExpectPtr(0x892760, 0x42A0B0) && ExpectPtr(0x892B10, 0x42A0B0)) {
        SetSlot(0x892760, 0x42A0B0, (void*)&RacerGasBrake);
        SetSlot(0x892B10, 0x42A0B0, (void*)&RacerGasBrake);
        Log("races: Career opponent speed hooks installed (100%% stock; other modes unchanged)");
    } else Log("races: Career opponent speed unavailable (unexpected racer controller slots)");
    if (!RetargetCall(0x60DBF9, 0x602010, (void*)&RaceStart))
        Log("races: tollbooth starting bonus unavailable (unexpected race-start call)");
    if (!SetSlot(kSlotGetTunings, kGetTunings, (void*)&HkGetTunings))
        Log("traffic: unexpected vehicle code, the game's performance tuning left as it is");
    if (!RetargetCall(kCallTurboRaceType, kGetRaceType, (void*)&HkTurboRaceType))
        Log("traffic: unexpected turbo dial code, its needle keeps the drag range in drag races");
    bool hudOk = true;
    for (uintptr_t slot : kSlotsSetHud) hudOk = hudOk && *(uintptr_t*)slot == kSetHud;
    if (hudOk) {
        for (uintptr_t slot : kSlotsSetHud) SetSlot(slot, kSetHud, (void*)&HkSetHud);
        Log("traffic: drag races with the %s HUD", g_cfg.normalDragHud ? "normal race" : "drag");
    } else {
        Log("traffic: unexpected player code, NormalDragHud unavailable");
    }
    if (SetSlot(kSlotSetDriverStyle, kSetDriverStyle, (void*)&HkSetDriverStyle)) {
        Log("traffic: drag races %s", g_cfg.normalDragRaces ? "with normal driving for the player" : "as the game's");
    } else {
        Log("traffic: unexpected vehicle code, NormalDragRaces unavailable");
    }
    Cops_Install();
}
