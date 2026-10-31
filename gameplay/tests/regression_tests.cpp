// Exercise production input, settings migration and smoke selection without the game.
#include "../src/devices.cpp"
#include "../src/input.cpp"
#include "../src/vfx.cpp"
#include "../src/traffic.cpp"
#include "../src/ini_migration.h"
#include "../src/impact_damage.h"
#include "../src/controls.h"
#include "../src/telemetry_inputs.h"
#include "../src/telemetry.h"
#include "../src/shift_direction.h"
#include <assert.h>
Config g_cfg;
volatile unsigned char gameAddressPadding[0x600000];
void Log(const char*, ...) {}
const char* GameDir() { return ""; }
double Now() { return 1; }
HudStatus Game_Hud() { return {}; }
float Physics_EngineSmokeBack() { return 0.0f; }
const char* const kButtonKeys[6] = {"Handbrake", "Nitrous", "Speedbreaker", "ResetCar", "Camera", "LookBack"};

static bool player;
static float observedRacerTarget;
static uint8_t testRaceVehicle[0xA0];
static void* __fastcall TestRaceVehicle(void*, void*) { return testRaceVehicle; }
static void __fastcall ObserveRacerTarget(void* ai, void*, float dt) {
    assert(fabsf(dt - 0.02f) < 0.000001f);
    observedRacerTarget = *(float*)((uint8_t*)ai + 0x84);
}
static void __fastcall ResetRaceTimer(void* race, void*) {
    *(float*)((uint8_t*)race + 0x1980) = 0.0f;
}
static int __fastcall TollboothType(void*, void*) { return 4; }
static float stockDensity;
static float __cdecl StockDensity() { return stockDensity; }
static void TestRaceControls() {
    for (float density : {0.0f, 0.1f, 0.3f, 0.5f, 1.0f}) {
        assert(ScaleTrafficDensity(density, 1) == density);
        assert(ScaleTrafficDensity(density, 0) == 0);
        assert(ScaleTrafficDensity(density, 2) == (density * 2 > 1 ? 1 : density * 2));
    }
    assert(ScaleTrafficDensity(0.1f, 10) == 1);
    assert(fabsf(ScaleTrafficDensity(0.3f, 0.5f) - 0.15f) < 0.000001f);
    assert(ScaleTrafficDensity(-10, 2) == 0);
    assert(ScaleTrafficDensity(0.5f, NAN) == 0.5f);
    assert(ScaleTrafficDensity(50, -1) == 0);
    auto densityGetter = s_density;
    s_density = &StockDensity;
    stockDensity = 0.3f;
    for (float multiplier : {2.0f, 2.0f, 0.5f, 0.0f, 1.0f}) {
        g_cfg.trafficMultiplier = multiplier;
        assert(fabsf(TrafficDensity() - ScaleTrafficDensity(0.3f, multiplier)) < 0.000001f);
        assert(fabsf(stockDensity - 0.3f) < 0.000001f); // live edits never compound or overwrite parameters
    }
    s_density = densityGetter;
    uint8_t race[0x19A0] = {};
    *(void**)(race + 0x1968) = race; // fake parameters, read by the stub only
    auto start = s_startRace;
    auto type = s_raceType;
    s_startRace = &ResetRaceTimer;
    s_raceType = &TollboothType;
    g_cfg.tollboothTimeBonus = 30.0f;
    RaceStart(race, nullptr);
    assert(*(float*)(race + 0x1980) == 30.0f);
    RaceStart(race, nullptr); // restart resets first: never 60
    assert(*(float*)(race + 0x1980) == 30.0f);
    ResetRaceTimer(race, nullptr);
    for (int other : {0, 1, 2, 3, 5, 6, 7, 8}) ApplyTollboothBonus(race, other);
    assert(*(float*)(race + 0x1980) == 0.0f);
    *(int*)(race + 0x1964) = 1;
    RaceStart(race, nullptr); // online excluded
    assert(*(float*)(race + 0x1980) == 0.0f);
    s_startRace = start;
    s_raceType = type;
    uint8_t ai[0x90] = {};
    uintptr_t aiVtable[3] = {0, 0, (uintptr_t)&TestRaceVehicle};
    *(uintptr_t**)(ai + 0x4C) = aiVtable;
    *(float*)(ai + 0x84) = 40.0f;
    auto gas = s_racerGasBrake;
    s_racerGasBrake = &ObserveRacerTarget;
    for (float factor : {0.25f, 0.5f, 1.0f, 2.0f}) {
        g_cfg.opponentSpeed = factor;
        for (int i = 0; i < 3; i++) {
            RacerGasBrakeTarget(ai, nullptr, 0.02f, false);
            assert(observedRacerTarget == 40.0f * factor);
            assert(*(float*)(ai + 0x84) == 40.0f);
        }
        RacerGasBrakeTarget(ai, nullptr, 0.02f, true);
        assert(observedRacerTarget == 40.0f); // player unaffected
        *(int*)(testRaceVehicle + kIV_DriverClass) = 0;
        RacerGasBrake(ai, nullptr, 0.02f);
        assert(observedRacerTarget == 40.0f);
        *(int*)(testRaceVehicle + kIV_DriverClass) = 1;
        RacerGasBrake(ai, nullptr, 0.02f);
        assert(observedRacerTarget == 40.0f * factor);
    }
    *(float*)(ai + 0x84) = -5.0f;
    RacerGasBrakeTarget(ai, nullptr, 0.02f, false);
    assert(observedRacerTarget == -5.0f); // reverse/stuck recovery unchanged
    s_racerGasBrake = gas;
    g_cfg = Config{};
}
static bool __attribute__((thiscall)) IsPlayer(void*) { return player; }
static void TestBindings() {
    s_joy[0].dev = (IDirectInputDevice8A*)1;
    s_joy[0].ok = true;
    std::vector<Binding> bindings;
    for (bool inverse : {false, true}) {
        assert(ParseBindings(inverse ? "joy0:-y" : "joy0:y", bindings, nullptr));
        for (LONG raw : {0L, 32767L, 65535L}) {
            s_joy[0].state.axis[1] = raw;
            assert(Down(bindings[0], true) == (inverse ? raw == 0 : raw == 65535));
        }
        s_joy[0].ok = false;
        assert(!Down(bindings[0], true));
        s_joy[0].ok = true;
    }
    assert(ParseBindings("joy0:b3", bindings, nullptr));
    s_joy[0].state.btn[3] = 0x80;
    assert(Down(bindings[0], true));
    s_joy[0].state.btn[3] = 0;
    assert(!Down(bindings[0], true));
}

static void TestSteeringWheelAngle() {
    s_joy[0].dev = (IDirectInputDevice8A*)1;
    s_joy[0].ok = true;
    assert(ParseBindings("joy0:x", s_steer, nullptr));
    g_cfg.wheelRotation = 900.0f;
    g_cfg.steeringLock = 540.0f;
    g_cfg.steerSensitivity = 2.0f;
    g_cfg.steerDeadzone = 0.0f;
    g_cfg.steerLinearity = 1.0f;

    float steer, wheelDeg;
    s_joy[0].state.axis[0] = 0;
    assert(ReadSteer(&steer, &wheelDeg) && fabsf(wheelDeg + 450.0f) < 0.001f);
    s_joy[0].state.axis[0] = 32767;
    assert(ReadSteer(&steer, &wheelDeg) && fabsf(wheelDeg) < 0.01f);
    s_joy[0].state.axis[0] = 65535;
    assert(ReadSteer(&steer, &wheelDeg) && fabsf(wheelDeg - 450.0f) < 0.001f);
    s_joy[0].state.axis[0] = 70000;
    assert(ReadSteer(&steer, &wheelDeg) && fabsf(wheelDeg - 450.0f) < 0.001f);

    g_cfg.wheelRotation = 1080.0f;
    s_joy[0].state.axis[0] = 65535;
    assert(ReadSteer(&steer, &wheelDeg) && fabsf(wheelDeg - 540.0f) < 0.001f);
    s_steer.clear();
    s_joy[0].ok = false;
    s_joy[0].dev = nullptr;
}

static void TestSmoke() {
    assert((uintptr_t)gameAddressPadding < kIsViewAnchor &&
           (uintptr_t)gameAddressPadding + sizeof(gameAddressPadding) > kIsViewAnchor + 5);
    DWORD old;
    assert(VirtualProtect((void*)kIsViewAnchor, 5, PAGE_EXECUTE_READWRITE, &old));
    *(uint8_t*)kIsViewAnchor = 0xE9;
    *(int32_t*)(kIsViewAnchor + 1) = (int32_t)((uintptr_t)&IsPlayer - kIsViewAnchor - 5);
    FlushInstructionCache(GetCurrentProcess(), (void*)kIsViewAnchor, 5);
    unsigned char packet[0x94] = {};
    *(float*)(packet + kPkt_Health) = 0.1f;
    player = true;
    s_copKey = 77; // Avoid accessing the game's attribute database.
    assert(Vfx_DamageKey(nullptr, packet, 1234, 0) == 1234 && s_playerSmoke && s_smokeHealth == 0);
    player = false;
    *(float*)(packet + kPkt_Health) = 0.4f;
    assert(Vfx_DamageKey(nullptr, packet, 2345, 0) == 2345 && !s_playerSmoke && fabsf(s_smokeHealth - 0.4f) < 0.000001f);
    player = true;
    g_cfg.damageSmoke = false;
    s_playerSmoke = true;
    assert(Vfx_DamageKey(nullptr, packet, 1234, 0) == 1234 && !s_playerSmoke);
    g_cfg.damageSmoke = true;
    *(float*)(packet + kPkt_Health) = 1;
    assert(Vfx_DamageKey(nullptr, packet, 1234, 0) == 0 && !s_playerSmoke);
}

static void TestMigration() {
    char defaults[MAX_PATH], root[MAX_PATH];
    GetFullPathNameA("nfsmw_gameplay.ini", MAX_PATH, defaults, nullptr);
    uint64_t fingerprint = IniFingerprint(defaults);
    if (fingerprint != kGameplayDefaultIniFingerprint)
        fprintf(stderr, "Update kGameplayDefaultIniFingerprint for shipped defaults: 0x%llx\n", (unsigned long long)fingerprint);
    assert(fingerprint == kGameplayDefaultIniFingerprint);
    char relative[128];
    snprintf(relative, sizeof(relative), "build\\regression-settings-%lu-%lu", GetCurrentProcessId(), GetTickCount());
    GetFullPathNameA(relative, MAX_PATH, root, nullptr);
    assert(CreateDirectoryA(root, nullptr));
    std::string oldIni = std::string(root) + "\\nfsmw_hshifter.ini";
    std::string current = std::string(root) + "\\nfsmw_gameplay.ini";
    std::string backup = current + ".pre-hshifter.bak";
    auto legacy = [&] {
        assert(WritePrivateProfileStringA("Clutch", "Clutch", "joy0:-y", oldIni.c_str()));
        assert(WritePrivateProfileStringA("Shifter", "Gear1", "joy0:b3", oldIni.c_str()));
    };
    auto exists = [](const std::string& p) { return GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES; };
    auto clear = [&] {
        for (const auto& p : {oldIni, current, backup}) {
            WritePrivateProfileStringA(nullptr, nullptr, nullptr, p.c_str());
            if (exists(p)) assert(DeleteFileA(p.c_str()));
        }
    };
    legacy();
    assert(MigrateLegacyIni(oldIni.c_str(), current.c_str()));
    assert(!exists(oldIni) && exists(current) && !exists(backup));
    char value[64];
    GetPrivateProfileStringA("Clutch", "Clutch", "", value, sizeof(value), current.c_str());
    assert(strcmp(value, "joy0:-y") == 0);
    clear(); legacy();
    assert(CopyFileA(defaults, current.c_str(), TRUE));
    assert(MigrateLegacyIni(oldIni.c_str(), current.c_str()) && exists(backup) && !exists(oldIni));
    assert(IniFingerprint(backup.c_str()) == kGameplayDefaultIniFingerprint);
    GetPrivateProfileStringA("Clutch", "Clutch", "", value, sizeof(value), current.c_str());
    assert(strcmp(value, "joy0:-y") == 0);
    clear(); legacy();
    assert(CopyFileA(defaults, current.c_str(), TRUE));
    assert(WritePrivateProfileStringA("Clutch", "Strength", "1.1", current.c_str()));
    uint64_t customised = IniFingerprint(current.c_str());
    assert(!MigrateLegacyIni(oldIni.c_str(), current.c_str()));
    assert(exists(oldIni) && !exists(backup) && IniFingerprint(current.c_str()) == customised);
    clear(); legacy();
    assert(CopyFileA(defaults, current.c_str(), TRUE));
    assert(WritePrivateProfileStringA("Car.mycar", "Grip", "1.2", current.c_str()));
    assert(!MigrateLegacyIni(oldIni.c_str(), current.c_str()) && exists(oldIni));
    clear(); legacy();
    assert(CopyFileA(defaults, current.c_str(), TRUE));
    assert(CopyFileA(defaults, backup.c_str(), TRUE));
    assert(!MigrateLegacyIni(oldIni.c_str(), current.c_str()) && exists(oldIni));
    assert(IniFingerprint(current.c_str()) == kGameplayDefaultIniFingerprint);
    clear();
    assert(RemoveDirectoryA(root));
}
// Speed-sensitive wheel steering: full lock parked, falling smoothly and steadily with speed,
// close to the game's own values at the default, never below 1 - sensitivity, none when off.
static void TestSteeringLock() {
    assert(SteeringLockShare(0, 0.85f) == 1.0f);
    float prev = 1.0f;
    for (float kmh = 1; kmh < 400; kmh += 1) {
        float s = SteeringLockShare(kmh, 0.85f);
        assert(s < prev && s > 0.15f);
        assert(prev - s < 0.02f);                       // no kinks or jumps
        prev = s;
    }
    float at64 = 45 * SteeringLockShare(64, 0.85f), at130 = 45 * SteeringLockShare(130, 0.85f);
    assert(at64 > 16 && at64 < 23 && at130 > 10 && at130 < 15);
    assert(SteeringLockShare(200, 0.0f) == 1.0f);
    assert(SteeringLockShare(200, 0.5f) > SteeringLockShare(200, 0.85f));
}

#include "../src/low_speed_tyres.h"

static void TestLowSpeedTyres() {
    using namespace low_speed_tyres;
    assert(Share(0.0f) == 0.0f && Share(2.0f) == 0.0f);
    assert(Share(5.0f) == 1.0f && Share(100.0f) == 1.0f);
    assert(Share(3.5f) == 0.5f);
    float previous = 0.0f;
    for (int i = 0; i <= 600; i++) {
        float speed = i * 0.01f, share = Share(speed);
        assert(share >= previous && share <= 1.0f);
        assert(share - previous < 0.006f);
        assert(Share(-speed) == share); // reversing uses the same transition
        previous = share;
    }
    // Preserve the stock signed contact force at rest; retain custom force at speed.
    assert(Blend(-120.0f, 900.0f, 0.0f) == -120.0f);
    assert(Blend(-120.0f, 900.0f, 5.0f) == 900.0f);
    assert(Blend(-120.0f, 900.0f, 3.5f) == 390.0f);
    assert(Blend(1.0f, 0.0f, 0.0f) == 1.0f); // no added circle cut at rest
    assert(Blend(1.0f, 0.0f, 3.5f) == 0.5f);
    // Launch still has stock traction loss, not an artificial full-traction tie.
    assert(fabsf(WheelTie(0.8f, 1.0f, 0.0f) - 0.8f) < 1e-6f);
    assert(fabsf(WheelTie(0.8f, 1.0f, 2.0f) - 0.8f) < 1e-6f);
    assert(fabsf(WheelTie(0.8f, 1.0f, 5.0f) - powf(0.8f, 16.0f)) < 1e-6f);
    assert(WheelTie(0.8f, 1.0f, 3.5f) < 0.8f);
    assert(WheelTie(0.8f, 1.0f, 3.5f) > WheelTie(0.8f, 1.0f, 5.0f));
    assert(fabsf(WheelTie(0.8f, 0.0f, 20.0f) - 0.8f) < 1e-6f);
    assert(WheelTie(0.0f, 1.0f, 20.0f) == 0.0f);
    assert(WheelTie(1.0f, 1.0f, 20.0f) == 1.0f);
}

static void TestTelemetryInputs() {
    InputSnapshot in;
    auto out = ReadTelemetryInputs(in, false);
    assert(!out.steering && out.degrees == 0.0f);
    for (int i = 0; i < 4; i++) assert(!out.bound[i] && out.pedal[i] == 0.0f);
    in.steerBound = in.clutchBound = true;
    in.wheelDeg = -450.0f;
    in.pedalBound[0] = in.pedalBound[1] = true;
    in.pedal[0] = 0.25f; in.pedal[1] = 1.5f; in.clutchPedal = 0.75f;
    in.buttonBound = 1u << BTN_HANDBRAKE; in.handbrake = 0.5f;
    out = ReadTelemetryInputs(in, false);
    assert(out.steering && out.degrees == -450.0f);
    for (int i = 0; i < 4; i++) assert(out.bound[i]);
    assert(out.pedal[0] == 0.25f && out.pedal[1] == 1.0f);
    assert(out.pedal[2] == 0.75f && out.pedal[3] == 0.5f);
    in.wheelDeg = 450.0f; in.pedal[0] = -1.0f;
    out = ReadTelemetryInputs(in, false);
    assert(out.degrees == 450.0f && out.pedal[0] == 0.0f);
    out = ReadTelemetryInputs(in, true);
    assert(!out.steering && !out.bound[0] && !out.bound[1] && !out.bound[3]);
    assert(out.bound[2] && out.pedal[2] == 0.75f);
    in.wheelDeg = NAN; in.handbrake = NAN;
    out = ReadTelemetryInputs(in, false);
    assert(!out.steering && !out.bound[3] && out.pedal[3] == 0.0f);
}

int main() {
    for (bool wall : {false, true}) {
        assert(ImpactDamage(7.9f, 8.0f, 1.0f, wall) == 0.0f);
        assert(ImpactDamage(8.0f, 8.0f, 1.0f, wall) == 0.0f);
        assert(ImpactDamage(20.0f, 8.0f, 0.0f, wall) == 0.0f);
        assert(ImpactDamage(20.0f, 25.0f, 1.0f, wall) == 0.0f);
    }
    assert(fabsf(ImpactDamage(20, 8, 1, false) - 0.12f) < 0.000001f);
    assert(fabsf(ImpactDamage(20, 8, 1, true) - 0.138f) < 0.000001f);
    assert(fabsf(ImpactDamage(20, 8, 2, true) - 0.276f) < 0.000001f);
    assert(!CarTotaled(false, false));
    assert(CarTotaled(false, true));
    assert(CarTotaled(true, false));
    assert(CarTotaled(true, true));
    assert(!PhysicsLogEnabled(0, false));
    assert(!PhysicsLogEnabled(0, true));
    assert(!PhysicsLogEnabled(1, false));
    assert(PhysicsLogEnabled(1, true));
    TestLowSpeedTyres();
    TestTelemetryInputs();
    TestSteeringLock();
    static_assert(sizeof(void*) == 4, "test the game layout");
    TestBindings(); TestSteeringWheelAngle(); TestSmoke(); TestMigration();
    assert(FirstGearRollbackAllowed(2, 0.0f));
    assert(FirstGearRollbackAllowed(2, 11.9f / 3.6f));
    assert(FirstGearRollbackAllowed(2, 12.0f / 3.6f));
    assert(FirstGearRollbackAllowed(2, -12.0f / 3.6f));
    assert(!FirstGearRollbackAllowed(2, 12.01f / 3.6f));
    assert(!FirstGearRollbackAllowed(2, -12.01f / 3.6f));
    assert(!FirstGearRollbackAllowed(0, 1.0f));
    assert(!FirstGearRollbackAllowed(3, 1.0f));
    g_cfg.disableRubberband = true;
    assert(RacerCatchup(nullptr, nullptr) == 0.0f); // must not dereference the racer when disabled
    g_cfg.disableRubberband = false;
    TestRaceControls();
    puts("gameplay regression tests passed");
}
