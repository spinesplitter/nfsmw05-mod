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

// Extra Options owns Quick Race presets. These separate runtime overrides never patch ExOpts.
namespace {

int RaceTrafficValue(int original, int mode, int type = -1) {
    if (mode == 1 || type == 2 || g_cfg.raceTrafficFloor < 0) return original;
    return std::max(original, (int)lroundf(Clamp(g_cfg.raceTrafficFloor, 0.0f, 100.0f)));
}

int RaceMode() {
    auto race = *(uint8_t**)0x91E000;
    return race ? *(int*)(race + 0x1964) : 1; // no race: do not override
}

static int(__fastcall* s_density)(void*, void*) = nullptr;
// AISpawnManager's update (439BB0) passes normalized density through a bounded
// curve at 439BF0. Keep density in its native range and scale the curve result.
static float(__cdecl* s_liveDensity)() = (float(__cdecl*)())0x41EC20;

static int(__fastcall* s_trafficRaceType)(void*, void*) = (int(__fastcall*)(void*, void*))0x5FAA20;

int TrafficRaceType(uint8_t* race) {
    if (!race || *(int*)(race + 0x1960) != 1) return -1;
    auto parms = *(void**)(race + 0x1968);
    return parms ? s_trafficRaceType(parms, nullptr) : 2; // unknown active race: fail closed
}

bool TrafficEligible(int context, int playMode, int type) {
    return context >= 0 && context <= 3 && context != 1 &&
        (playMode == 0 || (playMode == 1 && type >= 0 && type != 2));
}

float CareerTrafficValue(float original, int context, int playMode, int type = 0) {
    if (!TrafficEligible(context, playMode, type)) return original;
    float multiplier = Clamp(g_cfg.careerTrafficMultiplier, 1.0f, 10.0f);
    if (multiplier == 1.0f) return original; // exact stock bypass, including free roam
    if (playMode == 0 && g_cfg.raceTrafficFloor >= 0.0f)
        original = std::max(original, Clamp(g_cfg.raceTrafficFloor, 0.0f, 100.0f) * 0.01f);
    // The spawn-rate curve saturates at its last entry. Scale its output instead.
    return original;
}

float CareerSpawnRateValue(float original, int context, int playMode, int type = 0) {
    if (!TrafficEligible(context, playMode, type)) return original;
    return original > 0.0f ? original * Clamp(g_cfg.careerTrafficMultiplier, 1.0f, 10.0f) : original;
}

static float(__fastcall* s_spawnRate)(void*, void*, float) =
    (float(__fastcall*)(void*, void*, float))0x6533B0;

// 426230 reads this 24-byte trafficpattern record: rate at +0x0c,
// maximum instances at +0x10, percentage-based limit at +0x14.
// Return a private copy: never edit the shared attribute database or array bounds.
struct TrafficPatternEntry {
    uint32_t header[3];
    float rate;
    uint32_t maximum;
    uint32_t percent;
};
static_assert(sizeof(TrafficPatternEntry) == 24, "trafficpattern record size");

uint32_t CareerTrafficLimit(uint32_t original, int context, int playMode, int type = 0) {
    if (!original || !TrafficEligible(context, playMode, type)) return original;
    double scaled = ceil((double)original * Clamp(g_cfg.careerTrafficMultiplier, 1.0f, 10.0f));
    return scaled >= 4294967295.0 ? UINT32_MAX : (uint32_t)scaled;
}

static TrafficPatternEntry*(__fastcall* s_patternEntry)(void*, void*, uint32_t, uint32_t) =
    (TrafficPatternEntry*(__fastcall*)(void*, void*, uint32_t, uint32_t))0x454810;

TrafficPatternEntry* __fastcall CareerPatternEntry(void* pattern, void* edx, uint32_t key, uint32_t index) {
    auto entry = s_patternEntry(pattern, edx, key, index);
    auto race = *(uint8_t**)0x91E000;
    if (!entry || !race) return entry;
    int context = *(int*)(race + 0x1964), playMode = *(int*)(race + 0x1960);
    static thread_local TrafficPatternEntry copy;
    copy = *entry;
    int type = TrafficRaceType(race);
    copy.maximum = CareerTrafficLimit(entry->maximum, context, playMode, type);
    copy.percent = CareerTrafficLimit(entry->percent, context, playMode, type);
    return &copy;
}

float __fastcall CareerSpawnRate(void* curve, void* edx, float density) {
    float original = s_spawnRate(curve, edx, density);
    auto race = *(uint8_t**)0x91E000;
    if (!race) return original;
    int context = *(int*)(race + 0x1964), playMode = *(int*)(race + 0x1960);
    float scaled = CareerSpawnRateValue(original, context, playMode, TrafficRaceType(race));
    static int lastContext = -1, lastPlayMode = -1;
    if (context != lastContext || playMode != lastPlayMode) {
        Log("traffic: context %d play %d density %.3f spawn rate %.3f -> %.3f (pool capacities unchanged)",
            context, playMode, density, original, scaled);
        lastContext = context;
        lastPlayMode = playMode;
    }
    return scaled;
}

float __cdecl CareerLiveDensity() {
    float original = s_liveDensity();
    auto race = *(uint8_t**)0x91E000;
    if (!race) return original;
    return CareerTrafficValue(original, *(int*)(race + 0x1964), *(int*)(race + 0x1960), TrafficRaceType(race));
}

int __fastcall RaceDensity(void* parms, void* edx) {
    return RaceTrafficValue(s_density(parms, edx), RaceMode(), s_trafficRaceType(parms, nullptr));
}

// Observe the three direct calls in 439A10 without repeating them or bypassing checks.
struct TrafficSpawnStages {
    unsigned roads = 0, roadOK = 0, selections = 0, selectionOK = 0;
    unsigned vehicles = 0, vehicleOK = 0, creationAllowed = 0;
};
static TrafficSpawnStages s_spawnStages;
static bool s_spawnStagesInstalled = false;
static bool(__fastcall* s_spawnRoad)(void*, void*, void*) =
    (bool(__fastcall*)(void*, void*, void*))0x437DC0;
static uint32_t(__fastcall* s_spawnSelection)(void*, void*) =
    (uint32_t(__fastcall*)(void*, void*))0x426230;
static void*(__fastcall* s_spawnVehicle)(void*, void*, uint32_t, bool) =
    (void*(__fastcall*)(void*, void*, uint32_t, bool))0x426380;

bool __fastcall TrafficSpawnRoad(void* manager, void* edx, void* position) {
    ++s_spawnStages.roads;
    bool result = s_spawnRoad(manager, edx, position);
    if (result) ++s_spawnStages.roadOK;
    return result;
}

uint32_t __fastcall TrafficSpawnSelection(void* manager, void* edx) {
    ++s_spawnStages.selections;
    uint32_t result = s_spawnSelection(manager, edx);
    if (result) ++s_spawnStages.selectionOK;
    return result;
}

void* __fastcall TrafficSpawnVehicle(void* manager, void* edx, uint32_t key, bool allowCreate) {
    ++s_spawnStages.vehicles;
    if (allowCreate) ++s_spawnStages.creationAllowed;
    void* result = s_spawnVehicle(manager, edx, key, allowCreate);
    if (result) ++s_spawnStages.vehicleOK;
    return result;
}

// Observe the existing attempt; never bypass the game's capacity/road/visibility checks.
static bool(__fastcall* s_spawnAttempt)(void*, void*) = (bool(__fastcall*)(void*, void*))0x439A10;
bool __fastcall TrafficSpawnAttempt(void* manager, void* edx) {
    bool result = s_spawnAttempt(manager, edx);
    static unsigned attempts = 0, successes = 0;
    static double nextLog = 0;
    ++attempts;
    if (result) ++successes;
    double now = Now();
    if (now >= nextLog) {
        auto race = *(uint8_t**)0x91E000;
        Log("traffic: attempts %u successes %u in sample; context %d play %d type %d multiplier %.1fx",
            attempts, successes, race ? *(int*)(race + 0x1964) : -1,
            race ? *(int*)(race + 0x1960) : -1, TrafficRaceType(race), g_cfg.careerTrafficMultiplier);
        if (s_spawnStagesInstalled)
            Log("traffic: stages road %u/%u type %u/%u vehicle %u/%u creation-allowed %u; early-gate %u post-vehicle %u",
                s_spawnStages.roadOK, s_spawnStages.roads,
                s_spawnStages.selectionOK, s_spawnStages.selections,
                s_spawnStages.vehicleOK, s_spawnStages.vehicles, s_spawnStages.creationAllowed,
                attempts - s_spawnStages.roads, s_spawnStages.vehicleOK - successes);
        s_spawnStages = {};
        attempts = successes = 0;
        nextLog = now + 15.0;
    }
    return result;
}

// Independent runtime roster view. The optional More opponents VLT script was
// inspected only to establish that extra slots reuse event character profiles.
// Never read an added index from the original Opponents attribute array.
constexpr int kCareerOpponents = 11;
static int(__fastcall* s_rosterCount)(void*, void*) = (int(__fastcall*)(void*, void*))0x5FACD0;
static void*(__fastcall* s_rosterEntry)(void*, void*, int) = (void*(__fastcall*)(void*, void*, int))0x5FD2D0;
static int(__fastcall* s_rosterRaceType)(void*, void*) = (int(__fastcall*)(void*, void*))0x5FAA20;
// GRaceParameters BossRace getter: VLT hash 0xFF5EE5D6, cached flag bit 5.
static bool(__fastcall* s_rosterBossRace)(void*, void*) = (bool(__fastcall*)(void*, void*))0x5FBD90;
// EventID getter returns the VLT hash of the event's ID string.
static uint32_t(__fastcall* s_rosterEventId)(void*, void*) = (uint32_t(__fastcall*)(void*, void*))0x5FAB40;

bool CareerIntroEvent(uint32_t eventId) {
    // Opening story events from GLOBAL/gameplay.bin. Do not exclude
    // 16_1_1_r_circuit: that reversed circuit belongs to a later Career bin.
    switch (eventId) {
    case 0x42938CE2: // 16_1_0_partial_dday
    case 0x9F60A40E: // 16_1_1_circuit
    case 0xF7E4440A: // 16_2_1_d_day_intro
    case 0xD348F2CD: // 16_2_2_sprint
    case 0x6DDD8383: // 16_2_3_sprint
        return true;
    default:
        return false;
    }
}

bool CareerRosterEligible(int context, int type, int count, int racers, bool current, bool boss,
        uint32_t eventId) {
    // The spawn loop adds 1 - existing racers to the opponent count. Require
    // exactly the local player already registered; never affect split/online.
    return current && !boss && !CareerIntroEvent(eventId) && context == 2 &&
        (type == 0 || type == 1 || type == 5) &&
        count > 0 && count <= 15 && racers == 1;
}

int CareerRosterIndex(int index, int count) {
    if (index < 0 || index >= kCareerOpponents || count <= 0) return -1;
    return index % count;
}

bool UseCareerRoster(void* parms, int count) {
    auto race = *(uint8_t**)0x91E000;
    if (!race || *(void**)(race + 0x1968) != parms || *(int*)(race + 0x1964) != 2)
        return false;
    return CareerRosterEligible(2, s_rosterRaceType(parms, nullptr), count,
        *(int*)(race + 0x1958), true, s_rosterBossRace(parms, nullptr),
        s_rosterEventId(parms, nullptr));
}

int __fastcall CareerSpawnCount(void* parms, void* edx) {
    int count = s_rosterCount(parms, edx);
    if (!UseCareerRoster(parms, count)) return count;
    // Validate all profiles that the virtual roster will expose before raising
    // the count. Missing/modded event profiles fail closed to the stock roster.
    for (int i = 0; i < std::min(count, kCareerOpponents); ++i)
        if (!s_rosterEntry(parms, nullptr, i)) {
            Log("races: Career roster unchanged (missing profile %d)", i);
            return count;
        }
    Log("races: Career spawn roster %d -> 11 opponents (12 total), type %d",
        count, s_rosterRaceType(parms, nullptr));
    return kCareerOpponents;
}

void* __fastcall CareerSpawnEntry(void* parms, void* edx, int index) {
    int count = s_rosterCount(parms, nullptr);
    // During this loop racers are registered one at a time, so the existing
    // racer count no longer equals one. Scope by current parameters and type.
    auto race = *(uint8_t**)0x91E000;
    if (race && CareerRosterEligible(*(int*)(race + 0x1964),
            s_rosterRaceType(parms, nullptr), count, 1,
            *(void**)(race + 0x1968) == parms, s_rosterBossRace(parms, nullptr),
            s_rosterEventId(parms, nullptr))) {
        int mapped = CareerRosterIndex(index, count);
        if (mapped >= 0) index = mapped;
    }
    return s_rosterEntry(parms, edx, index);
}

void InstallCareerRoster() {
    // Both sites belong to the same opponent-creation loop. Check both before
    // writing either; do not stack on another mod's detour or change getters.
    if (!Expect(0x61DCB7, {0xE8,0x14,0xD0,0xFD,0xFF}) ||
        !Expect(0x61DCC8, {0xE8,0x03,0xF6,0xFD,0xFF}) ||
        !Expect(0x5FACD0, {0x56,0x8B,0xF1,0x8B,0x46,0x0C}) ||
        !Expect(0x5FD2D0, {0x56,0x8B,0xF1,0x8B,0x46,0x0C}) ||
        !Expect(0x5FAA20, {0x56,0x8B,0xF1,0x8B,0x46,0x04}) ||
        !Expect(0x5FBD90, {0x56,0x8B,0xF1,0x8B,0x46,0x04,0x85,0xC0,0x74,0x0A,
            0x8B,0x40,0x18,0xC1,0xE8,0x05,0x24,0x01}) ||
        !Expect(0x5FAB40, {0x56,0x8B,0xF1,0x8B,0x46,0x04,0x85,0xC0,0x74,0x05,
            0x8B,0x40,0x14,0x5E,0xC3}) ||
        !Expect(0x5FAB84, {0x68,0xEC,0x03,0x84,0xA7}) ||
        !Expect(0x5FAB9C, {0x8B,0x00,0x50,0xE8,0x2C,0x6E,0xE5,0xFF})) {
        Log("races: Career roster unavailable (incompatible spawn/getter code)");
        return;
    }
    WriteRel(0x61DCC8, 0xE8, (void*)&CareerSpawnEntry);
    WriteRel(0x61DCB7, 0xE8, (void*)&CareerSpawnCount);
    Log("races: 11-opponent Career Sprint/Circuit/Speedtrap spawn hooks installed (boss/intro races excluded)");
}

void InstallRaceOverrides() {
    InstallCareerRoster();
    // Validate the entire set before redirecting any stage. Each wrapper preserves
    // the original thiscall arguments, return value and number of invocations.
    if (Expect(0x439A39, {0xE8,0x82,0xE3,0xFF,0xFF}) &&
        Expect(0x439A44, {0xE8,0xE7,0xC7,0xFE,0xFF}) &&
        Expect(0x439A75, {0xE8,0x06,0xC9,0xFE,0xFF})) {
        WriteRel(0x439A39, 0xE8, (void*)&TrafficSpawnRoad);
        WriteRel(0x439A44, 0xE8, (void*)&TrafficSpawnSelection);
        WriteRel(0x439A75, 0xE8, (void*)&TrafficSpawnVehicle);
        s_spawnStagesInstalled = true;
        Log("traffic: road/type/vehicle rejection diagnostics installed; all spawn checks preserved");
    } else Log("traffic: stage diagnostics unavailable (unexpected spawn code)");
    if (!RetargetCall(0x439C6B, 0x439A10, (void*)&TrafficSpawnAttempt))
        Log("traffic: spawn diagnostics unavailable (unexpected attempt call)");
    if (Expect(0x426299, {0x52, 0x68, 0x95, 0xC7, 0xE3, 0x94, 0x89, 0x56, 0x60, 0xE8, 0x69, 0xE5, 0x02, 0x00}) &&
        Expect(0x42630B, {0x8B, 0x47, 0x10}) &&
        Expect(0x42631D, {0x8B, 0x47, 0x14})) {
        WriteRel(0x4262A2, 0xE8, (void*)&CareerPatternEntry);
        Log("traffic: offline per-type maximum and percentage limits scale 1..10x; drag excluded; array capacities unchanged");
    } else Log("traffic: per-type limit scaling unavailable (unexpected traffic selection code)");
    // Install the density floor and post-curve multiplier together, or neither.
    if (Expect(0x439BBD, {0xE8,0x5E,0x50,0xFE,0xFF}) &&
        Expect(0x439BF0, {0xE8,0xBB,0x97,0x21,0x00}) &&
        Expect(0x6533B0, {0x51,0xD9,0x44,0x24,0x08,0x56,0x8B,0xF1})) {
        WriteRel(0x439BBD, 0xE8, (void*)&CareerLiveDensity);
        WriteRel(0x439BF0, 0xE8, (void*)&CareerSpawnRate);
        Log("traffic: offline post-curve spawn-rate multiplier installed (1..10x); drag excluded; pool capacities unchanged");
    } else Log("traffic: Career spawn-rate multiplier unavailable (unexpected density/curve code)");
    // Six bytes, three complete position-independent instructions. Never hook an existing detour.
    if (Expect(0x5FABB0, {0x56, 0x8B, 0xF1, 0x8B, 0x46, 0x0C}))
        s_density = (decltype(s_density))Detour(0x5FABB0, 6, (void*)&RaceDensity);
    Log("races: density floor hook %s", s_density ? "installed" : "unavailable");
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
    InstallRaceOverrides();
    if (!RetargetCall(0x60DBF9, 0x602010, (void*)&RaceStart))
        Log("races: tollbooth starting bonus unavailable (unexpected race-start call)");
    // Traffic density, opponent counts and catch-up belong to bundled Extra Options.
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
