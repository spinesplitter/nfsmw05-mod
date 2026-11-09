// Cockpit: driver's seat and the driver model's head.
//
// Every car has a driver model (<CAR>_KIT00_DRIVER_A..D: body, arms and head in one draw entry,
// hands in another), normally only seen through the windows. In VR the eyes are put where its
// head is, so you sit in the driver's seat, and the head is left out so it isn't in your face.
//
// eViewPlatInterface::Render submits a model one draw entry (a range of the solid's index buffer)
// at a time. The submit call is hooked: the first time a driver solid comes through, each entry's
// triangles are reordered so the head (high up, at the back of the model) comes last, in the
// game's copy of the indices and in the index buffer. Instances whose head is near the eyes then
// get a copy of the entry that stops before the head; other cars keep their whole driver.
//
// The seat: the head of the driver nearest the game camera is measured each frame, in the space of
// the car body it is drawn with. In the bumper and hood cameras (camera close to the head) the
// eyes are put there, plus a per-car offset adjusted with the seat keys and saved to [Seat] in
// nfsmw_vr_seat.ini, and they take the car body's orientation, not the game camera's. The bumper and
// hood cameras lag and shake against the car, so a seat that followed the game camera (as it did
// at first, from the previous frame's measurement) wobbled inside the car. The body matrix is
// rebuilt before the eyes render the same way CarRenderConn::OnRender builds it, so the eyes
// stay fixed to the car model in the same frame.
#include "cockpit.h"
#include "frametrace.h"
#include "driver_pose_selection.h"
#include "driver_seam_cache.h"
#include "mirror_glass.h"
#include "d3d9hook.h"
#include <d3d9.h>
#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <utility>
#include <string.h>
#include <stdio.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <deque>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "cockpit_controls.h"
#include "driver_ik.h"
#include "driver_hover.h"
#include "driver_steering.h"
#include "driver_smoothing.h"

float g_steeringWheelAngle = 0.0f;
float g_seatInput[4] = {};
float g_cockpitHandbrake = 0, g_cockpitShiftUp = 0, g_cockpitShiftDown = 0;
float GameRainIntensity();

namespace {

// --- speed.exe v1.3 -------------------------------------------------------------------------
constexpr uintptr_t kCallSubmitEntry = 0x6DAD17;    // eViewPlatInterface::Render: call sub_6E0AB0(entry, ...)
constexpr uintptr_t kSubmitEntry = 0x6E0AB0;        // cdecl, queues one draw entry
constexpr uintptr_t kPlayerView = 0x919650;         // &eViews[EVIEW_PLAYER1]
constexpr int kViewCamera = 0x40;
constexpr int kCamPosition = 0x40;
constexpr uintptr_t kRvmViewPlat = 0x919730;         // eViews[EVIEW_PLAYER1_RVM]: its eViewPlatInfo*

// CarRenderConn (the car's render connection) and how OnRender (0x750E20) places the body.
constexpr uintptr_t kCarRenderConnVtbl = 0x8B5254;
constexpr int kVtblOnRender = 3;                    // OnRender(eView*, int reflection), thiscall
constexpr uintptr_t kOnRender = 0x750E20;
constexpr uintptr_t kFindConn = 0x744D40;           // VehicleRenderConn::Find(world id), cdecl
constexpr int kConnWorldId = 0x30;
constexpr int kConnBodyMatrix = 0x34;               // bMatrix4*: the body's matrix (in-car cameras)
constexpr int kConnCog = 0x50;                      // bVector4: centre of mass offset, body space
constexpr int kConnRenderMatrix = 0x330;            // bMatrix4: smoothed render matrix (other cameras)
constexpr int kViewMovers = 0x44;                   // eView: list of camera movers (node = mover + 4)
constexpr int kMoverGetAnchor = 0x10 / 4;           // CameraMover vtable: GetAnchor()
constexpr int kAnchorWorldId = 0x94;
constexpr int kAnchorPov = 0xC8;                    // int16: 1 = in-car (bumper / hood) camera

// eModel / eSolid / plat info / draw entry
constexpr int kModelSolid = 0x0C;
constexpr int kSolidPlat = 0x00;
constexpr int kSolidMin = 0x20, kSolidMax = 0x30;   // model-space bounding box (x forward, y left, z up)
constexpr int kSolidName = 0xA0;
constexpr int kPlatNumEntries = 0x10, kPlatEntries = 0x14;
constexpr int kPlatIndices = 0x24, kPlatIndexBuffer = 0x28, kPlatNumIndices = 0x2C;
constexpr int kEntrySize = 0x68;
constexpr int kEntryFormat = 0x34;                  // vertex format: +8 stride
constexpr int kEntryTris = 0x40, kEntryFirstIndex = 0x44;
constexpr int kEntryVertices = 0x4C;                // vertex data (first entry only), float xyz first
constexpr int kEntryVertexBytes = 0x50;
constexpr int kEntryNumIndices = 0x5C;

// Head: triangles above the top of the model minus HeadSize, in its rear part (the arms reach
// forward to the wheel at about shoulder height).
constexpr float kHeadDepth = 0.4f;
constexpr float kHeadNear = 0.6f;                   // head within this of the eyes: left out
constexpr float kSeatRange = 3.0f;                  // camera within this of the head: bumper / hood camera
constexpr float kSeatSpeed = 0.1f;                  // m/s while a seat key is held
constexpr float kSeatLimit = 0.6f;

template <typename T> T& At(void* base, int offset) { return *(T*)((char*)base + offset); }

// Each draw entry's triangles are reordered body, arms (and hands), head, so the seated driver
// can be drawn without any of the last two: one or two ranges of the entry.
// Ranges the seated driver can be drawn in, per draw entry: the body (and the arms when shown), the
// head when shown, and the triangles brought back by hand from a hidden part (see EditSet::show).
constexpr int kDriverPartSlots = 34;

struct Driver {
    // Resolved submission arguments, not the raw entry's material header.
    uint32_t skinShader = 0, skinAux = 0;
    std::array<uint32_t, 5> skinTextures{};
    bool skinBindingValid = false;
    bool ok = false;
    std::string car;
    DriverIK::Rig rig;
    bool rigTried = false;
    float head[3] = {};                             // head centre, model space
    std::vector<int> body, arms, heads;             // triangles per entry, in that order
    std::vector<uint8_t> parts;                     // per entry: kDriverPartSlots ranges to draw (kEntrySize each)
    // Per entry: runs (first index, triangles) of hidden-part triangles shown by hand, for runsKey.
    std::vector<std::vector<std::pair<int, int>>> shownRuns;
    int runsKey = -1;
    DriverSeamCache seamCache;
};

// The steering wheel inside a car's interior model (<CAR>_KIT00_INTERIOR_*), found as a ring of
// vertices in front of and below the driver's head: a disc in a plane tilted back from vertical,
// searched over tilt, centre and radius (checked offline against every car in the game). Its
// triangles are moved to the end of each draw entry, so the entry can be drawn as the rest of
// the interior and, with a matrix turned about the steering column, the wheel.
struct Wheel {
    bool ok = false;
    float centre[3] = {};                           // model space (x forward, y left, z up)
    float axis[3] = {};                             // steering column, toward the dashboard
    float radius = 0;
    // The in-car dashboard: where its panel goes (the gauge cluster behind the wheel), facing the
    // driver: centre, normal toward the head, the panel's right and up (model space).
    bool dashOk = false;
    V3 dash, dashN, dashRight, dashUp;
    std::vector<int> kept, turned;                  // per entry: triangles before the wheel, wheel triangles
    std::vector<uint8_t> parts;                     // per entry: the entry without the wheel, the wheel alone
};
struct WheelTemplate {
    Wheel wheel;
    std::vector<uint16_t> orderedIndices;
    std::vector<std::pair<int, int>> entryRanges;    // first index, triangle count per entry
};
// Only deterministic geometry rejections belong here, never missing resources or failed locks.
// Own the input data: streamed models can reuse both names and memory addresses.
struct RejectedWheel {
    std::string name;
    std::vector<V3> points;
    std::vector<uint16_t> indices;
    std::vector<std::pair<int, int>> ranges;
    std::array<float, 3> head;
};
std::vector<RejectedWheel> s_rejectedWheels;
// Side mirror glass that shares a draw entry with the rest of the body (BMW M3 GTR): in each such
// entry its triangles are moved to the end, the other triangles first, then the left glass, then
// the right, and the entry is submitted as up to three parts so the glass has draws of its own.
struct GlassSplit {
    struct Entry { int index; int count[3]; };   // other, left glass, right glass triangles
    std::vector<Entry> entries;
    std::vector<uint8_t> parts;                  // 3 entries per split entry
};
struct HeadPos { float v[3]; };
std::unordered_map<std::string, HeadPos> s_carHead; // driver's head per car (model space)

// A triangle by its three vertex indices, sorted: the same whatever order the index buffer has it
// in (the cockpit reorders draw entries) and whichever way round it is wound.
uint64_t TriId(uint16_t a, uint16_t b, uint16_t c) {
    if (a > b) std::swap(a, b);
    if (b > c) std::swap(b, c);
    if (a > b) std::swap(a, b);
    return ((uint64_t)a << 32) | ((uint64_t)b << 16) | c;
}
struct TriRef {
    V3 p[3];                                        // model space
    uint16_t v[3];                                  // as wound in the index buffer
    int entry;                                      // draw entry
    uint64_t id;
    bool hidden = false;                            // made zero-area in the index buffer (still pickable)
};

// The triangle editor's changes to one solid (by name, e.g. IS300_KIT00_INTERIOR_A), saved in
// nfsmw_vr_edits.ini: hidden (made degenerate in the index buffer), drawn black, added to or taken
// off the steering wheel, and shown although the seated driver's hidden arms or head contain them
// (Restore on a driver's hidden triangles). (Dash and NoAuto lines left by an earlier gauge blackout
// are not read.)
struct EditSet {
    std::map<uint64_t, std::array<uint16_t, 3>> hide;   // with the original winding, to put it back
    std::set<uint64_t> black, wheel, noWheel, show, handbrake, shifter, arms, skin, native, mirror, wipers;
    bool loaded = false;
};
const char* const kEditKeys[11] = {"Black", "Wheel", "NoWheel", "Show", "Handbrake", "Shifter", "Arms", "Skin", "Native", "Mirror", "Wipers"};
int s_editsGen = 0;                                 // counts edits, for caches built from them
std::unordered_map<std::string, EditSet> s_edits;

const char* EditsPath() {
    static char path[MAX_PATH];
    if (!path[0]) ConfigFile("nfsmw_vr_edits.ini", path, false);
    return path;
}

// "a.b.c,a.b.c,..." <-> triangles (vertex indices, as wound).
std::vector<std::array<uint16_t, 3>> ParseTris(const char* s) {
    std::vector<std::array<uint16_t, 3>> out;
    unsigned a, b, c;
    int n;
    while (*s && sscanf(s, "%u.%u.%u%n", &a, &b, &c, &n) == 3) {
        out.push_back({(uint16_t)a, (uint16_t)b, (uint16_t)c});
        s += n;
        while (*s == ',' || *s == ' ') s++;
    }
    return out;
}

std::string TrisText(const std::vector<std::array<uint16_t, 3>>& tris) {
    std::string s;
    char buf[32];
    for (const auto& t : tris) {
        snprintf(buf, sizeof(buf), "%s%u.%u.%u", s.empty() ? "" : ",", t[0], t[1], t[2]);
        s += buf;
    }
    return s;
}

std::array<uint16_t, 3> IdTri(uint64_t id) { return {(uint16_t)(id >> 32), (uint16_t)(id >> 16), (uint16_t)id}; }

EditSet& EditsFor(const std::string& solidName) {
    // The file is read once, whole: every model the game streams in (scenery too) asks here.
    static bool known;
    static std::set<std::string> sections;
    if (!known) {
        known = true;
        std::vector<char> names(1 << 16);
        DWORD n = GetPrivateProfileSectionNamesA(names.data(), (DWORD)names.size(), EditsPath());
        for (const char* p = names.data(); p < names.data() + n && *p; p += strlen(p) + 1) sections.insert(p);
    }
    EditSet& e = s_edits[solidName];
    if (e.loaded) return e;
    e.loaded = true;
    if (!sections.count(solidName)) return e;
    static std::vector<char> buf(1 << 17);
    auto read = [&](const char* key) {
        buf[0] = 0;
        GetPrivateProfileStringA(solidName.c_str(), key, "", buf.data(), (DWORD)buf.size(), EditsPath());
        return ParseTris(buf.data());
    };
    for (auto& t : read("Hide")) e.hide[TriId(t[0], t[1], t[2])] = t;
    std::set<uint64_t>* sets[11] = {&e.black, &e.wheel, &e.noWheel, &e.show, &e.handbrake, &e.shifter, &e.arms, &e.skin, &e.native, &e.mirror, &e.wipers};
    for (int k = 0; k < 11; k++)
        for (auto& t : read(kEditKeys[k])) sets[k]->insert(TriId(t[0], t[1], t[2]));
    size_t n = e.hide.size() + e.black.size() + e.wheel.size() + e.noWheel.size() + e.show.size() + e.mirror.size();
    if (n) Log("editor: %s: %u edited triangles loaded", solidName.c_str(), (unsigned)n);
    return e;
}

void SaveEdits(const std::string& solidName) {
    const EditSet& e = EditsFor(solidName);
    std::vector<std::array<uint16_t, 3>> hide;
    for (auto& h : e.hide) hide.push_back(h.second);
    const std::set<uint64_t>* sets[11] = {&e.black, &e.wheel, &e.noWheel, &e.show, &e.handbrake, &e.shifter, &e.arms, &e.skin, &e.native, &e.mirror, &e.wipers};
    WritePrivateProfileStringA(solidName.c_str(), "Hide", hide.empty() ? nullptr : TrisText(hide).c_str(), EditsPath());
    for (int k = 0; k < 11; k++) {
        std::vector<std::array<uint16_t, 3>> v;
        for (uint64_t id : *sets[k]) v.push_back(IdTri(id));
        WritePrivateProfileStringA(solidName.c_str(), kEditKeys[k], v.empty() ? nullptr : TrisText(v).c_str(), EditsPath());
    }
}

// Solids are streamed: a traffic car's or a reloaded car's model can be freed and another loaded
// at the same address. Each cache entry remembers what the solid looked like when it was seen, and
// is prepared again when that changed. Drawing a freed model's buffers through a stale entry made
// garbage polygons in the cockpit and corrupted d3d9 until it crashed releasing a buffer.
struct SolidIdentity {
    char name[64] = {};
    uint8_t* plat = nullptr;
    uint8_t* entries = nullptr;
    void* ib = nullptr;
    uint16_t* indices = nullptr;
};
struct SolidInfo : SolidIdentity {
    Driver* driver = nullptr;                       // nullptr = not a driver
    Wheel* wheel = nullptr;                         // interiors, once the car's driver head is known
    GlassSplit* split = nullptr;                    // bodies whose mirror glass shares draw entries
    bool interior = false;
    int mirrorKind = -1;                            // -1 not looked at, 0 none, 1 side mirror solid, 2 body, 3 interior
    MirrorGlass glass[3];                           // left, right, rear view (model space)
    void* glassIb[2] = {};                          // the side glass's registered draw entries (first indices)
    std::vector<int> glassFirsts[2];
    uint32_t indexHash = 0;                         // of the game's index copy, after reordering
    int hashSeen = -1;                              // eye the index hash above was last checked on
    const EditSet* edits = nullptr;                 // EditsFor(name), kept while the name is unchanged
    // Triangle editor: the solid's triangles (built when needed, again after its indices changed),
    // the hidden ones as applied to the index buffer (id -> winding), and what is drawn black.
    std::vector<TriRef> tris;
    bool trisDirty = true;
    std::map<uint64_t, std::array<uint16_t, 3>> hidden;
    std::vector<V3> black;                          // model space, 3 points per triangle
    bool blackDirty = true;
    bool manualMirrorLogged = false;
};
std::unordered_map<uint8_t*, SolidInfo> s_solids;
std::unordered_map<std::string, std::vector<WheelTemplate>> s_wheelTemplates;
std::vector<Driver*> s_retired;                     // replaced this frame; queued draws may still use them
std::vector<Wheel*> s_retiredWheels;
std::vector<GlassSplit*> s_retiredSplits;

// The game camera (before the seat and the eyes) this frame, as camera -> world.
M4 s_base;
bool s_haveBase = false;

// Nearest driver head to the game camera seen this frame, and the one in use.
struct Seen {
    float dist = 1e9f;
    float cam[3];
    std::string car;
    uint32_t worldId = 0;                           // never retain a render connection across frames
    float body[3];                                  // head in the car body's space (x fwd, y left, z up)
} s_frame;
struct {
    bool valid = false;
    float cam[3];                                   // head in game camera space (right, down, forward)
    std::string car;
    uint32_t worldId = 0;
    float body[3];
    double time = 0;
} s_head;

// The car being drawn in the player view (inside CarRenderConn::OnRender) and its body matrix.
uint8_t* s_curConn = nullptr;
M4 s_curBody;
bool s_curPov;
bool s_curPlayerCar = false;
// Compare only a seat actually applied in this frame, to the same car's player-view draw.
uint8_t* s_seatedConn = nullptr;
M4 s_seatedBody;
float s_bodyMismatch = 0;
unsigned s_bodyDraws = 0;

float s_offset[3];                                  // right, up, forward (m) for s_head.car
std::string s_offsetCar;
bool s_offsetDirty = false;

double Now() {
    static LARGE_INTEGER f;
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)f.QuadPart;
}

// Per-car settings in a file of their own per kind, next to nfsmw_vr.ini, each keeping its
// section from there ([Seat] <car>=..., Default included), so one can be shared or replaced without
// the rest. The triangle editor's are in nfsmw_vr_edits.ini.
struct CarIniFile {
    const char* section;
    const char* name;
    char path[MAX_PATH];
};
CarIniFile s_carInis[] = {{"Seat", "nfsmw_vr_seat.ini"},
                          {"Hud", "nfsmw_vr_hud.ini"},
                          {"Dash", "nfsmw_vr_dash.ini"},
                          {"DashLayout", "nfsmw_vr_dash.ini"},
                          {"MirrorOffset", "nfsmw_vr_mirror.ini"},
                          {"WheelPivot", "nfsmw_vr_wheel.ini"},
                          {"WheelPosition", "nfsmw_vr_wheel.ini"},
                          {"AnimatedDriver", "nfsmw_vr_driver.ini"}};

// Sections still in nfsmw_vr.ini (where they were before) are moved to their files, once; a line
// moved over replaces the file's (the file may be a release's defaults, the old line your own).
void MoveCarSections() {
    char ini[MAX_PATH];
    ConfigFile("nfsmw_vr.ini", ini, false);
    std::vector<char> names(1 << 14), buf(1 << 16);
    DWORD n = GetPrivateProfileSectionNamesA(names.data(), (DWORD)names.size(), ini);
    std::set<std::string> present;
    for (const char* p = names.data(); p < names.data() + n && *p; p += strlen(p) + 1) present.insert(p);
    for (CarIniFile& f : s_carInis) {
        if (!present.count(f.section)) continue;
        DWORD len = GetPrivateProfileSectionA(f.section, buf.data(), (DWORD)buf.size(), ini);
        if (len >= buf.size() - 2) continue;                // truncated: leave it where it is
        bool ok = true;
        int lines = 0;
        for (const char* p = buf.data(); p < buf.data() + len && *p; p += strlen(p) + 1) {
            const char* eq = strchr(p, '=');
            if (*p == ';' || !eq || eq == p) continue;
            std::string key(p, eq - p);
            ok = WritePrivateProfileStringA(f.section, key.c_str(), eq + 1, f.path) && ok;
            lines++;
        }
        if (!ok) continue;
        WritePrivateProfileStringA(f.section, nullptr, nullptr, ini);
        Log("cockpit: [%s] (%d lines) moved from nfsmw_vr.ini to %s", f.section, lines, f.name);
    }
}

const char* CarIni(const char* section) {
    static bool ready;
    if (!ready) {
        ready = true;
        for (CarIniFile& f : s_carInis) ConfigFile(f.name, f.path, false);
        MoveCarSections();
    }
    for (CarIniFile& f : s_carInis)
        if (strcmp(f.section, section) == 0) return f.path;
    return s_carInis[0].path;
}

void LoadOffset(const std::string& car) {
    char buf[96];
    GetPrivateProfileStringA("Seat", car.c_str(), "", buf, sizeof(buf), CarIni("Seat"));
    if (!buf[0]) GetPrivateProfileStringA("Seat", "Default", "0,0,0", buf, sizeof(buf), CarIni("Seat"));
    s_offset[0] = s_offset[1] = s_offset[2] = 0;
    sscanf(buf, "%f,%f,%f", &s_offset[0], &s_offset[1], &s_offset[2]);
    s_offsetCar = car;
    Log("cockpit: seat for %s: right %.2f up %.2f forward %.2f", car.c_str(), s_offset[0], s_offset[1], s_offset[2]);
}

void SaveOffset() {
    char buf[96];
    snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f", s_offset[0], s_offset[1], s_offset[2]);
    WritePrivateProfileStringA("Seat", s_offsetCar.c_str(), buf, CarIni("Seat"));
    s_offsetDirty = false;
    Log("cockpit: seat for %s saved: %s", s_offsetCar.c_str(), buf);
}

// The HUD panel and the in-car dashboard per car model (HudPerCar), like the seat: entering a car
// loads its own values if it has them, and a model with none starts from the mod's built-in
// defaults (captured before the config is read) instead of the last car's, so one model's
// placement never carries over to another. Changes made in a car are saved for it once they have
// settled for a second.
// [Hud]: size, distance, height, right (per car with HudPerCar); [Dash]: the display's placement
// and size, always per car model; [DashLayout] Default: element positions and sizes shared by all
// cars. The elements' HUD crops are global [VR] keys, the same for every car.
struct PerCar {
    const char* section;
    const char* what;
    int n;
    float* v[20];
    float lo[20], hi[20];                           // accepted when loading
    int least;                                      // values a line needs (older lines lack the last ones)
    bool nanOk;                                     // NaN accepted (automatic placement)
    bool always = false;                            // per car whatever HudPerCar says (like the seat)
    bool keepIfMissing = false;                     // no line: keep the values from the ini, not the built-in ones
    std::string car;
    float seen[20] = {};
    bool dirty = false;
    double changed = 0;
    float base[20] = {};                            // the mod's built-in values: where a model with none of its own starts
};
PerCar s_hudCar = {"Hud", "HUD", 4, {&g_cfg.hudWidth, &g_cfg.hudDistance, &g_cfg.hudHeight, &g_cfg.hudRight},
                   {0.1f, 0.1f, -5, -5}, {10, 10, 5, 5}, 3, false};
float s_retiredDashCover = 1.3f;                    // a retired size, kept so saved [Dash] lines read the same
PerCar s_dashCar = {"Dash", "dashboard", 10,
                    {&g_cfg.dashRight, &g_cfg.dashUp, &g_cfg.dashZoom, &s_retiredDashCover, &g_cfg.dashDepth,
                     &g_cfg.dashScreenW, &g_cfg.dashScreenH, &g_cfg.dashYaw, &g_cfg.dashPitch, &g_cfg.dashRoll},
                    {-1, -1, 0.2f, 1, -0.2f, 0.02f, 0.02f, -90, -90, -180}, {1, 1, 4, 4, 0.3f, 1, 1, 90, 90, 180}, 3, false, true};
// Each dashboard element's x, y and size (Config::dashEl), in that order per element.
PerCar s_dashLayoutCar = {"DashLayout", "dashboard layout", 18,
                          {&g_cfg.dashEl[0][0], &g_cfg.dashEl[0][1], &g_cfg.dashEl[0][2], &g_cfg.dashEl[1][0],
                           &g_cfg.dashEl[1][1], &g_cfg.dashEl[1][2], &g_cfg.dashEl[2][0], &g_cfg.dashEl[2][1],
                           &g_cfg.dashEl[2][2], &g_cfg.dashEl[3][0], &g_cfg.dashEl[3][1], &g_cfg.dashEl[3][2],
                           &g_cfg.dashEl[4][0], &g_cfg.dashEl[4][1], &g_cfg.dashEl[4][2], &g_cfg.dashEl[5][0],
                           &g_cfg.dashEl[5][1], &g_cfg.dashEl[5][2]},
                          {-1, -1, 0, -1, -1, 0, -1, -1, 0, -1, -1, 0, -1, -1, 0, -1, -1, 0},
                          {1, 1, 5, 1, 1, 5, 1, 1, 5, 1, 1, 5, 1, 1, 5, 1, 1, 5}, 15, true, true, true};

PerCar s_mirrorCar = {"MirrorOffset", "rear view mirror offset", 1, {&g_cfg.mirrorOffset}, {-0.1f}, {0.1f}, 1, false, true};
// The steering wheel's turning centre moved (right, up; Config::wheelPivot).
PerCar s_pivotCar = {"WheelPivot", "steering wheel turning centre", 2, {&g_cfg.wheelPivot[0], &g_cfg.wheelPivot[1]},
                     {-0.1f, -0.1f}, {0.1f, 0.1f}, 2, false, true};
PerCar s_wheelPositionCar = {"WheelPosition", "steering wheel location", 2,
                            {&g_cfg.wheelPosition[0], &g_cfg.wheelPosition[1]},
                            {-0.5f, -0.5f}, {0.5f, 0.5f}, 2, false, true};
PerCar s_animatedDriverCar = {"AnimatedDriver", "animated driver hand fit", 2,
                            {&g_cfg.animatedDriverSpread, &g_cfg.animatedDriverHandFix},
                            {0.5f, -60}, {1.5f, 60}, 1, false, true};

// WritePrivateProfileStringA rewrites the whole file and can take milliseconds on the game thread:
// an auto-save landing mid-frame was a visible hitch. One worker thread writes the queued lines in
// order; saving keeps the menu and per-car auto-save off the render thread.
struct PendingCarSave {
    std::string path, section, car, value, what;
};
// Never destroyed: the detached writer may still be waiting when the DLL is unloaded.
struct CarSaveQueue {
    std::deque<PendingCarSave> pending;
    std::mutex lock;
    std::condition_variable wake, idle;
    bool started = false, writing = false;
};
static CarSaveQueue* s_carSaves = new CarSaveQueue();

static void CarSaveWorker() {
    for (;;) {
        PendingCarSave w;
        {
            std::unique_lock<std::mutex> lock(s_carSaves->lock);
            s_carSaves->wake.wait(lock, [] { return !s_carSaves->pending.empty(); });
            w = std::move(s_carSaves->pending.front());
            s_carSaves->pending.pop_front();
            s_carSaves->writing = true;
        }
        WritePrivateProfileStringA(w.section.c_str(), w.car.c_str(), w.value.c_str(), w.path.c_str());
        Log("cockpit: %s for %s saved: %s", w.what.c_str(), w.car.c_str(), w.value.c_str());
        {
            std::lock_guard<std::mutex> lock(s_carSaves->lock);
            s_carSaves->writing = false;
        }
        s_carSaves->idle.notify_all();
    }
}

// Tests only: wait until every queued line has been written.
static void FlushCarSaves() {
    std::unique_lock<std::mutex> lock(s_carSaves->lock);
    s_carSaves->idle.wait(lock, [] { return s_carSaves->pending.empty() && !s_carSaves->writing; });
}

static void QueueCarSave(PendingCarSave&& w) {
    {
        std::lock_guard<std::mutex> lock(s_carSaves->lock);
        if (!s_carSaves->started) {
            s_carSaves->started = true;
            std::thread(CarSaveWorker).detach();   // lives for the process; writes are serialized
        }
        s_carSaves->pending.push_back(std::move(w));
    }
    s_carSaves->wake.notify_one();
}

void SavePerCar(PerCar& p) {
    char buf[512];
    int len = 0;
    for (int i = 0; i < p.n && len < (int)sizeof(buf) - 16; i++)
        len += snprintf(buf + len, sizeof(buf) - len, i ? ",%.4f" : "%.4f", *p.v[i]);
    PendingCarSave w;
    w.path = CarIni(p.section);
    w.section = p.section;
    w.car = p.car;
    w.value = buf;
    w.what = p.what;
    QueueCarSave(std::move(w));
    p.dirty = false;
}

void UpdatePerCar(PerCar& p, const std::string& car) {
    if ((g_cfg.hudPerCar < 0.5f && !p.always) || car.empty()) {
        if (p.dirty && !p.car.empty()) SavePerCar(p);
        p.car.clear();
        return;
    }
    if (car != p.car) {
        if (p.dirty && !p.car.empty()) SavePerCar(p);
        p.car = car;
        char buf[512];
        GetPrivateProfileStringA(p.section, car.c_str(), "", buf, sizeof(buf), CarIni(p.section));
        // Lines saved before a value existed lack it: those start from the mod's built-in value
        // (older [Hud]: no "right", 0; [Dash]: no angles, 0), never the car we came from.
        float x[20] = {};
        int got = 0;
        for (char* s = buf; *s && got < p.n;) {
            char* end;
            x[got] = strtof(s, &end);
            if (end == s) break;
            got++;
            s = end;
            while (*s == ',' || *s == ' ') s++;
        }
        for (int i = got; i < p.n; i++) x[i] = p.base[i];
        bool ok = got >= p.least;
        for (int i = 0; i < p.n && ok; i++) ok = (p.nanOk && x[i] != x[i]) || (x[i] >= p.lo[i] && x[i] <= p.hi[i]);
        if (ok) {
            for (int i = 0; i < p.n; i++) *p.v[i] = x[i];
            Log("cockpit: %s for %s: %s", p.what, car.c_str(), buf);
        } else if (p.keepIfMissing) {
            Log("cockpit: %s for %s: none saved, the ini's values kept", p.what, car.c_str());
        } else {
            // No line for this car model: start from the mod's defaults, not the car we came
            // from, so each model's placement is its own.
            for (int i = 0; i < p.n; i++) *p.v[i] = p.base[i];
            Log("cockpit: %s for %s: none saved, the default used", p.what, car.c_str());
        }
        for (int i = 0; i < p.n; i++) p.seen[i] = *p.v[i];
        return;
    }
    bool moved = false;
    for (int i = 0; i < p.n; i++) {
        float a = *p.v[i], b = p.seen[i];
        if ((a == a) != (b == b) || (a == a && fabsf(a - b) > 1e-4f)) moved = true;
        p.seen[i] = a;
    }
    if (moved) {
        p.dirty = true;
        p.changed = Now();
    } else if (p.dirty && Now() - p.changed > 1.0) {
        SavePerCar(p);
    }
}

void UpdateAnimatedDriverCar(const std::string& car) {
    // Same per-car auto-save as the HUD, dashboard and seat: a changed fit is written for this car
    // once it has been left alone for a moment, and a car change writes any pending fit first.
    UpdatePerCar(s_animatedDriverCar, car);
    // "Save for this car" remains an immediate save.
    if (g_cfg.animatedDriverSave > 0.5f) {
        if (!car.empty()) SavePerCar(s_animatedDriverCar);
        g_cfg.animatedDriverSave = 0;
    }
}

void UpdateHudCar(const std::string& car) {
    UpdateAnimatedDriverCar(car);
    UpdatePerCar(s_hudCar, car);
    UpdatePerCar(s_dashCar, car);
    UpdatePerCar(s_dashLayoutCar, "Default");
    UpdatePerCar(s_mirrorCar, car);
    UpdatePerCar(s_pivotCar, car);
    UpdatePerCar(s_wheelPositionCar, car);
}

// Arms and hands: in front of the head, above the lap, and out to the side (the chest is in the
// middle). Checked against every car's driver model offline.
constexpr float kArmsForward = 0.10f, kArmsBelowHead = 0.48f, kArmsSideways = 0.06f;   // 0.48: hands low on the wheel in low cars (Gallardo)

// Reorders each entry's triangles body, arms, head (head: high up at the back of the model).
Driver* Prepare(uint8_t* solid, const char* name) {
    Driver* d = new Driver;
    const char* kit = strstr(name, "_KIT");
    d->car.assign(name, kit ? kit - name : strlen(name));
    uint8_t* plat = At<uint8_t*>(solid, kSolidPlat);
    int n = plat ? At<int>(plat, kPlatNumEntries) : 0;
    uint8_t* entries = plat ? At<uint8_t*>(plat, kPlatEntries) : nullptr;
    uint16_t* indices = plat ? At<uint16_t*>(plat, kPlatIndices) : nullptr;
    auto* ib = plat ? At<IDirect3DIndexBuffer9*>(plat, kPlatIndexBuffer) : nullptr;
    int numIndices = plat ? At<int>(plat, kPlatNumIndices) : 0;
    if (n <= 0 || n > 64 || !entries || !indices || !ib) {
        Log("cockpit: %s: no draw entries", name);
        return d;
    }
    uint8_t* verts = nullptr;
    int stride = 0, numVerts = 0;
    for (int i = 0; i < n && !verts; i++) {
        uint8_t* e = entries + i * kEntrySize;
        verts = At<uint8_t*>(e, kEntryVertices);
        uint8_t* fmt = At<uint8_t*>(e, kEntryFormat);
        stride = fmt ? At<int>(fmt, 8) : 0;
        numVerts = stride > 0 ? At<int>(e, kEntryVertexBytes) / stride : 0;
    }
    D3DINDEXBUFFER_DESC desc;
    if (!verts || stride < 12 || stride > 128 || FAILED(ib->GetDesc(&desc)) || desc.Format != D3DFMT_INDEX16 ||
        desc.Size < (UINT)numIndices * 2) {
        Log("cockpit: %s: unexpected vertex or index data (stride %d)", name, stride);
        return d;
    }
    auto pos = [&](int v) { return (const float*)(verts + v * stride); };
    auto centroid = [&](const uint16_t* tri, float* c) {
        c[0] = c[1] = c[2] = 0;
        for (int k = 0; k < 3; k++) {
            if (tri[k] >= numVerts) return false;
            for (int a = 0; a < 3; a++) c[a] += pos(tri[k])[a] / 3;
        }
        return true;
    };
    float topZ = At<float>(solid, kSolidMax + 8) - g_cfg.headSize;
    float backX = At<float>(solid, kSolidMin) + kHeadDepth;
    auto isHead = [&](const float* c) { return c[2] > topZ && c[0] < backX; };

    // The head's centre first: the arms are found relative to it.
    float sum[3] = {};
    int headVerts = 0;
    for (int i = 0; i < n; i++) {
        uint8_t* e = entries + i * kEntrySize;
        int tris = At<int>(e, kEntryTris), first = At<int>(e, kEntryFirstIndex);
        if (tris <= 0 || first < 0 || first + tris * 3 > numIndices) continue;
        for (int t = 0; t < tris; t++) {
            const uint16_t* tri = indices + first + t * 3;
            float c[3];
            if (!centroid(tri, c) || !isHead(c)) continue;
            for (int k = 0; k < 3; k++, headVerts++)
                for (int a = 0; a < 3; a++) sum[a] += pos(tri[k])[a];
        }
    }
    if (headVerts)
        for (int a = 0; a < 3; a++) d->head[a] = sum[a] / headVerts;

    d->parts.assign(n * kDriverPartSlots * kEntrySize, 0);
    for (int i = 0; i < n; i++) {
        uint8_t* e = entries + i * kEntrySize;
        int tris = At<int>(e, kEntryTris), first = At<int>(e, kEntryFirstIndex);
        d->body.push_back(tris);
        d->arms.push_back(0);
        d->heads.push_back(0);
        if (tris <= 0 || first < 0 || first + tris * 3 > numIndices) continue;
        uint16_t* idx = indices + first;
        std::vector<uint16_t> body, arms, head;
        for (int t = 0; t < tris; t++) {
            const uint16_t* tri = idx + t * 3;
            float c[3];
            bool valid = centroid(tri, c);
            std::vector<uint16_t>* to = &body;
            if (valid && isHead(c)) to = &head;
            else if (valid && headVerts && c[0] > d->head[0] + kArmsForward && c[2] > d->head[2] - kArmsBelowHead &&
                     fabsf(c[1] - d->head[1]) > kArmsSideways)
                to = &arms;
            to->insert(to->end(), tri, tri + 3);
        }
        // Some drivers split the hands into a draw entry of their own (a second DRIVER_PLAYER
        // material): an entry that is mostly arms is all arms, so no stray hand pieces stay.
        if (!arms.empty() && arms.size() * 4 >= (body.size() + arms.size()) * 3) {
            arms.insert(arms.begin(), body.begin(), body.end());
            body.clear();
        }
        d->body[i] = (int)body.size() / 3;
        d->arms[i] = (int)arms.size() / 3;
        d->heads[i] = (int)head.size() / 3;
        if (arms.empty() && head.empty()) continue;
        body.insert(body.end(), arms.begin(), arms.end());
        body.insert(body.end(), head.begin(), head.end());
        void* p = nullptr;
        if (FAILED(ib->Lock(first * 2, tris * 6, &p, 0)) || !p) {
            Log("cockpit: %s: index buffer lock failed", name);
            return d;
        }
        memcpy(p, body.data(), tris * 6);
        ib->Unlock();
        memcpy(idx, body.data(), tris * 6);
    }
    if (headVerts) {
        d->ok = true;
        HeadPos& h = s_carHead[d->car];
        memcpy(h.v, d->head, sizeof(h.v));
    }
    Log("cockpit: %s: head %.2f %.2f %.2f, triangles per entry (body/arms/head):%s", name, d->head[0], d->head[1],
        d->head[2], [&] {
            static char buf[256];
            buf[0] = 0;
            for (int i = 0; i < n && strlen(buf) < 200; i++) sprintf(buf + strlen(buf), " %d/%d/%d", d->body[i], d->arms[i], d->heads[i]);
            return buf;
        }());
    return d;
}

// Ring search, in planes tilted back by 0..40 degrees: for each centre, count vertices by distance
// along the column and by radius, and take the bin with the most vertices spread all round.
int WheelOctant(float du, float dw) {
    if (du == 0 && dw == 0) return 4;
    if (du >= 0) {
        if (dw >= 0) return du == 0 ? 6 : (dw < du ? 4 : 5);
        return du == 0 || -dw > du ? 2 : 3;
    }
    if (dw >= 0) return dw == 0 ? 0 : (dw > -du ? 6 : 7);
    return -dw < -du ? 0 : 1;
}

// depthBin: the bins' depth along the column (2 cm; 4 cm finds a thick, low-poly rim such as the
// Clio's, whose vertices spread over several 2 cm slices).
bool FindWheel(const std::vector<V3>& pts, const float head[3], Wheel& w, float depthBin = 0.02f) {
    std::vector<V3> cand;
    for (const V3& p : pts)
        if (p.x > head[0] + 0.2f && p.x < head[0] + 0.85f && fabsf(p.y - head[1]) < 0.32f && p.z > head[2] - 0.55f &&
            p.z < head[2] - 0.02f)
            cand.push_back(p);
    if (cand.size() < 12) return false;
    struct Bin { int count; uint8_t octants; uint32_t epoch; };
    Bin bins[160][20] = {};
    std::vector<int> used;
    used.reserve(3200);
    int bestScore = 0;
    float best[5] = {};                             // tilt, centre y, centre height, column distance, radius
    int bestCwi = 0;
    std::vector<float> d(cand.size()), wv(cand.size());
    uint32_t epoch = 0;
    auto scan = [&](int cwiFirst, int cwiLast, int cwiStep) {
        for (int ti = 0; ti <= 16; ti++) {
            float th = ti * 2.5f * 3.14159265f / 180.0f;
            float n[2] = {cosf(th), -sinf(th)}, up[2] = {sinf(th), cosf(th)};
            for (size_t k = 0; k < cand.size(); k++) {
                d[k] = cand[k].x * n[0] + cand[k].z * n[1];
                wv[k] = cand[k].x * up[0] + cand[k].z * up[1];
            }
            float headW = head[0] * up[0] + head[2] * up[1];
            for (int cu10 = -3; cu10 <= 3; cu10++) {
                float cu = head[1] + cu10 * 0.02f;
                for (int cwi = cwiFirst; cwi <= cwiLast; cwi += cwiStep) {
                    float cw = headW - 0.55f + cwi * 0.01f;
                    used.clear();
                    if (++epoch == 0) {
                        memset(bins, 0, sizeof(bins));
                        epoch = 1;
                    }
                    for (size_t k = 0; k < cand.size(); k++) {
                        float du = cand[k].y - cu;
                        if (fabsf(du) > 0.24f) continue;
                        float dw = wv[k] - cw;
                        if (fabsf(dw) > 0.24f) continue;
                        float r2 = du * du + dw * dw;
                        if (r2 < 0.014399f || r2 > 0.057601f) continue;
                        int db = (int)lrintf(d[k] / depthBin) + 80;
                        int rb = (int)lrintf(sqrtf(r2) / 0.015f);
                        if (db < 0 || db >= 160 || rb < 0 || rb >= 20) continue;
                        Bin& b = bins[db][rb];
                        if (b.epoch != epoch) {
                            b.count = 0;
                            b.octants = 0;
                            b.epoch = epoch;
                            used.push_back(db * 20 + rb);
                        }
                        b.count++;
                        b.octants |= (uint8_t)(1 << WheelOctant(du, dw));
                    }
                    for (int key : used) {
                        const Bin& b = bins[key / 20][key % 20];
                        int octs = 0;
                        for (int o = 0; o < 8; o++) octs += (b.octants >> o) & 1;
                        if (octs < 7 || b.count * octs <= bestScore) continue;
                        bestScore = b.count * octs;
                        best[0] = th; best[1] = cu; best[2] = cw;
                        best[3] = (key / 20 - 80) * depthBin; best[4] = (key % 20) * 0.015f;
                        bestCwi = cwi;
                    }
                }
            }
        }
    };
    scan(0, 48, 4);
    if (!bestScore) return false;
    scan(std::max(0, bestCwi - 4), std::min(49, bestCwi + 4), 1);
    if (!bestScore) return false;
    float th = best[0];
    // Back from (column distance, wheel-up height) to model x and z.
    float c0[3] = {best[3] * cosf(th) + best[2] * sinf(th), best[1], -best[3] * sinf(th) + best[2] * cosf(th)};
    // The search bins are coarse (any tilt within a few degrees holds the whole rim), so fit the rim
    // vertices: centre = their mean, tilt = the plane through them (column kept in the x-z plane).
    float sum[3] = {}, a = 0, b = 0, c = 0;
    int count = 0;
    std::vector<V3> rim;
    for (const V3& p : cand) {
        float rel[3] = {p.x - c0[0], p.y - c0[1], p.z - c0[2]};
        float along = rel[0] * cosf(th) - rel[2] * sinf(th);
        float r = sqrtf(std::max(0.0f, rel[0] * rel[0] + rel[1] * rel[1] + rel[2] * rel[2] - along * along));
        if (fabsf(along) > 0.03f || fabsf(r - best[4]) > 0.03f) continue;
        rim.push_back(p);
        sum[0] += p.x; sum[1] += p.y; sum[2] += p.z;
        count++;
    }
    if (count < 8) return false;
    float centre[3] = {sum[0] / count, sum[1] / count, sum[2] / count};
    for (const V3& p : rim) {
        float dx = p.x - centre[0], dz = p.z - centre[2];
        a += dx * dx; b += dz * dz; c += dx * dz;
    }
    // Minimises the squared distances along the column (cos t, 0, -sin t): t = atan2(2c, b - a) / 2.
    float fitted = 0.5f * atan2f(2 * c, b - a);
    if (fabsf(fitted - th) < 0.2f) th = fitted;
    w.axis[0] = cosf(th); w.axis[1] = 0; w.axis[2] = -sinf(th);
    memcpy(w.centre, centre, sizeof(centre));
    w.radius = best[4];
    return true;
}

// Whether a vertex lies on the wheel: near its plane and within its rim.
bool OnWheel(const Wheel& w, const float* p) {
    float rel[3] = {p[0] - w.centre[0], p[1] - w.centre[1], p[2] - w.centre[2]};
    float along = rel[0] * w.axis[0] + rel[1] * w.axis[1] + rel[2] * w.axis[2];
    float off2 = rel[0] * rel[0] + rel[1] * rel[1] + rel[2] * rel[2] - along * along;
    float rim = w.radius + 0.035f;
    return along > -0.06f && along < 0.05f && off2 < rim * rim;
}

// The in-car dashboard's place: from the driver's head over the top half of the wheel (its centre
// plus 0.55 of the radius, up in the wheel's plane) to the first surface of the interior past the
// wheel (the gauge cluster), facing the driver. Without a hit, a point behind the top of the wheel.
// The panel's plane stands 3 cm off that surface toward the driver, clear of the cluster's curves;
// the wheel is 15 cm or more in front of it.
constexpr float kDashLift = 0.55f, kDashReach = 0.6f, kDashStandoff = 0.03f;
using mirror::Sub;
using mirror::Cross;
using mirror::Dot;
using mirror::Norm;

bool RayTriangle(V3 o, V3 d, V3 a, V3 b, V3 c, float* t) {
    V3 e1 = Sub(b, a), e2 = Sub(c, a), p = Cross(d, e2);
    float det = Dot(e1, p);
    if (fabsf(det) < 1e-9f) return false;
    float inv = 1.0f / det;
    V3 s = Sub(o, a);
    float u = Dot(s, p) * inv;
    if (u < 0 || u > 1) return false;
    V3 q = Cross(s, e1);
    float v = Dot(d, q) * inv;
    if (v < 0 || u + v > 1) return false;
    *t = Dot(e2, q) * inv;
    return true;
}

void FindDash(Wheel& w, const std::vector<TriRef>& tris, const float headPos[3], const char* name) {
    V3 head = {headPos[0], headPos[1], headPos[2]}, c = {w.centre[0], w.centre[1], w.centre[2]};
    V3 k = Norm({w.axis[0], w.axis[1], w.axis[2]}), z = {0, 0, 1};
    V3 up = Norm(Sub(z, k * Dot(z, k)));
    V3 target = c + up * (w.radius * kDashLift);
    V3 to = Sub(target, head);
    float past = sqrtf(Dot(to, to)) + 0.03f;
    V3 d = Norm(to);
    float best = 1e9f;
    int bestTri = -1;
    for (size_t i = 0; i < tris.size(); i++) {
        float t;
        if (RayTriangle(head, d, tris[i].p[0], tris[i].p[1], tris[i].p[2], &t) && t > past && t < past + kDashReach && t < best) {
            best = t;
            bestTri = (int)i;
        }
    }
    V3 hit = bestTri >= 0 ? head + d * best : c + k * 0.12f + up * (w.radius * 0.7f);
    V3 n = d * -1.0f;
    V3 pu = Norm(Sub(z, n * Dot(z, n)));
    V3 right = Norm(Cross(d, pu));
    w.dashN = n;
    w.dashUp = pu;
    w.dashRight = right;
    w.dash = hit - d * kDashStandoff;
    w.dashOk = true;
    Log("cockpit: %s: dashboard panel at %.2f %.2f %.2f (%s, %.2f m past the wheel, draw entry %d)", name, w.dash.x,
        w.dash.y, w.dash.z, bestTri >= 0 ? "gauge cluster" : "no surface found, behind the wheel",
        bestTri >= 0 ? best - past + 0.03f : 0.0f, bestTri >= 0 ? tris[bestTri].entry : -1);
}

// The panel's plane in the world: origin (with the offsets from F4, dashRight / dashUp / dashDepth,
// per car model), right, up and toward the driver (unit). The panel is turned about its own centre:
// dashYaw about its up axis (+ = its face turned to the driver's right), then dashPitch about its
// right axis (+ = its face tilted up), then roll clockwise as seen by the driver.
void DashBasis(const Wheel& w, const float* m, float origin[3], float right[3], float up[3], float toEye[3]) {
    V3 o = w.dash + w.dashRight * g_cfg.dashRight + w.dashUp * g_cfg.dashUp + w.dashN * g_cfg.dashDepth;
    const float kDeg = 0.017453293f;
    float cy = cosf(g_cfg.dashYaw * kDeg), sy = sinf(g_cfg.dashYaw * kDeg);
    float cp = cosf(g_cfg.dashPitch * kDeg), sp = sinf(g_cfg.dashPitch * kDeg);
    V3 r1 = w.dashRight * cy - w.dashN * sy, n1 = w.dashN * cy + w.dashRight * sy;
    V3 u2 = w.dashUp * cp - n1 * sp, n2 = n1 * cp + w.dashUp * sp;
    float cr = cosf(g_cfg.dashRoll * kDeg), sr = sinf(g_cfg.dashRoll * kDeg);
    V3 r3 = r1 * cr - u2 * sr, u3 = u2 * cr + r1 * sr;
    for (int a = 0; a < 3; a++) origin[a] = o.x * m[a] + o.y * m[4 + a] + o.z * m[8 + a] + m[12 + a];
    auto dir = [&](V3 v, float* out) {
        V3 r = {v.x * m[0] + v.y * m[4] + v.z * m[8], v.x * m[1] + v.y * m[5] + v.z * m[9], v.x * m[2] + v.y * m[6] + v.z * m[10]};
        r = Norm(r);
        out[0] = r.x; out[1] = r.y; out[2] = r.z;
    };
    dir(r3, right);
    dir(u3, up);
    dir(n2, toEye);
}

Wheel* PrepareWheel(uint8_t* solid, const char* name, const float head[3]) {
    Wheel* w = new Wheel;
    uint8_t* plat = At<uint8_t*>(solid, kSolidPlat);
    int n = plat ? At<int>(plat, kPlatNumEntries) : 0;
    uint8_t* entries = plat ? At<uint8_t*>(plat, kPlatEntries) : nullptr;
    uint16_t* indices = plat ? At<uint16_t*>(plat, kPlatIndices) : nullptr;
    auto* ib = plat ? At<IDirect3DIndexBuffer9*>(plat, kPlatIndexBuffer) : nullptr;
    int numIndices = plat ? At<int>(plat, kPlatNumIndices) : 0;
    if (n <= 0 || n > 64 || !entries || !indices || !ib) return w;
    uint8_t* verts = nullptr;
    int stride = 0, numVerts = 0;
    for (int i = 0; i < n && !verts; i++) {
        uint8_t* e = entries + i * kEntrySize;
        verts = At<uint8_t*>(e, kEntryVertices);
        uint8_t* fmt = At<uint8_t*>(e, kEntryFormat);
        stride = fmt ? At<int>(fmt, 8) : 0;
        numVerts = stride > 0 ? At<int>(e, kEntryVertexBytes) / stride : 0;
    }
    D3DINDEXBUFFER_DESC desc;
    if (!verts || stride < 12 || stride > 128 || numVerts <= 0 || FAILED(ib->GetDesc(&desc)) ||
        desc.Format != D3DFMT_INDEX16 || desc.Size < (UINT)numIndices * 2)
        return w;
    auto pos = [&](int v) { return (const float*)(verts + v * stride); };
    std::vector<V3> pts(numVerts);
    for (int v = 0; v < numVerts; v++) pts[v] = {pos(v)[0], pos(v)[1], pos(v)[2]};
    RejectedWheel input;
    input.name = name;
    input.head = {head[0], head[1], head[2]};
    // Bound the copy and require a usable index copy before consulting the negative cache.
    bool cacheable = numIndices > 0 && numIndices < (1 << 20);
    if (cacheable) {
        input.points = pts;
        input.indices.assign(indices, indices + numIndices);
        for (int i = 0; i < n; i++) {
            uint8_t* e = entries + i * kEntrySize;
            input.ranges.push_back({At<int>(e, kEntryFirstIndex), At<int>(e, kEntryTris)});
        }
        for (const auto& rejected : s_rejectedWheels) {
            if (rejected.name == input.name && rejected.head == input.head &&
                rejected.ranges == input.ranges && rejected.indices == input.indices &&
                rejected.points.size() == pts.size() &&
                !memcmp(rejected.points.data(), pts.data(), pts.size() * sizeof(V3)))
                return w;
        }
    }
    auto rememberRejection = [&]() {
        if (!cacheable) return;
        // Bounded across car changes; no live game pointers or GPU resources are retained.
        if (s_rejectedWheels.size() >= 16) s_rejectedWheels.erase(s_rejectedWheels.begin());
        s_rejectedWheels.push_back(std::move(input));
    };
    // A wheel the usual search misses (the Clio's, the only one in the game) is looked for again
    // with deeper bins; a wheel the first search finds is never searched for that way.
    if (!FindWheel(pts, head, *w) && !FindWheel(pts, head, *w, 0.04f)) {
        Log("cockpit: %s: no steering wheel found", name);
        rememberRejection();
        return w;
    }
    w->parts.assign(n * 2 * kEntrySize, 0);
    std::vector<TriRef> rest;                       // the interior's other triangles
    const EditSet& edits = EditsFor(name);          // triangles put on or taken off the wheel by hand
    int total = 0;
    std::vector<std::vector<uint16_t>> orders(n);
    for (int i = 0; i < n; i++) {
        uint8_t* e = entries + i * kEntrySize;
        int tris = At<int>(e, kEntryTris), first = At<int>(e, kEntryFirstIndex);
        w->kept.push_back(tris);
        w->turned.push_back(0);
        if (tris <= 0 || first < 0 || first + tris * 3 > numIndices) continue;
        std::vector<uint16_t> keep, wheel;
        const uint16_t* idx = indices + first;
        for (int t = 0; t < tris; t++) {
            const uint16_t* tri = idx + t * 3;
            bool valid = tri[0] < numVerts && tri[1] < numVerts && tri[2] < numVerts;
            bool on = valid;
            for (int k = 0; k < 3 && on; k++) on = OnWheel(*w, pos(tri[k]));
            uint64_t id = valid ? TriId(tri[0], tri[1], tri[2]) : 0;
            if (valid && edits.wheel.count(id)) on = true;
            else if (valid && edits.noWheel.count(id)) on = false;
            (on ? wheel : keep).insert((on ? wheel : keep).end(), tri, tri + 3);
            if (!on && valid && tri[0] != tri[1] && tri[1] != tri[2] && tri[0] != tri[2]) rest.push_back({{pts[tri[0]], pts[tri[1]], pts[tri[2]]}, {tri[0], tri[1], tri[2]}, i, id});
        }
        w->kept[i] = (int)keep.size() / 3;
        w->turned[i] = (int)wheel.size() / 3;
        total += w->turned[i];
        keep.insert(keep.end(), wheel.begin(), wheel.end());
        orders[i].swap(keep);
    }
    // A real wheel has well over a hundred triangles; a few means something else matched.
    if (total < 80) {
        Log("cockpit: %s: steering wheel candidate rejected (%d triangles)", name, total);
        rememberRejection();
        return w;
    }
    for (int i = 0; i < n; i++) {
        if (!w->turned[i]) continue;
        uint8_t* e = entries + i * kEntrySize;
        int tris = At<int>(e, kEntryTris), first = At<int>(e, kEntryFirstIndex);
        void* p = nullptr;
        if (FAILED(ib->Lock(first * 2, tris * 6, &p, 0)) || !p) {
            Log("cockpit: %s: index buffer lock failed", name);
            return w;
        }
        memcpy(p, orders[i].data(), tris * 6);
        ib->Unlock();
        memcpy(indices + first, orders[i].data(), tris * 6);
    }
    w->ok = true;
    Log("cockpit: %s: steering wheel at %.2f %.2f %.2f, column %.2f %.2f %.2f, radius %.3f, %d triangles", name,
        w->centre[0], w->centre[1], w->centre[2], w->axis[0], w->axis[1], w->axis[2], w->radius, total);
    FindDash(*w, rest, head, name);
    return w;
}

bool WheelTemplateLayout(uint8_t* solid, const WheelTemplate& cached) {
    uint8_t* plat = At<uint8_t*>(solid, kSolidPlat);
    int n = plat ? At<int>(plat, kPlatNumEntries) : 0;
    int numIndices = plat ? At<int>(plat, kPlatNumIndices) : 0;
    uint8_t* entries = plat ? At<uint8_t*>(plat, kPlatEntries) : nullptr;
    if (!entries || n != (int)cached.entryRanges.size() || numIndices != (int)cached.orderedIndices.size()) return false;
    for (int i = 0; i < n; i++) {
        uint8_t* entry = entries + i * kEntrySize;
        if (At<int>(entry, kEntryFirstIndex) != cached.entryRanges[i].first ||
            At<int>(entry, kEntryTris) != cached.entryRanges[i].second)
            return false;
    }
    return true;
}

Wheel* ReuseWheelTemplate(uint8_t* solid, const char* name) {
    auto templates = s_wheelTemplates.find(name);
    if (templates == s_wheelTemplates.end()) return nullptr;
    for (const WheelTemplate& cached : templates->second) {
        if (!WheelTemplateLayout(solid, cached)) continue;
        uint8_t* plat = At<uint8_t*>(solid, kSolidPlat);
        uint16_t* indices = At<uint16_t*>(plat, kPlatIndices);
        auto* ib = At<IDirect3DIndexBuffer9*>(plat, kPlatIndexBuffer);
        D3DINDEXBUFFER_DESC desc;
        if (!indices || !ib || FAILED(ib->GetDesc(&desc)) || desc.Format != D3DFMT_INDEX16 ||
            desc.Size < cached.orderedIndices.size() * sizeof(uint16_t)) {
            Log("cockpit: %s: cached steering wheel layout is incompatible with its index buffer", name);
            continue;
        }
        void* locked = nullptr;
        UINT bytes = (UINT)(cached.orderedIndices.size() * sizeof(uint16_t));
        if (FAILED(ib->Lock(0, bytes, &locked, 0)) || !locked) {
            Log("cockpit: %s: cached steering wheel index buffer lock failed; preparing the model again", name);
            continue;
        }
        memcpy(locked, cached.orderedIndices.data(), bytes);
        ib->Unlock();
        memcpy(indices, cached.orderedIndices.data(), bytes);
        Wheel* wheel = new Wheel(cached.wheel);
        int n = At<int>(plat, kPlatNumEntries);
        wheel->parts.assign(n * 2 * kEntrySize, 0);
        return wheel;
    }
    return nullptr;
}

void RememberWheelTemplate(uint8_t* solid, const std::string& name, const Wheel& wheel) {
    if (!wheel.ok) return;
    uint8_t* plat = At<uint8_t*>(solid, kSolidPlat);
    int n = plat ? At<int>(plat, kPlatNumEntries) : 0;
    int numIndices = plat ? At<int>(plat, kPlatNumIndices) : 0;
    uint8_t* entries = plat ? At<uint8_t*>(plat, kPlatEntries) : nullptr;
    uint16_t* indices = plat ? At<uint16_t*>(plat, kPlatIndices) : nullptr;
    if (!entries || !indices || n <= 0 || numIndices <= 0 || n != (int)wheel.turned.size()) return;

    WheelTemplate cached;
    cached.wheel = wheel;
    cached.wheel.parts.clear();
    cached.orderedIndices.assign(indices, indices + numIndices);
    cached.entryRanges.reserve(n);
    for (int i = 0; i < n; i++) {
        uint8_t* entry = entries + i * kEntrySize;
        cached.entryRanges.push_back({At<int>(entry, kEntryFirstIndex), At<int>(entry, kEntryTris)});
    }

    auto& variants = s_wheelTemplates[name];
    for (WheelTemplate& variant : variants) {
        if (variant.entryRanges == cached.entryRanges && variant.orderedIndices.size() == cached.orderedIndices.size()) {
            variant = std::move(cached);
            return;
        }
    }
    variants.push_back(std::move(cached));
}

// The car-model -> world matrix with the wheel turned by `degrees` (+ = clockwise, to the right,
// as the driver sees it) about the column: row vectors, so M' = T * M with T turning about the
// centre in model space. A right-handed turn about the column (toward the dash) lifts the left
// side of the wheel (+y) toward the top (+z): clockwise from the driver's seat.
void TurnedMatrix(const Wheel& w, float degrees, const float* m, float* out) {
    float a = degrees * 3.14159265f / 180.0f;
    float c = cosf(a), s = sinf(a), t = 1 - c;
    const float* k = w.axis;
    float R[3][3] = {   // column-vector rotation about k, transposed below for row vectors
        {c + k[0] * k[0] * t, k[0] * k[1] * t - k[2] * s, k[0] * k[2] * t + k[1] * s},
        {k[1] * k[0] * t + k[2] * s, c + k[1] * k[1] * t, k[1] * k[2] * t - k[0] * s},
        {k[2] * k[0] * t - k[1] * s, k[2] * k[1] * t + k[0] * s, c + k[2] * k[2] * t}};
    float T[4][4] = {};
    for (int r = 0; r < 3; r++)
        for (int col = 0; col < 3; col++) T[r][col] = R[col][r];
    // About the found centre moved by the car's pivot offset (right = -y, up in the wheel's plane).
    float upv[3] = {-k[2], 0, k[0]};
    float ul = sqrtf(upv[0] * upv[0] + upv[2] * upv[2]);
    if (ul > 1e-4f) upv[0] /= ul, upv[2] /= ul;
    float pivot[3] = {w.centre[0] + upv[0] * g_cfg.wheelPivot[1], w.centre[1] - g_cfg.wheelPivot[0],
                      w.centre[2] + upv[2] * g_cfg.wheelPivot[1]};
    for (int col = 0; col < 3; col++) {
        float cr = 0;
        for (int r = 0; r < 3; r++) cr += pivot[r] * T[r][col];
        // Translate after rotation: steering must not rotate the chosen location.
        float offset[3] = {0, -g_cfg.wheelPosition[0], g_cfg.wheelPosition[1]};
        T[3][col] = pivot[col] - cr + offset[col];
    }
    T[3][3] = 1;
    for (int r = 0; r < 4; r++)
        for (int col = 0; col < 4; col++) {
            float v = 0;
            for (int j = 0; j < 4; j++) v += T[r][j] * m[j * 4 + col];
            out[r * 4 + col] = v;
        }
}

uint32_t IndexHash(const uint16_t* indices, int count) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < count; i++) h = (h ^ indices[i]) * 16777619u;
    return h;
}

// The solid's current plat info, entries, index buffer and index copy (null where missing).
void ReadSolid(uint8_t* solid, SolidIdentity& s) {
    // The name is a C string, not a fixed-width identity blob. Bytes after its
    // terminator may change independently of the model (including adjacent data).
    // Canonicalize the unused tail, retaining all 64 bytes for a full-width name.
    memset(s.name, 0, sizeof(s.name));
    for (size_t i = 0; i < sizeof(s.name); ++i) {
        char c = (char)solid[kSolidName + i];
        if (!c) break;
        s.name[i] = c;
    }
    s.plat = At<uint8_t*>(solid, kSolidPlat);
    s.entries = s.plat ? At<uint8_t*>(s.plat, kPlatEntries) : nullptr;
    s.ib = s.plat ? At<void*>(s.plat, kPlatIndexBuffer) : nullptr;
    s.indices = s.plat ? At<uint16_t*>(s.plat, kPlatIndices) : nullptr;
}

uint32_t CurrentHash(const SolidIdentity& s) {
    int n = s.plat ? At<int>(s.plat, kPlatNumIndices) : 0;
    return s.indices && n > 0 && n < (1 << 20) ? IndexHash(s.indices, n) : 0;
}

const HeadPos* CarHead(const char* name) {
    const char* kit = strstr(name, "_KIT");
    auto it = s_carHead.find(std::string(name, kit ? kit - name : strlen(name)));
    return it != s_carHead.end() ? &it->second : nullptr;
}

void ApplyHides(uint8_t* solid, SolidInfo& info);
void RestoreAll(uint8_t* solid, SolidInfo& info);
// Bumped once a frame (Cockpit_EndFrame), in VR and on a flat screen alike.
int s_hashSerial = 0;

SolidInfo* GetSolid(uint8_t* solid) {
    TraceDetailScope cacheTiming(TRACE_SOLID_CACHE);
    SolidIdentity identity;
    ReadSolid(solid, identity);
    char name[65] = {};
    memcpy(name, identity.name, 64);
    auto it = s_solids.find(solid);
    if (it != s_solids.end()) {
        SolidInfo& was = it->second;
        bool same = !memcmp(was.name, identity.name, sizeof(identity.name)) && was.plat == identity.plat &&
                    was.entries == identity.entries && was.ib == identity.ib && was.indices == identity.indices;
        // Drivers and split mirror entries can be restored in place by streaming. Wheel geometry
        // stays cached; only watch its indices while the triangle editor is active, where edits
        // require an explicit refresh.
        bool watchIndices = (was.driver && was.driver->ok) || was.split ||
                            ((was.wheel && was.wheel->ok) && (g_cfg.editorOn > 0.5f || g_cfg.editPicking > 0.5f));
        if (same && watchIndices) {
            if (was.hashSeen == s_hashSerial)
                same = true;
            else {
                was.hashSeen = s_hashSerial;
                same = CurrentHash(identity) == was.indexHash;
            }
        }
        if (same) {
            // An interior seen before its car's driver: look for the wheel once the head is known.
            if (was.interior && !was.wheel && g_cfg.steeringWheel) {
                if (const HeadPos* head = CarHead(name)) {
                    RestoreAll(solid, was);
                    was.wheel = ReuseWheelTemplate(solid, name);
                    if (!was.wheel) {
                        was.wheel = PrepareWheel(solid, name, head->v);
                        if (was.wheel) RememberWheelTemplate(solid, name, *was.wheel);
                    }
                    was.indexHash = CurrentHash(identity);
                    ApplyHides(solid, was);
                }
            }
            return &was;
        }
    }
    // A cache hit needs no owning containers (MSVC's empty map allocates a
    // sentinel). Keep all identity and streaming checks above, without that cost.
    SolidInfo now;
    Trace_CacheRebuild();
    static_cast<SolidIdentity&>(now) = identity;
    if (it != s_solids.end()) {
        SolidInfo& was = it->second;
        bool sameModel = !memcmp(was.name, now.name, sizeof(now.name)) && was.indices == now.indices && was.plat == now.plat;
        if (was.driver || was.wheel)
            Log("cockpit: %.64s changed or reloaded, preparing it again: name=%d plat=%d entries=%d ib=%d indices=%d; old=%p/%p/%p/%p new=%p/%p/%p/%p (index check serial %d)",
                was.name, memcmp(was.name, now.name, sizeof(now.name)) != 0,
                was.plat != now.plat, was.entries != now.entries, was.ib != now.ib, was.indices != now.indices,
                was.plat, was.entries, was.ib, was.indices, now.plat, now.entries, now.ib, now.indices, s_hashSerial);
        if (was.driver) s_retired.push_back(was.driver);
        if (was.wheel) s_retiredWheels.push_back(was.wheel);
        if (was.split) s_retiredSplits.push_back(was.split);
        // Our zero-area triangles are put back first: the wheel and the gauges are found from whole
        // triangles (prepared over them, the gauges were lost: missing, and not pickable).
        if (sameModel && !was.hidden.empty()) {
            now.hidden = was.hidden;
            RestoreAll(solid, now);
        }
    }
    now.driver = strstr(name, "_DRIVER") ? Prepare(solid, name) : nullptr;
    now.interior = strstr(name, "_INTERIOR") != nullptr;
    if (now.interior && g_cfg.steeringWheel) {
        if (const HeadPos* head = CarHead(name)) {
            now.wheel = ReuseWheelTemplate(solid, name);
            if (!now.wheel) {
                now.wheel = PrepareWheel(solid, name, head->v);
                if (now.wheel) RememberWheelTemplate(solid, name, *now.wheel);
            }
        }
    }
    // Ordinary traffic/scenery (including newly spawned breakable pieces) never
    // compares an index hash. Do not walk their entire mesh on a cache miss.
    // Mirror splits establish their own hash in SplitMirrorEntries; wheels and
    // drivers still need the baseline for the existing reload checks above.
    if (now.driver || now.wheel) now.indexHash = CurrentHash(now);
    SolidInfo& stored = s_solids[solid];
    stored = std::move(now);
    ApplyHides(solid, stored);
    return &stored;
}

Driver* GetDriver(uint8_t* solid) { return GetSolid(solid)->driver; }

// Whether CarRenderConn::OnRender draws the car in `view` with the in-car camera's body matrix.
// Decided while the car is drawn: before ___eDisplayFrame the camera's anchor can still say
// otherwise (seen in the hood camera, which draws with the render matrix).
uint8_t* ViewAnchor(uint8_t* view) {
    uint8_t* list = view + kViewMovers;
    uint8_t* node = At<uint8_t*>(list, 0);
    if (node == list || !node) return nullptr;
    uint8_t* mover = node - 4;
    typedef uint8_t*(__thiscall * GetAnchor_t)(uint8_t*);
    return ((GetAnchor_t)(At<void**>(mover, 0)[kMoverGetAnchor]))(mover);
}

bool DrawsWithPov(uint8_t* conn, uint8_t* view) {
    uint8_t* anchor = ViewAnchor(view);
    return anchor && At<int>(anchor, kAnchorWorldId) == At<int>(conn, kConnWorldId) && At<int16_t>(anchor, kAnchorPov) == 1;
}

// The body matrix CarRenderConn::OnRender draws the car with: with `pov`, the body's own
// matrix moved by its centre of mass offset, otherwise the smoothed render matrix. Both are up
// to date before ___eDisplayFrame. Translation in row 3.
M4 BodyMatrix(uint8_t* conn, bool pov) {
    M4 m;
    memcpy(&m, conn + kConnRenderMatrix, sizeof(m));
    const M4* body = pov ? At<const M4*>(conn, kConnBodyMatrix) : nullptr;
    if (!body) return m;
    m = *body;
    const float* cog = &At<float>(conn, kConnCog);
    for (int a = 0; a < 3; a++)
        m.m[3][a] -= cog[0] * m.m[0][a] + cog[1] * m.m[1][a] + cog[2] * m.m[2][a] + cog[3] * m.m[3][a];
    return m;
}

void __fastcall hkOnRender(uint8_t* conn, void* edx, uint8_t* view, int reflection) {
    bool tracePlayer = false;
    if (Trace_On()) {
        uint8_t* anchor = ViewAnchor(view);
        tracePlayer = anchor && At<uint32_t>(anchor, kAnchorWorldId) == At<uint32_t>(conn, kConnWorldId);
    }
    TraceDetailScope carTiming(tracePlayer ? TRACE_PLAYER_CAR : TRACE_OTHER_CAR);
    uint8_t* prev = s_curConn;
    M4 prevBody = s_curBody;
    bool prevPov = s_curPov;
    bool prevPlayerCar = s_curPlayerCar;
    s_curConn = !reflection && (uintptr_t)view == kPlayerView ? conn : nullptr;
    s_curPlayerCar = false;
    if (s_curConn) {
        uint8_t* anchor = ViewAnchor(view);
        s_curPlayerCar = anchor && At<uint32_t>(anchor, kAnchorWorldId) == At<uint32_t>(conn, kConnWorldId);
        s_curPov = s_curPlayerCar && At<int16_t>(anchor, kAnchorPov) == 1;
        s_curBody = BodyMatrix(conn, s_curPov);
    }
    if (s_curConn && conn == s_seatedConn) {
        float d = 0;
        for (int i = 0; i < 4; i++) for (int j = 0; j < 3; j++) d = std::max(d, fabsf(s_curBody.m[i][j] - s_seatedBody.m[i][j]));
        s_bodyMismatch = std::max(s_bodyMismatch, d);
        s_bodyDraws++;
    }
    typedef void(__fastcall * OnRender_t)(uint8_t*, void*, uint8_t*, int);
    ((OnRender_t)kOnRender)(conn, edx, view, reflection);
    s_curConn = prev;
    s_curBody = prevBody;
    s_curPov = prevPov;
    s_curPlayerCar = prevPlayerCar;
}

// CarRenderInfo::Render skips a car in the player view when the camera is within 4 m of it and,
// lifted 0.5 m, inside its bounding box (bBoundingBoxIsInside(min, max, eye + 0.5 up, 0)): the
// game's way of not drawing your own car around a camera inside it. From the seat the eyes sit at
// the driver's head, so looking down or leaning brings them low enough for the lifted point to
// fall under the roof, and the whole car vanished (IS300, low roof). Then the driver wasn't drawn
// either and the seat dropped to the game camera. While seated the car is always drawn.
constexpr uintptr_t kCallCarCameraInside = 0x74E68C;
constexpr uintptr_t kBoxIsInside = 0x45FDE0;        // int bBoundingBoxIsInside(const bVector3* min, max, point, float), cdecl

int __cdecl hkCarCameraInside(const float* mn, const float* mx, const float* point, float extra) {
    if (s_seatedConn) {
        static bool logged;
        if (!logged) {
            logged = true;
            Log("cockpit: car kept drawn with the eyes inside it (seated)");
        }
        return 0;
    }
    return ((int(__cdecl*)(const float*, const float*, const float*, float))kBoxIsInside)(mn, mx, point, extra);
}

// Each car model (0x6DA9A2) and each of its draw entries (0x6DAB11) is frustum-culled before it is
// submitted. The frustum planes come from the player view, which while the depth split is on is set
// up with WorldNearZ (0.5 m), not the car's NearZ (0.091 m). A small entry whose whole box is in
// front of that far near plane is skipped even though the car's own draw matrices would show it: the
// merged driver fingers vanished as soon as the eyes came within WorldNearZ of the hand. When the eye
// is inside a box (or within a metre of it) that box counts as visible, exactly as the camera being
// inside a car should.
constexpr uintptr_t kPlatVisibleState = 0x6CF2B0;    // eViewPlatInterface::GetVisibleState, thiscall, 3 args
constexpr uintptr_t kCallsPartVisible[] = {0x6DA9A2, 0x6DAB11};

int __fastcall hkPartVisible(uint8_t* view, void*, const float* mn, const float* mx, const float* m) {
    int state = ((int(__thiscall*)(void*, const float*, const float*, const float*))kPlatVisibleState)(view, mn, mx, m);
    void* cam = view ? At<void*>(view, kViewCamera) : nullptr;
    if (state != 0 || !cam || !mn || !mx) return state;
    const float* eye = &At<float>(cam, kCamPosition);
    if (!m) {   // a box in world space
        for (int a = 0; a < 3; a++)
            if (eye[a] < mn[a] - 1.0f || eye[a] > mx[a] + 1.0f) return state;
    } else {
        // The eye in the box's own space (row vectors: p = local * m; the axes may carry a scale).
        float rel[3] = {eye[0] - m[12], eye[1] - m[13], eye[2] - m[14]};
        for (int a = 0; a < 3; a++) {
            const float* axis = m + a * 4;
            float len2 = axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2];
            if (len2 < 1e-8f) return state;
            float local = (rel[0] * axis[0] + rel[1] * axis[1] + rel[2] * axis[2]) / len2;
            float margin = 1.0f / sqrtf(len2);
            if (local < mn[a] - margin || local > mx[a] + margin) return state;
        }
    }
    static int kept;
    if (kept < 4) {
        ++kept;
        Log("cockpit: part kept visible with the eyes inside its box (view near plane)");
    }
    return 1;
}

float Dist(const float* a, const float* b) {
    float d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
    return sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
}

bool DrawingPlayerCar();

// The solid's vertices (first entry with vertex data) and index copy.
struct SolidMesh {
    uint8_t* verts = nullptr;
    int stride = 0, numVerts = 0, numIndices = 0, n = 0;
    uint8_t* entries = nullptr;
    uint16_t* indices = nullptr;
    IDirect3DIndexBuffer9* ib = nullptr;
    const float* pos(int v) const { return (const float*)(verts + v * stride); }
    V3 at(int v) const { const float* p = pos(v); return {p[0], p[1], p[2]}; }
};

bool ReadMesh(uint8_t* solid, SolidMesh& s) {
    uint8_t* plat = At<uint8_t*>(solid, kSolidPlat);
    s.n = plat ? At<int>(plat, kPlatNumEntries) : 0;
    s.entries = plat ? At<uint8_t*>(plat, kPlatEntries) : nullptr;
    s.indices = plat ? At<uint16_t*>(plat, kPlatIndices) : nullptr;
    s.ib = plat ? At<IDirect3DIndexBuffer9*>(plat, kPlatIndexBuffer) : nullptr;
    s.numIndices = plat ? At<int>(plat, kPlatNumIndices) : 0;
    if (s.n <= 0 || s.n > 64 || !s.entries || !s.indices || !s.ib || s.numIndices <= 0) return false;
    for (int i = 0; i < s.n && !s.verts; i++) {
        uint8_t* e = s.entries + i * kEntrySize;
        s.verts = At<uint8_t*>(e, kEntryVertices);
        uint8_t* fmt = At<uint8_t*>(e, kEntryFormat);
        s.stride = fmt ? At<int>(fmt, 8) : 0;
        s.numVerts = s.stride > 0 ? At<int>(e, kEntryVertexBytes) / s.stride : 0;
    }
    return s.verts && s.stride >= 12 && s.stride <= 128 && s.numVerts > 0;
}

// --- side mirrors ------------------------------------------------------------------------------
// The solids whose rear view mirror was handed to the d3d9 side in the current eye.
std::vector<uint8_t*> s_mirrorSolids;
std::vector<uint8_t*> s_manualMirrorSolids;
// The player car's window/glass solids in the current eye. They are drawn with culling off so the
// outside surface is also visible from inside the cabin.
std::unordered_set<void*> s_windowIbs;
std::unordered_set<uint8_t*> s_windowOverlaySolids;

bool WindowDraw(void* indexBuffer) {
    return indexBuffer && s_windowIbs.count(indexBuffer) != 0;
}

// The player car's wiper solid per index buffer (registered while submitting the car).
std::unordered_map<void*, uint8_t*> s_wiperSolids;

// Read by the gameplay mod through NfsmwVr_Setting("WiperSweep" / "WiperStrike"): the sweep angle
// and a count of wiper strokes. The count drives the stitch-static wiper sounds (defined after the
// anonymous namespace below).
static int s_wiperHalf = -1;


// All of a solid's triangles (three points each), for which `keep` is true.
template <class Keep> void SolidTriangles(uint8_t* solid, std::vector<V3>& out, Keep keep) {
    SolidMesh m;
    if (!ReadMesh(solid, m)) return;
    for (int i = 0; i < m.n; i++) {
        uint8_t* e = m.entries + i * kEntrySize;
        int tris = At<int>(e, kEntryTris), first = At<int>(e, kEntryFirstIndex);
        if (tris <= 0 || first < 0 || first + tris * 3 > m.numIndices) continue;
        for (int t = 0; t < tris; t++) {
            const uint16_t* tri = m.indices + first + t * 3;
            // Editor hides collapse indices in the live mesh. Detection must still see the
            // original geometry, otherwise hiding the glass also removes our mirror anchor.
            auto found = s_solids.find(solid);
            if (tri[0] != tri[1] && tri[1] == tri[2] && found != s_solids.end()) {
                for (const auto& hidden : found->second.hidden)
                    if (hidden.second[0] == tri[0] && hidden.second[1] == tri[1]) {
                        tri = hidden.second.data();
                        break;
                    }
            }
            if (tri[0] == tri[1] || tri[1] == tri[2] || tri[0] == tri[2]) continue;
            if (tri[0] >= m.numVerts || tri[1] >= m.numVerts || tri[2] >= m.numVerts) continue;
            V3 p[3] = {m.at(tri[0]), m.at(tri[1]), m.at(tri[2])};
            if (keep(p)) out.insert(out.end(), p, p + 3);
        }
    }
}

// Whether a triangle (vertex indices) lies on the glass: in its plane (1.2 cm) and within its
// outline (`margin`, m).
bool OnGlass(const SolidMesh& m, const MirrorGlass& g, const uint16_t* tri, float margin = 0.01f) {
    float d = mirror::Dot(g.centre, g.normal), cu = mirror::Dot(g.centre, g.u), cv = mirror::Dot(g.centre, g.v);
    for (int k = 0; k < 3; k++) {
        if (tri[k] >= m.numVerts) return false;
        V3 q = m.at(tri[k]);
        if (!(fabsf(mirror::Dot(q, g.normal) - d) < 0.012f && fabsf(mirror::Dot(q, g.u) - cu) < g.halfU + margin &&
              fabsf(mirror::Dot(q, g.v) - cv) < g.halfV + margin))
            return false;
    }
    return true;
}

// User-picked stock Carrera glass, sorted vertex triples. Do not apply to other kits or LODs.
int CarreraSide(const char* name) {
    if (!strcmp(name, "CARRERAGT_KIT00_LEFT_SIDE_MIRROR_A")) return 0;
    if (!strcmp(name, "CARRERAGT_KIT00_RIGHT_SIDE_MIRROR_A")) return 1;
    return -1;
}
bool CarreraTriangle(int side, const uint16_t* tri) {
    static const uint16_t ids[2][18][3] = {
        {{252,253,254},{252,254,271},{254,270,271},{255,256,257},{255,257,258},{257,258,259},
         {258,259,260},{259,260,261},{260,261,262},{261,262,263},{262,263,264},{263,264,265},
         {264,265,266},{265,266,267},{266,267,268},{267,268,269},{268,269,270},{269,270,271}},
        {{252,253,254},{252,254,255},{252,255,271},{255,270,271},{256,257,258},{256,258,259},
         {256,259,260},{259,260,261},{260,261,262},{261,262,263},{262,263,264},{263,264,265},
         {264,265,266},{265,266,267},{266,267,268},{267,268,269},{268,269,270},{269,270,271}}
    };
    if (side < 0 || side > 1) return false;
    uint16_t sorted[3] = {tri[0], tri[1], tri[2]};
    std::sort(sorted, sorted + 3);
    for (const auto& id : ids[side])
        if (std::equal(sorted, sorted + 3, id)) return true;
    return false;
}

bool CamaroTriangle(int side, const uint16_t* tri) {
    static const uint16_t ids[2][14][3] = {
        {{9164,9165,9166},{9164,9166,9178},{9164,9178,9179},{9166,9167,9168},
         {9166,9168,9170},{9166,9170,9174},{9166,9174,9178},{9168,9169,9170},
         {9170,9171,9172},{9170,9172,9174},{9172,9173,9174},{9174,9175,9176},
         {9174,9176,9178},{9176,9177,9178}},
        {
        {7003,7004,7005},{7003,7005,7017},{7003,7017,7018},{7005,7006,7007},
        {7005,7007,7009},{7005,7009,7017},{7007,7008,7009},{7009,7010,7011},
        {7009,7011,7013},{7009,7013,7017},{7011,7012,7013},{7013,7014,7015},
        {7013,7015,7017},{7015,7016,7017}}
    };
    if (side < 0 || side > 1) return false;
    uint16_t sorted[3] = {tri[0], tri[1], tri[2]};
    std::sort(sorted, sorted + 3);
    for (const auto& id : ids[side]) if (std::equal(sorted, sorted + 3, id)) return true;
    return false;
}

// Require the complete distinct selection before using a model-specific override.
bool PickedGlass(const SolidMesh& m, int side, bool camaro, MirrorGlass& g) {
    std::vector<std::vector<uint16_t>> found;
    std::vector<V3> points;
    for (int i = 0; i < m.n; i++) {
        uint8_t* e = m.entries + i * kEntrySize;
        int count = At<int>(e, kEntryTris), first = At<int>(e, kEntryFirstIndex);
        if (count <= 0 || first < 0 || count > (m.numIndices - first) / 3) continue;
        for (int t = 0; t < count; t++) {
            const uint16_t* tri = m.indices + first + 3 * t;
            if (!(camaro ? CamaroTriangle(side, tri) : CarreraTriangle(side, tri)) || tri[0] >= m.numVerts || tri[1] >= m.numVerts || tri[2] >= m.numVerts) continue;
            std::vector<uint16_t> key(tri, tri + 3);
            std::sort(key.begin(), key.end());
            if (std::find(found.begin(), found.end(), key) != found.end()) continue;
            found.push_back(key);
            for (int k = 0; k < 3; k++) points.push_back(m.at(tri[k]));
        }
    }
    if (found.size() != (camaro ? 14u : 18u) || !g.ok) return false;
    float minU = 1e9f, maxU = -1e9f, minV = 1e9f, maxV = -1e9f;
    for (V3 p : points) {
        float u = mirror::Dot(p, g.u), v = mirror::Dot(p, g.v);
        minU = std::min(minU, u); maxU = std::max(maxU, u);
        minV = std::min(minV, v); maxV = std::max(maxV, v);
    }
    float du = (minU + maxU) * 0.5f - mirror::Dot(g.centre, g.u);
    float dv = (minV + maxV) * 0.5f - mirror::Dot(g.centre, g.v);
    g.centre = {g.centre.x + g.u.x * du + g.v.x * dv, g.centre.y + g.u.y * du + g.v.y * dv,
                g.centre.z + g.u.z * du + g.v.z * dv};
    g.halfU = (maxU - minU) * 0.5f; g.halfV = (maxV - minV) * 0.5f;
    g.tris = points;
    return true;
}

// The part of the rear view (u from, to) a side mirror shows: [VR] SideMirrorWidth of its width
// from that side. The rear-facing camera's right is the car's left, so the left
// mirror needs the HIGH-u slice. MirrorFlip changes orientation within that slice,
// not which side of the car is shown.
void SideMirrorRange(int side, float* u0, float* u1) {
    float w = g_cfg.sideMirrorWidth;
    *u0 = side == 0 ? 1.0f - w : 0.0f;
    *u1 = *u0 + w;
    if (g_cfg.mirrorFlip) std::swap(*u0, *u1);
}

std::string SolidName(const SolidInfo& info);
void BuildTris(uint8_t* solid, SolidInfo& info);
bool BuildMarkedMirrorVertices(const SolidInfo& info, const EditSet& edits, const float* matrix, int side,
                              std::vector<float>& out);
void NoteMarkedMirrors(uint8_t* solid, SolidInfo& info, const float* m, const EditSet& edits);
// A tint over the player car's windows so the interior glass reads as glass from the seat; its
// opacity is Config::glassOpacity (0 = off).
void NoteWindowOverlay(uint8_t* solid, SolidInfo& info, const float* m);

// A car listed in [VR] GameMirrorsSkip (model names such as BMWM3GTRE46, any case): its side
// mirrors stay the game's.
bool SkipGameMirrors(const std::string& car) {
    char list[sizeof(g_cfg.gameMirrorsSkip)];
    strcpy(list, g_cfg.gameMirrorsSkip);
    for (char* tok = strtok(list, ", ;"); tok; tok = strtok(nullptr, ", ;"))
        if (_stricmp(tok, car.c_str()) == 0) return true;
    return false;
}

// Side mirrors: the game draws their glass as its own draw entry (the MIRROR material). That entry
// is handed to the d3d9 side, which gives its vertices texture coordinates across the glass and
// draws it with the rear view as its texture: the game's own glass, showing the rear view.
std::unordered_map<std::string, bool> s_hasMirrorSolids;

std::string CarOf(const char* name) {
    const char* kit = strstr(name, "_KIT");
    return std::string(name, kit ? kit - name : strlen(name));
}

void RegisterGlassEntry(uint8_t* solid, SolidInfo& info, int side) {
    char name[65] = {};
    memcpy(name, info.name, 64);
    if (info.mirrorKind == 2 && s_hasMirrorSolids.count(CarOf(name))) return;
    const MirrorGlass& g = info.glass[side];
    SolidMesh m;
    if (!ReadMesh(solid, m)) return;
    void* ib = m.ib;
    // The entry made (almost) only of the glass.
    for (int i = 0; i < m.n; i++) {
        uint8_t* e = m.entries + i * kEntrySize;
        int tris = At<int>(e, kEntryTris), first = At<int>(e, kEntryFirstIndex);
        if (tris < 2 || first < 0 || first + tris * 3 > m.numIndices) continue;
        int on = 0;
        // 2.5 cm past the glass found: on the Supra's right mirror that is only part of the glass
        // (its triangles are not quite in one plane), and the glass's own entries reach beyond it.
        for (int t = 0; t < tris; t++) on += OnGlass(m, g, m.indices + first + t * 3, 0.025f);
        if (on * 10 < tris * 8) continue;
        // The glass's own coordinates, 0..1 across and down: what part of the rear view it shows
        // is worked out each eye (NoteMirrors), as a mirror would show it. Spanned over this entry's
        // own extent, so a glass found smaller than its entry still gets the whole picture.
        float cu = mirror::Dot(g.centre, g.u), cv = mirror::Dot(g.centre, g.v);
        float minU = cu - g.halfU, maxU = cu + g.halfU, minV = cv - g.halfV, maxV = cv + g.halfV;
        for (int t = 0; t < tris * 3; t++) {
            if (m.indices[first + t] >= m.numVerts) continue;
            V3 q = m.at(m.indices[first + t]);
            float pu = mirror::Dot(q, g.u), pv = mirror::Dot(q, g.v);
            minU = std::min(minU, pu), maxU = std::max(maxU, pu), minV = std::min(minV, pv), maxV = std::max(maxV, pv);
        }
        float du = 0.5f * (minU + maxU) - cu, dv = 0.5f * (minV + maxV) - cv;
        V3 c = {g.centre.x + g.u.x * du + g.v.x * dv, g.centre.y + g.u.y * du + g.v.y * dv, g.centre.z + g.u.z * du + g.v.z * dv};
        const float basis[14] = {c.x, c.y, c.z, g.u.x, g.u.y, g.u.z, g.v.x, g.v.y, g.v.z,
                                 0.5f * (maxU - minU), 0.5f * (maxV - minV), 0.0f, 1.0f, 0};
        info.glassIb[side] = ib;
        info.glassFirsts[side].push_back(first);
        std::vector<uint16_t> used(m.indices + first, m.indices + first + tris * 3);
        std::sort(used.begin(), used.end());
        used.erase(std::unique(used.begin(), used.end()), used.end());
        D3D_RegisterMirrorGlass(ib, first, tris, basis, used.data(), (int)used.size());
        Log("mirrors: %s: %s glass is draw entry %d (%d triangles)", name, side ? "right" : "left", i, tris);
        // Every entry on the glass: some cars lay a second one (the old glass texture) over it.
    }
    if (info.glassFirsts[side].empty()) Log("mirrors: %s: %s glass has no draw entry of its own", name, side ? "right" : "left");
}

// Both side mirrors' glass in one draw entry of their own (one material, as on the BMW M3 GTR's
// body): that entry gets both, each glass's vertices that side's slice of the rear view
// (SideMirrorWidth) baked in. Without one, every entry touching the glass is logged.
bool RegisterSharedGlass(uint8_t* solid, SolidInfo& info) {
    char name[65] = {};
    memcpy(name, info.name, 64);
    SolidMesh m;
    if (!ReadMesh(solid, m)) return false;
    void* ib = m.ib;
    bool found = false;
    for (int i = 0; i < m.n && !found; i++) {
        uint8_t* e = m.entries + i * kEntrySize;
        int tris = At<int>(e, kEntryTris), first = At<int>(e, kEntryFirstIndex);
        if (tris < 2 || first < 0 || first + tris * 3 > m.numIndices) continue;
        std::vector<uint16_t> used[2];
        int on[2] = {0, 0};
        for (int t = 0; t < tris; t++) {
            const uint16_t* tri = m.indices + first + t * 3;
            for (int side = 0; side < 2; side++)
                if (OnGlass(m, info.glass[side], tri)) {
                    on[side]++;
                    used[side].insert(used[side].end(), tri, tri + 3);
                    break;
                }
        }
        if (on[0] + on[1] > 0)
            Log("mirrors: %s: draw entry %d (%d triangles) has %d left and %d right glass triangles", name, i, tris, on[0], on[1]);
        if (!on[0] || !on[1] || (on[0] + on[1]) * 10 < tris * 8) continue;
        for (int side = 0; side < 2; side++) {
            const MirrorGlass& g = info.glass[side];
            float u0, u1;
            SideMirrorRange(side, &u0, &u1);
            const float basis[14] = {g.centre.x, g.centre.y, g.centre.z, g.u.x, g.u.y, g.u.z, g.v.x, g.v.y, g.v.z,
                                     g.halfU, g.halfV, u0, u1, 0};
            std::sort(used[side].begin(), used[side].end());
            used[side].erase(std::unique(used[side].begin(), used[side].end()), used[side].end());
            if (side == 0) D3D_RegisterMirrorGlass(ib, first, tris, basis, used[side].data(), (int)used[side].size());
            else D3D_AddMirrorGlassPart(ib, first, basis, used[side].data(), (int)used[side].size());
        }
        Log("mirrors: %s: both side mirrors' glass is draw entry %d (%d triangles)", name, i, tris);
        found = true;
    }
    return found;
}

// Glass without an entry of its own: split out of the entries it shares (GlassSplit).
void SplitSharedGlass(uint8_t* solid, SolidInfo& info, const bool need[2], int carrera = -1, int camaro = 0) {
    char name[65] = {};
    memcpy(name, info.name, 64);
    SolidMesh m;
    if (info.split || !ReadMesh(solid, m)) return;
    IDirect3DIndexBuffer9* ib = m.ib;
    uint16_t* indices = m.indices;
    D3DINDEXBUFFER_DESC desc;
    if (FAILED(ib->GetDesc(&desc)) || desc.Format != D3DFMT_INDEX16) return;
    GlassSplit* split = new GlassSplit;
    for (int i = 0; i < m.n; i++) {
        uint8_t* e = m.entries + i * kEntrySize;
        int tris = At<int>(e, kEntryTris), first = At<int>(e, kEntryFirstIndex);
        if (tris < 2 || first < 0 || first + tris * 3 > m.numIndices) continue;
        std::vector<uint16_t> part[3];
        const uint16_t* idx = indices + first;
        for (int t = 0; t < tris; t++) {
            const uint16_t* tri = idx + t * 3;
            int cls = 0;
            for (int side = 0; side < 2 && !cls; side++)
                if (need[side] && info.glass[side].ok &&
                    (carrera >= 0 ? side == carrera && CarreraTriangle(carrera, tri) :
                     (camaro & (1 << side)) ? CamaroTriangle(side, tri) : OnGlass(m, info.glass[side], tri))) cls = side + 1;
            part[cls].insert(part[cls].end(), tri, tri + 3);
        }
        // A few stray triangles are not a mirror's glass.
        for (int side = 1; side < 3; side++)
            if (part[side].size() < 6) {
                part[0].insert(part[0].end(), part[side].begin(), part[side].end());
                part[side].clear();
            }
        if (part[1].empty() && part[2].empty()) continue;
        std::vector<uint16_t> order = part[0];
        order.insert(order.end(), part[1].begin(), part[1].end());
        order.insert(order.end(), part[2].begin(), part[2].end());
        void* p = nullptr;
        if (FAILED(ib->Lock(first * 2, tris * 6, &p, 0)) || !p) {
            Log("mirrors: %s: index buffer lock failed", name);
            continue;
        }
        memcpy(p, order.data(), tris * 6);
        ib->Unlock();
        memcpy(indices + first, order.data(), tris * 6);
        GlassSplit::Entry se = {i, {(int)part[0].size() / 3, (int)part[1].size() / 3, (int)part[2].size() / 3}};
        split->entries.push_back(se);
        // Vertices the glass shares with the rest get the glass's coordinates too (noted).
        std::vector<uint16_t> rest(part[0]);
        std::sort(rest.begin(), rest.end());
        int offset = se.count[0];
        for (int side = 0; side < 2; side++) {
            const std::vector<uint16_t>& tri = part[side + 1];
            if (tri.empty()) continue;
            std::vector<uint16_t> used(tri);
            std::sort(used.begin(), used.end());
            used.erase(std::unique(used.begin(), used.end()), used.end());
            int shared = 0;
            for (uint16_t v : used) shared += std::binary_search(rest.begin(), rest.end(), v);
            const MirrorGlass& g = info.glass[side];
            const float basis[14] = {g.centre.x, g.centre.y, g.centre.z, g.u.x, g.u.y, g.u.z, g.v.x, g.v.y, g.v.z,
                                     g.halfU, g.halfV, 0.0f, 1.0f, 0};
            D3D_RegisterMirrorGlass(ib, first + offset * 3, (int)tri.size() / 3, basis, used.data(), (int)used.size());
            info.glassIb[side] = ib;
            info.glassFirsts[side].push_back(first + offset * 3);
            Log("mirrors: %s: %s glass split out of draw entry %d (%d of %d triangles, %d vertices shared)", name,
                side ? "right" : "left", i, (int)tri.size() / 3, tris, shared);
            offset += (int)tri.size() / 3;
        }
    }
    if (split->entries.empty()) {
        delete split;
        return;
    }
    split->parts.assign(split->entries.size() * 3 * kEntrySize, 0);
    info.split = split;
    info.indexHash = CurrentHash(info);
}


// Finds the rear view mirror's glass in the player car's interior the first time it is drawn,
// once its driver's head is known (as for the steering wheel), and the side mirrors' glass.
void FindMirrors(uint8_t* solid, SolidInfo& info) {
    char name[65] = {};
    memcpy(name, info.name, 64);
    int carrera = -1;
    int camaro = 0;
    if (info.interior) {
        const HeadPos* head = CarHead(name);
        if (!head) return;
        std::vector<V3> tris;
        const float* h = head->v;
        SolidTriangles(solid, tris, [h](const V3* p) { return mirror::InRearViewRegion(p, h); });
        info.glass[2] = mirror::FindRearViewGlass(tris);
        info.mirrorKind = 3;
        const MirrorGlass& g = info.glass[2];
        if (g.ok)
            Log("mirrors: %s: rear view mirror at %.2f %.2f %.2f, %.3f x %.3f, facing %.2f %.2f %.2f", name, g.centre.x,
                g.centre.y, g.centre.z, 2 * g.halfU, 2 * g.halfV, g.normal.x, g.normal.y, g.normal.z);
        else
            Log("mirrors: %s: no rear view mirror found", name);
        return;
    }
    if (!EditsFor(name).mirror.empty()) {
        info.mirrorKind = 4;
        return;
    }
    info.mirrorKind = 0;
    bool left = strstr(name, "_LEFT_SIDE_MIRROR") != nullptr, right = strstr(name, "_RIGHT_SIDE_MIRROR") != nullptr;
    if (left || right) {
        std::vector<V3> tris;
        SolidTriangles(solid, tris, [](const V3*) { return true; });
        info.glass[left ? 0 : 1] = mirror::FindGlass(tris);
        int side = CarreraSide(name);
        SolidMesh m;
        if (side >= 0 && ReadMesh(solid, m) && PickedGlass(m, side, false, info.glass[side])) {
            carrera = side;
            Log("mirrors: %s: using all 18 user-picked glass triangles", name);
        }
        info.mirrorKind = 1;
        if (info.glass[left ? 0 : 1].ok) s_hasMirrorSolids[CarOf(name)] = true;
    } else if (strstr(name, "_KIT") && strstr(name, "_BODY_")) {
        // Mirrors built into the body sit beside the driver: from 0.2 m behind to 1 m ahead of the
        // head. Further ahead are hood vents (the Viper's KIT04 body). Looked at once the head is known.
        const HeadPos* head = CarHead(name);
        if (!head) {
            info.mirrorKind = -1;
            return;
        }
        float hx = head->v[0];
        for (int side = 0; side < 2; side++) {
            std::vector<V3> tris;
            float s = side == 0 ? 1.0f : -1.0f;
            SolidTriangles(solid, tris, [s, hx](const V3* p) {
                if (!mirror::InBodyMirrorRegion(p, s)) return false;
                for (int k = 0; k < 3; k++)
                    if (p[k].x < hx - 0.2f || p[k].x > hx + 1.0f) return false;
                return true;
            });
            info.glass[side] = mirror::FindGlass(tris, false);
        }
        SolidMesh m;
        if (!strcmp(name, "CAMARO_KIT00_BODY_A") && ReadMesh(solid, m))
            for (int side = 0; side < 2; side++)
                if (PickedGlass(m, side, true, info.glass[side])) {
                    camaro |= 1 << side;
                    Log("mirrors: %s: using all 14 user-picked %s glass triangles", name, side ? "right" : "left");
                }
        info.mirrorKind = 2;
    }
    for (int side = 0; side < 2; side++) {
        const MirrorGlass& g = info.glass[side];
        if (g.ok)
            Log("mirrors: %s: %s glass at %.2f %.2f %.2f, %.3f x %.3f, facing %.2f %.2f %.2f", name, side ? "right" : "left",
                g.centre.x, g.centre.y, g.centre.z, 2 * g.halfU, 2 * g.halfV, g.normal.x, g.normal.y, g.normal.z);
    }
    if (info.mirrorKind == 1 && !info.glass[0].ok && !info.glass[1].ok) Log("mirrors: %s: no glass found", name);
    for (int side = 0; side < 2; side++)
        if (carrera < 0 && !(camaro & (1 << side)) && info.glass[side].ok && g_cfg.gameMirrors && !SkipGameMirrors(CarOf(name))) RegisterGlassEntry(solid, info, side);
    if (g_cfg.gameMirrors && !SkipGameMirrors(CarOf(name))) {
        const bool need[2] = {info.glass[0].ok && info.glassFirsts[0].empty(),
                              info.glass[1].ok && info.glassFirsts[1].empty()};
        if (need[0] && need[1]) {
            if (camaro || !RegisterSharedGlass(solid, info)) SplitSharedGlass(solid, info, need, carrera, camaro);
        } else if (need[0] || need[1]) {
            SplitSharedGlass(solid, info, need, carrera, camaro);
        }
    }
}

// The glass's outline in its own plane, across and up (u, v): the convex hull of the patch's
// points. Some glass is a faceted shell whose triangles overlap, fold or leave gaps (the Eclipse's
// in the back of its housing), and drawing those raw made a broken shape; fanned from the hull
// they make one clean patch in the glass's plane.
void GlassOutline(const MirrorGlass& g, std::vector<std::pair<float, float>>& hull) {
    hull.clear();
    std::vector<std::pair<float, float>> pts;
    pts.reserve(g.tris.size());
    for (const V3& p : g.tris) pts.push_back({mirror::Dot(p, g.u), mirror::Dot(p, g.v)});
    std::sort(pts.begin(), pts.end());
    pts.erase(std::unique(pts.begin(), pts.end()), pts.end());
    if (pts.size() < 3) return;
    auto cross = [](const std::pair<float, float>& o, const std::pair<float, float>& a, const std::pair<float, float>& b) {
        return (a.first - o.first) * (b.second - o.second) - (a.second - o.second) * (b.first - o.first);
    };
    std::vector<std::pair<float, float>> h(2 * pts.size());
    size_t k = 0;
    for (size_t i = 0; i < pts.size(); i++) {
        while (k >= 2 && cross(h[k - 2], h[k - 1], pts[i]) <= 0) k--;
        h[k++] = pts[i];
    }
    for (size_t i = pts.size() - 1, t = k + 1; i > 0; i--) {
        while (k >= t && cross(h[k - 2], h[k - 1], pts[i - 1]) <= 0) k--;
        h[k++] = pts[i - 1];
    }
    if (k < 4) return;
    h.resize(k - 1);
    float twiceArea = 0;
    for (size_t i = 0; i < h.size(); i++)
        twiceArea += h[i].first * h[(i + 1) % h.size()].second - h[(i + 1) % h.size()].first * h[i].second;
    if (fabsf(twiceArea) < 1e-7f) return;
    hull.swap(h);
}

// The glass's triangles in world space, `lift` metres toward the driver and shrunk by `scale`
// about the glass's centre, with their place in the glass (0..1 across, 0..1 down) as texture
// coordinates: x, y, z, u, v per vertex.
void GlassVertices(const MirrorGlass& g, const M4& m, std::vector<float>& out, float lift = 0.008f, float scale = 1.0f,
                   bool symmetric = false) {
    float cu = mirror::Dot(g.centre, g.u), cv = mirror::Dot(g.centre, g.v), cd = mirror::Dot(g.centre, g.normal);
    out.clear();
    std::vector<std::pair<float, float>> hull;
    GlassOutline(g, hull);
    if (symmetric && hull.size() >= 3) {
        // Intersect the outline with its horizontal reflection. Unlike a union, this cannot
        // extend the overlay beyond the detected glass and into the housing.
        auto original = hull;
        std::vector<std::pair<float, float>> reflected;
        for (auto it = original.rbegin(); it != original.rend(); ++it)
            reflected.push_back({2 * cu - it->first, it->second});
        for (size_t edge = 0; edge < reflected.size() && !hull.empty(); edge++) {
            auto a = reflected[edge], b = reflected[(edge + 1) % reflected.size()];
            auto side = [a, b](std::pair<float, float> p) {
                return (b.first - a.first) * (p.second - a.second) - (b.second - a.second) * (p.first - a.first);
            };
            std::vector<std::pair<float, float>> clipped;
            for (size_t i = 0; i < hull.size(); i++) {
                auto p = hull[i], q = hull[(i + 1) % hull.size()];
                float dp = side(p), dq = side(q);
                if (dp >= 0) clipped.push_back(p);
                if ((dp < 0 && dq > 0) || (dp > 0 && dq < 0)) {
                    float t = dp / (dp - dq);
                    clipped.push_back({p.first + t * (q.first - p.first), p.second + t * (q.second - p.second)});
                }
            }
            hull.swap(clipped);
        }
        float area = 0;
        for (size_t i = 0; i < hull.size(); i++)
            area += hull[i].first * hull[(i + 1) % hull.size()].second - hull[(i + 1) % hull.size()].first * hull[i].second;
        if (hull.size() < 3 || fabsf(area) < 1e-7f) hull.swap(original);
    }
    for (size_t i = 1; i + 1 < hull.size(); i++) {
        const std::pair<float, float>* fan[3] = {&hull[0], &hull[i], &hull[i + 1]};
        for (const std::pair<float, float>* at : fan) {
            // In the glass's plane, from its place across and up.
            V3 p0 = {g.u.x * at->first + g.v.x * at->second + g.normal.x * cd,
                     g.u.y * at->first + g.v.y * at->second + g.normal.y * cd,
                     g.u.z * at->first + g.v.z * at->second + g.normal.z * cd};
            V3 p = {g.centre.x + (p0.x - g.centre.x) * scale, g.centre.y + (p0.y - g.centre.y) * scale,
                    g.centre.z + (p0.z - g.centre.z) * scale};
            V3 q = {p.x + g.normal.x * lift, p.y + g.normal.y * lift, p.z + g.normal.z * lift};
            for (int a = 0; a < 3; a++) out.push_back(q.x * m.m[0][a] + q.y * m.m[1][a] + q.z * m.m[2][a] + m.m[3][a]);
            out.push_back(g.halfU > 0 ? (mirror::Dot(p, g.u) - (cu - g.halfU)) / (2 * g.halfU) : 0.0f);
            out.push_back(g.halfV > 0 ? ((cv + g.halfV) - mirror::Dot(p, g.v)) / (2 * g.halfV) : 0.0f);
        }
    }
}

// A side mirror: a fixed slice of the rear view (SideMirrorRange), its full height.
void SideMirrorView(const SolidInfo& info, int side) {
    float u0, u1;
    SideMirrorRange(side, &u0, &u1);
    const float uv[8] = {u0, 0.0f, u1, 0.0f, u0, 1.0f, u1, 1.0f};
    for (int first : info.glassFirsts[side]) D3D_SetGlassCorners(info.glassIb[side], first, uv);
}

void NoteMirrors(uint8_t* solid, SolidInfo& info, const float* m) {
    if (info.mirrorKind < 0) FindMirrors(solid, info);
    char solidName[65] = {};
    memcpy(solidName, info.name, 64);
    const EditSet& edits = EditsFor(solidName);
    if (!info.interior && edits.mirror.empty()) {
        // Mirror marks cleared: stop hiding the glass in the player's private draw copy.
        void* ib = At<void*>(At<uint8_t*>(solid, kSolidPlat), kPlatIndexBuffer);
        D3D_SetManualMirrorTriangles(ib, nullptr, 0, 0);
    }
    if (!info.interior && !edits.mirror.empty()) {
        NoteMarkedMirrors(solid, info, m, edits);
        return;
    }
    if (info.mirrorKind <= 0) return;
    for (int side = 0; side < 2; side++)
        if (!info.glassFirsts[side].empty() && info.glass[side].ok) SideMirrorView(info, side);
    // The rear view mirror inside (the side mirrors are drawn by the game, RegisterGlassEntry): once
    // per solid and eye.
    if (info.glass[2].ok && std::find(s_mirrorSolids.begin(), s_mirrorSolids.end(), solid) == s_mirrorSolids.end()) {
        s_mirrorSolids.push_back(solid);
        const MirrorGlass& glass = info.glass[2];
        M4 mat;
        memcpy(&mat, m, sizeof(mat));
        // To be drawn with the model: a dark rim the shape of the glass, and the rear view on the
        // glass shrunk inside it, as if set into the housing.
        std::vector<float> verts, frame;
        float inner = std::min(std::max(g_cfg.mirrorScale, 0.5f), 1.0f);
        // MirrorOffset (per car, F4 > Menu and testing) moves it toward the driver or back into the glass.
        char name[65] = {};
        memcpy(name, info.name, 64);
        bool symmetric = CarOf(name) == "PUNTO";
        GlassVertices(glass, mat, frame, 0.006f + g_cfg.mirrorOffset, 1.0f, symmetric);
        GlassVertices(glass, mat, verts, 0.010f + g_cfg.mirrorOffset, inner, symmetric);
        if (g_cfg.mirrorFlip)
            for (size_t i = 3; i < verts.size(); i += 5) verts[i] = 1.0f - verts[i];
        uint8_t* plat = At<uint8_t*>(solid, kSolidPlat);
        void* ib = plat ? At<void*>(plat, kPlatIndexBuffer) : nullptr;
        D3D_AddMirror(ib, verts.data(), (int)(verts.size() / 5), inner < 0.999f ? frame.data() : nullptr,
                      inner < 0.999f ? (int)(frame.size() / 5) : 0);
    }
}

V3 ToWorld(const V3& p, const float* m);

void NoteWindowOverlay(uint8_t* solid, SolidInfo& info, const float* m) {
    if (!s_windowOverlaySolids.insert(solid).second) return;
    SolidMesh mesh;
    if (!ReadMesh(solid, mesh)) return;
    void* ib = At<void*>(At<uint8_t*>(solid, kSolidPlat), kPlatIndexBuffer);
    if (!ib) return;
    std::vector<float> xyz, uv;
    for (int i = 0; i < mesh.n; i++) {
        uint8_t* e = mesh.entries + i * kEntrySize;
        int tris = At<int>(e, kEntryTris), first = At<int>(e, kEntryFirstIndex);
        if (tris <= 0 || first < 0 || first + tris * 3 > mesh.numIndices) continue;
        for (int t = 0; t < tris; t++) {
            const uint16_t* tri = mesh.indices + first + t * 3;
            if (tri[0] >= mesh.numVerts || tri[1] >= mesh.numVerts || tri[2] >= mesh.numVerts) continue;
            if (info.hidden.count(TriId(tri[0], tri[1], tri[2]))) continue;
            V3 p0 = mesh.at(tri[0]), p1 = mesh.at(tri[1]), p2 = mesh.at(tri[2]);
            V3 n = Cross(Sub(p1, p0), Sub(p2, p0));
            float ax = fabsf(n.x), ay = fabsf(n.y), az = fabsf(n.z);
            for (int k = 0; k < 3; k++) {
                V3 p = mesh.at(tri[k]);
                V3 w = ToWorld(p, m);
                xyz.push_back(w.x);
                xyz.push_back(w.y);
                xyz.push_back(w.z);
                // Model space keeps the droplet pattern stuck to the glass; the largest normal
                // axis picks the two coordinates that lie across the pane.
                float u, v;
                if (ax >= ay && ax >= az) {
                    u = p.z; v = -p.y;
                } else if (ay >= az) {
                    u = p.x; v = -p.z;
                } else {
                    u = p.x; v = -p.y;
                }
                uv.push_back(u * 0.8f);
                uv.push_back(v * 0.8f);
            }
        }
    }
    if (xyz.empty()) return;
    const int tris = (int)(xyz.size() / 9);
    if (g_cfg.glassOpacity > 0.001f) {
        const int alpha = (int)(g_cfg.glassOpacity * 255.0f + 0.5f);
        D3D_AddColorTris(ib, xyz.data(), tris, D3DCOLOR_ARGB(alpha, 18, 20, 24));
    }
    float rain = GameRainIntensity();
    if (rain > 1.0f) rain = 1.0f;
    if (rain > 0.02f && g_cfg.glassRain > 0.001f && !uv.empty()) {
        int alpha = (int)(g_cfg.glassRain * rain * 220.0f + 0.5f);
        if (alpha > 255) alpha = 255;
        D3D_AddTexturedTris(ib, xyz.data(), uv.data(), tris, D3DCOLOR_ARGB(alpha, 255, 255, 255));
    }
}

void NoteMarkedMirrors(uint8_t* solid, SolidInfo& info, const float* matrix, const EditSet& edits) {
    if (std::find(s_manualMirrorSolids.begin(), s_manualMirrorSolids.end(), solid) != s_manualMirrorSolids.end()) return;
    s_manualMirrorSolids.push_back(solid);
    if (info.trisDirty) BuildTris(solid, info);
    void* ib = At<void*>(At<uint8_t*>(solid, kSolidPlat), kPlatIndexBuffer);
    D3D_SuppressMirrorGlass(ib);
    std::vector<uint16_t> replacing;
    for (const TriRef& tri : info.tris)
        if (edits.mirror.count(tri.id))
            replacing.insert(replacing.end(), {tri.v[0], tri.v[1], tri.v[2]});
    // The index copy changes with hides/black marks and model reloads too, not just edit generation.
    // This runs once per solid per eye; hash the indices once a frame and reuse that for the
    // second eye (GetSolid may already have filled it in for this frame).
    if (info.hashSeen != s_hashSerial) {
        info.indexHash = CurrentHash(info);
        info.hashSeen = s_hashSerial;
    }
    int version = (int)(info.indexHash ^ (uint32_t)s_editsGen);
    D3D_SetManualMirrorTriangles(ib, replacing.data(), (int)(replacing.size() / 3), version);
    bool added = false;
    for (int side = 0; side < 2; side++) {
        std::vector<float> vertices;
        if (!BuildMarkedMirrorVertices(info, edits, matrix, side, vertices)) continue;
        D3D_AddMirrorOnce(ib, vertices.data(), (int)(vertices.size() / 5), nullptr, 0);
        added = true;
    }
    if (!info.manualMirrorLogged) {
        info.manualMirrorLogged = true;
        Log("mirrors: %s: marked triangles %s the lightweight mirror overlay", SolidName(info).c_str(),
            added ? "are using" : "could not build");
    }
}

// --- triangle editor -------------------------------------------------------------------------------
// F4 > VR > Triangle editor. Picking ("Pick triangles": the menu hides): every triangle of the player
// car on the line from between the eyes, straight ahead, is a candidate; the menu's up / down step
// through them from the nearest, left / right change the selection's size (one triangle, within
// 2 / 5 / 10 cm, the connected piece, the whole draw entry). A reticle marks the point and the
// selection is shown over everything (also when behind something); the menu key confirms. The
// page's commands then hide the selection, draw it black, put it on or take it off the steering
// wheel (prepared again), restore it, or undo.
// Hidden and black triangles are made zero-area in the index buffer, (a, b, b); black ones are
// drawn again in black in their place, so the game's own (whose gauge layer ignores the depth
// test) can't show through. Saved per solid in nfsmw_vr_edits.ini.

V3 s_gazePos, s_gazeDir;
bool s_haveGaze = false;
struct Pick {
    uint8_t* solid = nullptr;
    std::string name;
    uint64_t id = 0;
    V3 hit{};                                       // model space
    V3 hitWorld{};
    float dist = 1e9f;
};
std::vector<Pick> s_hits;                           // this frame's candidates along the gaze
Pick s_pick;                                        // the one in use
std::vector<uint64_t> s_brush;                      // s_pick's selection
uint64_t s_brushFor = 0;
int s_brushMode = -1;
struct Undo { std::string name; EditSet set; };
std::vector<Undo> s_undo;
int s_eyeIndex = 0;                                 // eye being rendered (Cockpit_BeginEye counts them)
std::set<uint8_t*> s_eyeDone, s_frameDone;         // solids handled this eye / picked this frame
std::vector<float> s_overlay;                       // this eye's selection, world (drawn over the eye)

// --- driver rig (Show IK) --------------------------------------------------------------------------
// DRIVERMOD/rigonly.blend's armature, in the driver model's own space. The player's driver solid
// hands over the model-to-world matrix it is drawn with, once per eye. Show IK draws the rig over
// the scene: arms orange, the rest gray, joints as dots.
DriverIK::Rig s_driverIK;
bool s_driverIKTried = false;
M4 s_driverIKWorld;
bool s_driverIKWorldValid = false;
// Native lever target: use its submitted matrix, never add our own lever rotation.
V3 s_handbrakeGripWorld;
bool s_handbrakeGripValid = false;
V3 s_shifterHoverWorld;
bool s_shifterHoverValid = false;
V3 s_shifterVehicle, s_brakeVehicle;
bool s_controlVehicleValid = false;
uint32_t s_controlVehicle = 0;
double s_armShiftUntil = -1;
bool s_armShiftPending = false, s_shifterHandReady = false;
float s_armShiftUp = 0, s_armShiftDown = 0;
bool s_armShiftInitialized = false;
DriverHoverMotion s_armHover;
V3 ToWorld(const V3& p, const float* m);
V3 ToWorld(const V3& p, const M4& m) { return ToWorld(p,&m.m[0][0]); }
V3 s_nativeWheelPivot, s_nativeWheelAxis, s_nativeWheelOffset;
// s_curBody is hook-scoped and restored before deferred draws / EndFrame.
// Retain the matching body transform with the submitted native driver instead.
M4 s_nativeDriverBody;
bool s_nativeWheelValid = false;
uint32_t s_nativeWheelCar = 0;
bool s_armControlOverride = false;
float s_armControlBlend = 0;
int ControlSide() { return s_head.car == "CLIO" || s_head.car == "MONARO" ? 0 : 1; }
float SteeringDegrees() { return ControlSide()==0 ? -g_steeringWheelAngle : g_steeringWheelAngle; }

bool NativeSteeringActive() {
    return s_nativeWheelValid && s_nativeWheelCar == s_head.worldId;
}

DriverSmoothing::Filter s_driverSteeringSmoothing;
uint32_t s_driverSmoothingCar = 0;

// The animation angle lags the wheel when smoothing is set; the wheel itself never does.
float DriverAnimationAngle() {
    return s_driverSteeringSmoothing.initialized ? s_driverSteeringSmoothing.value : g_steeringWheelAngle;
}

void UpdateDriverAnimationSmoothing(float dt) {
    if (!s_head.valid || !NativeSteeringActive()) {
        s_driverSteeringSmoothing.Reset();
        s_driverSmoothingCar = 0;
        return;
    }
    if (s_driverSmoothingCar != s_head.worldId) {
        s_driverSmoothingCar = s_head.worldId;
        s_driverSteeringSmoothing.Reset();
    }
    s_driverSteeringSmoothing.Update(g_steeringWheelAngle, g_cfg.animatedDriverSmoothing, dt);
}

DriverSteering::Grip NativeGrip(const DriverIK::Rig& rig, int side, const M4& world) {
    const float degrees = DriverAnimationAngle();
    // Wheel measurements are stored in vehicle space, so movement between frames
    // and submission order cannot make the hand trail behind the car.
    M4 inv = MInvRigid(world);
    V3 pivotWorld = ToWorld(s_nativeWheelPivot, s_nativeDriverBody);
    V3 axisWorld = ToWorld(s_nativeWheelPivot + s_nativeWheelAxis, s_nativeDriverBody);
    V3 pivot = ToWorld(pivotWorld, inv), end = ToWorld(axisWorld, inv);
    const auto& bone = rig.bones[rig.arms[side*3+2]];
    CustomDriver::Point wrist{bone.head[0],bone.head[1],bone.head[2]};
    V3 offsetWorld=ToWorld(s_nativeWheelOffset,s_nativeDriverBody)-ToWorld(V3{},s_nativeDriverBody);
    V3 offset=ToWorld(offsetWorld,inv)-ToWorld(V3{},inv);
    wrist=wrist+CustomDriver::Point{offset.x,offset.y,offset.z};
    CustomDriver::Point direction{bone.tail[0]-bone.head[0],bone.tail[1]-bone.head[1],bone.tail[2]-bone.head[2]};
    if(ControlSide()==0) {
        float centre=(rig.bones[rig.arms[2]].head[1]+rig.bones[rig.arms[5]].head[1])*0.5f;
        auto grip=DriverSteering::Evaluate({wrist.x,2*centre-wrist.y,wrist.z},
            {direction.x,-direction.y,direction.z},{pivot.x,2*centre-pivot.y,pivot.z},
            {-(end.x-pivot.x),end.y-pivot.y,-(end.z-pivot.z)},
            -degrees,1-side,g_cfg.animatedDriverHandFix,g_cfg.animatedDriverSpread);
        grip.wrist.y=2*centre-grip.wrist.y; grip.direction.y=-grip.direction.y;
        grip.roll=-grip.roll;
        return grip;
    }
    return DriverSteering::Evaluate(wrist,
        {bone.tail[0]-bone.head[0],bone.tail[1]-bone.head[1],bone.tail[2]-bone.head[2]},
        {pivot.x,pivot.y,pivot.z}, {end.x-pivot.x,end.y-pivot.y,end.z-pivot.z},
        degrees, side, g_cfg.animatedDriverHandFix,g_cfg.animatedDriverSpread);
}
struct ArmDraw { uint8_t* solid; M4 world; };
std::unordered_map<void*, ArmDraw> s_armDraws;

const DriverIK::Rig* DriverRig() {
    // Per-driver cache survives eye boundaries but cannot leak across cars or LODs.
    for(auto& item:s_solids) {
        Driver* driver=item.second.driver;
        if(!driver || driver->car!=s_head.car || !driver->ok) continue;
        if(!driver->rigTried) {
            driver->rigTried=true;
            char path[MAX_PATH];
            snprintf(path,sizeof(path),"%sDRIVERMOD\\rigs\\%s.bin",GameDir(),driver->car.c_str());
            char name[65]{}; memcpy(name,item.second.name,64);
            if(strstr(name,"_DRIVER_A"))
                DriverIK::LoadFile(path,&driver->rig);
        }
        if(driver->rig.ok) return &driver->rig;
    }
    // Keep the original Gallardo path for older installs/tests only. A missing
    // fit is not permission to animate another car with Gallardo coordinates.
    if(!s_head.car.empty() && s_head.car!="GALLARDO") return nullptr;
    if (!s_driverIKTried) {
        s_driverIKTried = true;
        char path[MAX_PATH];
        snprintf(path, sizeof(path), "%sDRIVERMOD\\driver-ik.bin", GameDir());
        if (DriverIK::LoadFile(path, &s_driverIK))
            Log("driver IK: %u bones, %u controls from %s", (unsigned)s_driverIK.bones.size(),
                (unsigned)s_driverIK.controls.size(), path);
        else
            Log("driver IK: could not load %s", path);
    }
    return s_driverIK.ok ? &s_driverIK : nullptr;
}

void UpdateDriverHover(float dt, double now = -1) {
    if (now < 0) now = GetTickCount64() * 0.001;
    s_shifterHandReady = false;
    if (!s_shifterHoverValid && !s_handbrakeGripValid) {
        s_armHover.Reset();
        s_armControlBlend = 0;
        s_armControlOverride = false;
        s_armShiftUntil = -1;
        s_armShiftPending = false;
        s_armShiftInitialized = false;
        s_armShiftUp = g_cockpitShiftUp;
        s_armShiftDown = g_cockpitShiftDown;
        return;
    }
    if (!s_shifterHoverValid) {
        s_armShiftUntil = -1;
        s_armShiftPending = false;
    }
    if (s_armShiftInitialized && s_head.valid &&
        (s_armShiftUp != g_cockpitShiftUp || s_armShiftDown != g_cockpitShiftDown))
    {
        s_armShiftPending = true;
        s_armShiftUntil = now + 0.24;
    }
    s_armShiftInitialized = s_head.valid;
    s_armShiftUp = g_cockpitShiftUp; s_armShiftDown = g_cockpitShiftDown;
    // A shift temporarily interrupts the brake pose, even with the input held.
    // After the arrival-based hold expires, the held brake becomes the target again.
    bool shift = s_shifterHoverValid && (s_armShiftPending || now < s_armShiftUntil);
    bool brake = !shift && g_cockpitHandbrake > 0.01f && s_handbrakeGripValid;
    s_armControlOverride = brake || shift;
    const DriverIK::Rig* rig = DriverRig();
    int control=ControlSide();
    if (!s_head.valid || !s_driverIKWorldValid || !rig || rig->arms[control*3+2] < 0) {
        s_armHover.Reset();
        s_armControlBlend = 0;
        s_armControlOverride = false;
        s_armShiftUntil = -1;
        s_armShiftPending = false;
        return;
    }
    M4 inv = MInvRigid(s_driverIKWorld);
    V3 w = brake ? s_handbrakeGripWorld : s_shifterHoverWorld;
    if(s_controlVehicleValid && s_controlVehicle==s_head.worldId)
        w=ToWorld(brake?s_brakeVehicle:s_shifterVehicle,s_nativeDriverBody);
    V3 p = {w.x*inv.m[0][0]+w.y*inv.m[1][0]+w.z*inv.m[2][0]+inv.m[3][0],
            w.x*inv.m[0][1]+w.y*inv.m[1][1]+w.z*inv.m[2][1]+inv.m[3][1],
            w.x*inv.m[0][2]+w.y*inv.m[1][2]+w.z*inv.m[2][2]+inv.m[3][2]};
    const auto& fore = rig->bones[rig->arms[control*3+2]];
    // Drive the elbow, not wrist IK. The distal arm stays rigid, without hand articulation.
    CustomDriver::Point rest{fore.head[0],fore.head[1],fore.head[2]};
    if (NativeSteeringActive()) rest = NativeGrip(*rig, control, s_driverIKWorld).wrist;
    CustomDriver::Point goal{p.x-(fore.tail[0]-fore.head[0]),
        p.y-(fore.tail[1]-fore.head[1]),p.z-(fore.tail[2]-fore.head[2])};
    // The rigid hand tip, not an arbitrary clamped wrist pose, hovers over the
    // measured control. Brake elbow clearance is applied separately below.
    // A free hand follows the angle exactly. Control interruptions remain speed
    // limited and return to the live steering grip rather than the neutral wrist.
    if (NativeSteeringActive() && !s_armControlOverride && s_armControlBlend == 0) {
        s_armHover.wrist = rest;
        s_armHover.initialized = true;
        s_armHover.progress = 0;
    } else s_armHover.Update(dt, brake || shift, rest, goal);
    float blendStep = std::max(0.0f,std::min(0.1f,dt)) * 8;
    s_armControlBlend = s_armControlOverride ? std::min(1.0f,s_armControlBlend+blendStep) :
        std::max(0.0f,s_armControlBlend-blendStep);
    if (!s_armControlOverride && CustomDriver::Length(s_armHover.wrist-rest) > 0.0001f)
        s_armControlBlend = std::max(0.001f,s_armControlBlend);
    s_shifterHandReady = shift && CustomDriver::Length(s_armHover.wrist-goal) <= 0.005f;
    if (s_shifterHandReady && s_armShiftPending) {
        s_armShiftPending = false;
        s_armShiftUntil = now + 0.24; // hold starts at arrival, not at the input edge
    }
    CustomDriver::Point elbowGoal = brake ? CustomDriver::Point{0,control ? -0.08f : 0.08f,-0.10f} : CustomDriver::Point{};
    auto elbowDelta = elbowGoal - s_armHover.brakeElbowOffset;
    float elbowDistance = CustomDriver::Length(elbowDelta);
    float elbowStep = DriverHoverMotion::speed * std::max(0.0f,std::min(0.1f,dt));
    if (elbowDistance <= elbowStep) s_armHover.brakeElbowOffset = elbowGoal;
    else if (elbowDistance > 1e-7f)
        s_armHover.brakeElbowOffset = s_armHover.brakeElbowOffset + elbowDelta*(elbowStep/elbowDistance);
}

void DriverArmPose(const M4& world, std::vector<V3>& head, std::vector<V3>& tail) {
    const DriverIK::Rig* rig = DriverRig();
    if (!rig || !rig->ok || rig->bones.empty()) return;
    size_t n = rig->bones.size();
    head.resize(n); tail.resize(n);
    for (size_t i = 0; i < n; i++) {
        head[i] = {rig->bones[i].head[0], rig->bones[i].head[1], rig->bones[i].head[2]};
        tail[i] = {rig->bones[i].tail[0], rig->bones[i].tail[1], rig->bones[i].tail[2]};
    }

    if (NativeSteeringActive() || s_armHover.progress > 0.0001f) {
        for (int side=0; side<2; ++side) {
            if (!NativeSteeringActive() && side!=ControlSide()) continue;
            int upper=rig->arms[side*3], fore=rig->arms[side*3+1], hand=rig->arms[side*3+2];
            if (upper<0 || fore<0 || hand<0) continue;
            const auto& restHand=rig->bones[hand];
            auto grip=NativeSteeringActive() ? NativeGrip(*rig,side,world) : DriverSteering::Grip{
                {restHand.head[0],restHand.head[1],restHand.head[2]},
                {restHand.tail[0]-restHand.head[0],restHand.tail[1]-restHand.head[1],restHand.tail[2]-restHand.head[2]}};
            CustomDriver::Point wrist=grip.wrist;
            auto direction=grip.direction;
            if (side==ControlSide() && s_armHover.initialized && (s_armControlBlend>0 || !NativeSteeringActive())) {
                wrist=s_armHover.wrist;
                const auto& b=rig->bones[hand];
                direction=CustomDriver::Mix(direction,{b.tail[0]-b.head[0],b.tail[1]-b.head[1],b.tail[2]-b.head[2]},s_armControlBlend);
                direction=CustomDriver::Unit(direction)*CustomDriver::Length(grip.direction);
            }
            auto point=[](V3 p) { return CustomDriver::Point{p.x,p.y,p.z}; };
            CustomDriver::Point elbow;
            // Transport the fitted bend plane with the shoulder-to-wrist ray.
            // Blending toward a fixed model-space elbow during lift changes
            // the bend plane independently of the hand and can splay the elbow.
            auto elbowOffset = side==ControlSide() ? s_armHover.brakeElbowOffset : CustomDriver::Point{};
            if (!DriverSteering::Solve(point(head[upper]),point(head[fore]),point(head[hand]),wrist,elbow,
                elbowOffset)) continue;
            tail[upper]=head[fore]={elbow.x,elbow.y,elbow.z};
            tail[fore]=head[hand]={wrist.x,wrist.y,wrist.z};
            auto tip=wrist+direction;
            tail[hand]={tip.x,tip.y,tip.z};
            int extra=rig->Find(side ? "JNT_hand_tip.R" : "JNT_hand_tip.L");
            if (extra>=0) {
                auto old=point(head[extra]);
                auto extent=CustomDriver::Rotate(point(tail[extra])-old,
                    {rig->bones[hand].tail[0]-rig->bones[hand].head[0],rig->bones[hand].tail[1]-rig->bones[hand].head[1],rig->bones[hand].tail[2]-rig->bones[hand].head[2]},direction);
                head[extra]=tail[hand];
                tail[extra]={tip.x+extent.x,tip.y+extent.y,tip.z+extent.z};
            }
        }
        return;
    }
}
void DrawDriverIK() {
    if (g_cfg.showDriverIK < 0.5f || !s_driverIKWorldValid) return;
    const DriverIK::Rig* rig = DriverRig();
    std::vector<V3> head, tail;
    DriverArmPose(s_driverIKWorld, head, tail);
    if (head.empty()) return;
    size_t n = head.size();
    const float* m = &s_driverIKWorld.m[0][0];
    auto toWorld = [&](const V3& p) -> V3 {
        return {p.x * m[0] + p.y * m[4] + p.z * m[8] + m[12],
                p.x * m[1] + p.y * m[5] + p.z * m[9] + m[13],
                p.x * m[2] + p.y * m[6] + p.z * m[10] + m[14]};
    };
    std::vector<float> arms, body, joints;
    for (size_t i = 0; i < n; i++) {
        V3 h = toWorld(head[i]), t = toWorld(tail[i]);
        // The six marked shoulder / elbow / wrist bones and the hand tips: the arm chain.
        bool arm = rig->bones[i].arm || strstr(rig->bones[i].name, "hand_tip") != nullptr;
        std::vector<float>& lines = arm ? arms : body;
        lines.insert(lines.end(), {h.x, h.y, h.z, t.x, t.y, t.z});
        joints.insert(joints.end(), {h.x, h.y, h.z});
    }
    D3D_DrawOverlayLines(body.data(), (int)body.size() / 6, 0.0035f, D3DCOLOR_ARGB(200, 150, 150, 150));
    D3D_DrawOverlayLines(arms.data(), (int)arms.size() / 6, 0.005f, D3DCOLOR_ARGB(235, 255, 150, 0));
    D3D_DrawOverlayPoints(joints.data(), (int)joints.size() / 3, 0.007f, D3DCOLOR_ARGB(220, 255, 255, 255));
}

bool EditorLive() { return g_cfg.editorOn > 0.5f || g_cfg.editPicking > 0.5f; }

std::string SolidName(const SolidInfo& info) {
    char name[65] = {};
    memcpy(name, info.name, 64);
    return name;
}

// The solid's edits without rebuilding its name string and re-hashing it on every driver draw; the
// cached pointer stays valid while the solid's name is unchanged (GetSolid rebuilds the info then).
const EditSet& EditsOf(SolidInfo& info) {
    if (!info.edits) info.edits = &EditsFor(SolidName(info));
    return *info.edits;
}

// The solid's triangles, the hidden ones included (where they were), for picking.
void BuildTris(uint8_t* solid, SolidInfo& info) {
    info.tris.clear();
    info.trisDirty = false;
    info.blackDirty = true;
    SolidMesh s;
    if (!ReadMesh(solid, s)) return;
    for (int i = 0; i < s.n; i++) {
        uint8_t* e = s.entries + i * kEntrySize;
        int tris = At<int>(e, kEntryTris), first = At<int>(e, kEntryFirstIndex);
        if (tris <= 0 || first < 0 || first + tris * 3 > s.numIndices) continue;
        for (int t = 0; t < tris; t++) {
            int p = first + t * 3;
            const uint16_t* tri = s.indices + p;
            if (tri[0] >= s.numVerts || tri[1] >= s.numVerts || tri[2] >= s.numVerts) continue;
            bool empty = tri[0] == tri[1] || tri[1] == tri[2] || tri[0] == tri[2];
            uint16_t v[3] = {tri[0], tri[1], tri[2]};
            if (empty) {
                // One of ours, (a, b, b): the triangle it was.
                bool found = false;
                for (auto& h : info.hidden)
                    if (h.second[0] == tri[0] && h.second[1] == tri[1] && tri[2] == tri[1]) {
                        memcpy(v, h.second.data(), 6);
                        found = true;
                        break;
                    }
                if (!found || v[2] >= s.numVerts) continue;
            }
            TriRef r;
            for (int k = 0; k < 3; k++) {
                r.p[k] = s.at(v[k]);
                r.v[k] = v[k];
            }
            r.entry = i;
            r.id = TriId(v[0], v[1], v[2]);
            r.hidden = empty;
            info.tris.push_back(r);
        }
    }
}

bool BuildMarkedMirrorVertices(const SolidInfo& info, const EditSet& edits, const float* matrix, int side,
                              std::vector<float>& out) {
    std::vector<const TriRef*> triangles;
    V3 normalSum{}, inward{-1.0f, side == 0 ? -1.0f : 1.0f, 0};
    float minU = 1e9f, maxU = -1e9f, minV = 1e9f, maxV = -1e9f;
    for (const TriRef& tri : info.tris) {
        if (!edits.mirror.count(tri.id)) continue;
        float y = (tri.p[0].y + tri.p[1].y + tri.p[2].y) / 3.0f;
        if ((side == 0 && y < 0) || (side == 1 && y >= 0)) continue;
        V3 n = mirror::Cross(mirror::Sub(tri.p[1], tri.p[0]), mirror::Sub(tri.p[2], tri.p[0]));
        if (mirror::Dot(n, inward) < 0) n = {-n.x, -n.y, -n.z};
        normalSum = normalSum + n;
        triangles.push_back(&tri);
    }
    if (triangles.empty()) return false;
    V3 normal = mirror::Norm(normalSum);
    if (mirror::Dot(normal, normal) < 0.5f) return false;
    V3 u = mirror::Norm(mirror::Cross({0, 0, 1}, normal));
    if (mirror::Dot(u, u) < 0.5f) u = mirror::Norm(mirror::Cross({0, 1, 0}, normal));
    V3 v = mirror::Norm(mirror::Cross(normal, u));
    for (const TriRef* tri : triangles)
        for (const V3& p : tri->p) {
            float pu = mirror::Dot(p, u), pv = mirror::Dot(p, v);
            minU = std::min(minU, pu); maxU = std::max(maxU, pu);
            minV = std::min(minV, pv); maxV = std::max(maxV, pv);
        }
    if (maxU - minU < 0.001f || maxV - minV < 0.001f) return false;
    float rearU0, rearU1;
    SideMirrorRange(side, &rearU0, &rearU1);
    out.clear();
    out.reserve(triangles.size() * 15);
    for (const TriRef* tri : triangles)
        for (const V3& p : tri->p) {
            // In the surface being replaced: the opaque, depth-biased picture covers the original
            // texture from the same vertices instead of floating 8 mm ahead of the housing.
            V3 world = ToWorld(p, matrix);
            float across = (mirror::Dot(p, u) - minU) / (maxU - minU);
            float down = (maxV - mirror::Dot(p, v)) / (maxV - minV);
            out.insert(out.end(), {world.x, world.y, world.z, rearU0 + (rearU1 - rearU0) * across, down});
        }
    return !out.empty();
}

bool WriteIndices(const SolidMesh& s, int at, const uint16_t v[3]) {
    void* p = nullptr;
    if (FAILED(s.ib->Lock(at * 2, 6, &p, 0)) || !p) return false;
    memcpy(p, v, 6);
    s.ib->Unlock();
    memcpy(s.indices + at, v, 6);
    return true;
}

// What should be zero-area in the index buffer now: the hidden and the black triangles (black ones
// are drawn again in black in their place: drawn over the game's own, they fought with them, whose
// gauge layer doesn't keep to the depth test); applied by difference (newly hidden made (a, b, b),
// others put back). A model prepared again gets them back first (GetSolid, RestoreAll).
void ApplyHides(uint8_t* solid, SolidInfo& info) {
    const EditSet& e = EditsOf(info);
    std::set<uint64_t> want;
    for (auto& h : e.hide) want.insert(h.first);
    want.insert(e.black.begin(), e.black.end());
    info.blackDirty = true;
    if (want.empty() && info.hidden.empty()) return;
    SolidMesh s;
    if (!ReadMesh(solid, s)) return;
    bool changed = false;
    for (auto it = info.hidden.begin(); it != info.hidden.end();) {
        if (want.count(it->first)) {
            ++it;
            continue;
        }
        const auto& w = it->second;
        for (int p = 0; p + 2 < s.numIndices; p += 3)
            if (s.indices[p] == w[0] && s.indices[p + 1] == w[1] && s.indices[p + 2] == w[1]) {
                changed |= WriteIndices(s, p, w.data());
                break;
            }
        it = info.hidden.erase(it);
    }
    for (uint64_t id : want) {
        if (info.hidden.count(id)) continue;
        for (int p = 0; p + 2 < s.numIndices; p += 3) {
            const uint16_t* t = s.indices + p;
            if (t[0] == t[1] || t[1] == t[2] || TriId(t[0], t[1], t[2]) != id) continue;
            std::array<uint16_t, 3> wound = {t[0], t[1], t[2]};
            uint16_t gone[3] = {t[0], t[1], t[1]};
            if (WriteIndices(s, p, gone)) {
                info.hidden[id] = wound;
                changed = true;
            }
            break;
        }
    }
    if (changed) {
        info.indexHash = CurrentHash(info);
        info.trisDirty = true;
    }
}

// The editor's black triangles, model space, in place of the hidden originals.
void UpdateBlack(uint8_t* solid, SolidInfo& info) {
    if (!info.blackDirty) return;
    info.blackDirty = false;
    info.black.clear();
    const EditSet& e = EditsOf(info);
    const std::set<uint64_t>& ids = e.black;
    if (ids.empty()) return;
    SolidMesh s;
    if (!ReadMesh(solid, s)) return;
    for (uint64_t id : ids) {
        std::array<uint16_t, 3> v = IdTri(id);
        if (v[0] >= s.numVerts || v[1] >= s.numVerts || v[2] >= s.numVerts) continue;
        for (int k = 0; k < 3; k++) info.black.push_back(s.at(v[k]));
    }
}

V3 ToWorld(const V3& p, const float* m) {
    return {p.x * m[0] + p.y * m[4] + p.z * m[8] + m[12], p.x * m[1] + p.y * m[5] + p.z * m[9] + m[13],
            p.x * m[2] + p.y * m[6] + p.z * m[10] + m[14]};
}

// Model-space triangles to the world for the current eye, each vertex lifted `lift` m toward the
// eyes (last frame's gaze position: the game camera's own is not where the eyes are).
void AddColor(void* ib, const std::vector<V3>& tris, const float* m, float lift, uint32_t argb) {
    if (tris.empty()) return;
    V3 eye = s_gazePos;
    if (!s_haveGaze) {
        void* cam = At<void*>((void*)kPlayerView, kViewCamera);
        if (!cam) return;
        const float* c = &At<float>(cam, kCamPosition);
        eye = {c[0], c[1], c[2]};
    }
    std::vector<float> xyz(tris.size() * 3);
    for (size_t i = 0; i < tris.size(); i++) {
        V3 w = ToWorld(tris[i], m);
        V3 d = Sub(eye, w);
        float len = sqrtf(Dot(d, d));
        float k = len > 1e-4f ? std::min(lift, len * 0.5f) / len : 0.0f;
        xyz[i * 3] = w.x + d.x * k;
        xyz[i * 3 + 1] = w.y + d.y * k;
        xyz[i * 3 + 2] = w.z + d.z * k;
    }
    D3D_AddColorTris(ib, xyz.data(), (int)tris.size() / 3, argb);
}

// The gaze against the solid's triangles (drawn with `m`): every hit is a candidate.
void PickIn(uint8_t* solid, SolidInfo& info, const float* m) {
    if (info.trisDirty) BuildTris(solid, info);
    V3 ax[3] = {{m[0], m[1], m[2]}, {m[4], m[5], m[6]}, {m[8], m[9], m[10]}};
    V3 rel = Sub(s_gazePos, {m[12], m[13], m[14]});
    V3 o, d;
    float* oc[3] = {&o.x, &o.y, &o.z};
    float* dc[3] = {&d.x, &d.y, &d.z};
    for (int i = 0; i < 3; i++) {
        float l2 = Dot(ax[i], ax[i]);
        if (l2 < 1e-8f) return;
        *oc[i] = Dot(rel, ax[i]) / l2;
        *dc[i] = Dot(s_gazeDir, ax[i]) / l2;
    }
    std::string name = SolidName(info);
    for (const TriRef& t : info.tris) {
        float hit;
        if (!RayTriangle(o, d, t.p[0], t.p[1], t.p[2], &hit) || hit < 0.02f || hit > 6.0f) continue;
        Pick p;
        p.solid = solid;
        p.name = name;
        p.id = t.id;
        p.hit = o + d * hit;
        p.hitWorld = s_gazePos + s_gazeDir * hit;   // the map from model to world is affine: t in metres
        p.dist = hit;
        s_hits.push_back(p);
    }
}

// s_pick's selection for the size chosen (Config::editMode).
void UpdateBrush(SolidInfo& info) {
    int mode = std::max(0, std::min(5, (int)lrintf(g_cfg.editMode)));
    if (s_brushFor == s_pick.id && s_brushMode == mode && !info.trisDirty) return;
    s_brushFor = s_pick.id;
    s_brushMode = mode;
    s_brush.assign(1, s_pick.id);
    const TriRef* picked = nullptr;
    for (const TriRef& t : info.tris)
        if (t.id == s_pick.id) picked = &t;
    if (!picked || mode == 0) return;
    if (mode <= 3) {
        const float radius[4] = {0, 0.02f, 0.05f, 0.10f};
        float r = radius[mode];
        for (const TriRef& t : info.tris) {
            V3 dd = Sub((t.p[0] + t.p[1] + t.p[2]) * (1.0f / 3), s_pick.hit);
            if (t.id != s_pick.id && Dot(dd, dd) < r * r) s_brush.push_back(t.id);
        }
    } else if (mode == 4) {
        // The connected piece: triangles sharing vertices, spreading from the one picked.
        std::unordered_map<uint16_t, std::vector<int>> byVertex;
        for (int i = 0; i < (int)info.tris.size(); i++)
            for (int k = 0; k < 3; k++) byVertex[info.tris[i].v[k]].push_back(i);
        std::vector<char> seen(info.tris.size(), 0);
        std::vector<int> todo(1, (int)(picked - info.tris.data()));
        seen[todo[0]] = 1;
        while (!todo.empty()) {
            int i = todo.back();
            todo.pop_back();
            if (info.tris[i].id != s_pick.id) s_brush.push_back(info.tris[i].id);
            for (int k = 0; k < 3; k++)
                for (int j : byVertex[info.tris[i].v[k]])
                    if (!seen[j]) {
                        seen[j] = 1;
                        todo.push_back(j);
                    }
        }
    } else {
        for (const TriRef& t : info.tris)
            if (t.entry == picked->entry && t.id != s_pick.id) s_brush.push_back(t.id);
    }
}

// Every triangle made zero-area put back (the wheel and the gauges are found from whole triangles).
void RestoreAll(uint8_t* solid, SolidInfo& info) {
    SolidMesh s;
    if (info.hidden.empty() || !ReadMesh(solid, s)) return;
    for (auto& h : info.hidden)
        for (int p = 0; p + 2 < s.numIndices; p += 3)
            if (s.indices[p] == h.second[0] && s.indices[p + 1] == h.second[1] && s.indices[p + 2] == h.second[1]) {
                WriteIndices(s, p, h.second.data());
                break;
            }
    info.hidden.clear();
    info.indexHash = CurrentHash(info);
    info.trisDirty = info.blackDirty = true;
}

// Prepares the steering wheel of every loaded solid with that name again (after wheel edits).
void RePrepareWheels(const std::string& name) {
    s_wheelTemplates.erase(name);
    s_rejectedWheels.erase(std::remove_if(s_rejectedWheels.begin(), s_rejectedWheels.end(),
        [&](const RejectedWheel& cached) { return cached.name == name; }), s_rejectedWheels.end());
    for (auto& kv : s_solids) {
        SolidInfo& info = kv.second;
        if (!info.interior || SolidName(info) != name) continue;
        const HeadPos* head = CarHead(name.c_str());
        if (!head) continue;
        RestoreAll(kv.first, info);
        if (info.wheel) s_retiredWheels.push_back(info.wheel);
        info.wheel = PrepareWheel(kv.first, name.c_str(), head->v);
        if (info.wheel) RememberWheelTemplate(kv.first, name, *info.wheel);
        info.indexHash = CurrentHash(info);
        info.trisDirty = info.blackDirty = true;
    }
}

void ApplyEditsTo(const std::string& name, bool wheel) {
    if (wheel) RePrepareWheels(name);
    for (auto& kv : s_solids) {
        if (SolidName(kv.second) != name) continue;
        ApplyHides(kv.first, kv.second);
        kv.second.blackDirty = true;
        if (kv.second.mirrorKind == 4 && EditsFor(name).mirror.empty()) kv.second.mirrorKind = -1;
    }
    s_brushFor = 0;
}

// The current car's steering wheel turning centre fitted to the wheel's triangles as they are now
// (those put on it by hand too), as the pivot offset (Config::wheelPivot, saved per car); which
// triangles are on the wheel is left as it is. The outermost wheel vertex in each of 36 directions
// round the column, a circle through them (least squares; points over 3 cm off the median radius
// dropped), three times from the new centre.
void CentreWheelPivot() {
    std::string car = s_head.valid ? s_head.car : std::string();
    for (auto& kv : s_solids) {
        SolidInfo& info = kv.second;
        std::string name = SolidName(info);
        if (car.empty() || !info.interior || !info.wheel || !info.wheel->ok || name.compare(0, car.size() + 1, car + "_") != 0)
            continue;
        const Wheel& w = *info.wheel;
        SolidMesh s;
        if (!ReadMesh(kv.first, s)) continue;
        std::vector<V3> pts;
        for (int i = 0; i < s.n && i < (int)w.kept.size(); i++) {
            if (!w.turned[i]) continue;
            int first = At<int>(s.entries + i * kEntrySize, kEntryFirstIndex);
            int a = first + w.kept[i] * 3, b = std::min(s.numIndices, first + (w.kept[i] + w.turned[i]) * 3);
            for (int k = std::max(0, a); k < b; k++)
                if (s.indices[k] < s.numVerts) pts.push_back(s.at(s.indices[k]));
        }
        V3 k = Norm({w.axis[0], w.axis[1], w.axis[2]}), e1 = {0, 1, 0};   // the column is in the x-z plane
        V3 e2 = Norm({-k.z, 0, k.x});                                      // up in the wheel's plane
        V3 c0 = {w.centre[0], w.centre[1], w.centre[2]}, c = c0;
        bool fitted = false;
        for (int pass = 0; pass < 3; pass++) {
            float rmax[36], sa[36] = {}, sb[36] = {};
            for (float& r : rmax) r = -1;
            for (const V3& p : pts) {
                V3 rel = Sub(p, c);
                if (fabsf(Dot(rel, k)) > 0.08f) continue;
                float a = Dot(rel, e1), b = Dot(rel, e2), r = sqrtf(a * a + b * b);
                if (r < 0.06f || r > 0.30f) continue;
                int sec = (int)((atan2f(b, a) + 3.14159265f) / (2 * 3.14159265f) * 36) % 36;
                if (r > rmax[sec]) {
                    rmax[sec] = r;
                    sa[sec] = a;
                    sb[sec] = b;
                }
            }
            std::vector<float> radii;
            for (float r : rmax)
                if (r > 0) radii.push_back(r);
            if (radii.size() < 18) break;
            std::nth_element(radii.begin(), radii.begin() + radii.size() / 2, radii.end());
            float med = radii[radii.size() / 2];
            // Circle a^2 + b^2 + D a + E b + F = 0 through the points, least squares (Cramer's rule).
            double m[3][3] = {}, v[3] = {};
            int used = 0;
            for (int i = 0; i < 36; i++) {
                if (rmax[i] < 0 || fabsf(rmax[i] - med) > 0.03f) continue;
                double row[3] = {sa[i], sb[i], 1}, rhs = -(sa[i] * (double)sa[i] + sb[i] * (double)sb[i]);
                for (int r = 0; r < 3; r++) {
                    for (int q = 0; q < 3; q++) m[r][q] += row[r] * row[q];
                    v[r] += row[r] * rhs;
                }
                used++;
            }
            if (used < 12) break;
            auto det = [](double a[3][3]) {
                return a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0]) +
                       a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
            };
            double d = det(m);
            if (fabs(d) < 1e-12) break;
            double sol[2];
            for (int col = 0; col < 2; col++) {
                double t[3][3];
                memcpy(t, m, sizeof(t));
                for (int r = 0; r < 3; r++) t[r][col] = v[r];
                sol[col] = det(t) / d;
            }
            c = c + e1 * (float)(-sol[0] / 2) + e2 * (float)(-sol[1] / 2);
            fitted = true;
        }
        V3 moved = Sub(c, c0);
        float right = -Dot(moved, e1), up = Dot(moved, e2);
        if (!fitted || fabsf(right) > 0.1f || fabsf(up) > 0.1f) {
            Log("cockpit: %s: steering wheel turning centre: no fit (%u wheel vertices)", name.c_str(), (unsigned)pts.size());
            return;
        }
        g_cfg.wheelPivot[0] = right;
        g_cfg.wheelPivot[1] = up;
        Log("cockpit: %s: steering wheel turns about a centre %.1f mm right, %.1f mm up of the found one (%u wheel vertices)",
            name.c_str(), right * 1000, up * 1000, (unsigned)pts.size());
        return;
    }
    Log("cockpit: steering wheel turning centre: no steering wheel for %s", car.c_str());
}

std::string SelectionText(const std::string& name, const std::vector<uint64_t>& ids) {
    std::set<uint64_t> unique(ids.begin(), ids.end());
    std::string text = "Solid: " + name + "\r\nTriangles (sorted vertex indices):\r\n";
    for (uint64_t id : unique) {
        char line[64];
        sprintf(line, "%u.%u.%u\r\n", (unsigned)((id >> 32) & 0xffff),
                (unsigned)((id >> 16) & 0xffff), (unsigned)(id & 0xffff));
        text += line;
    }
    return text;
}

void CopySelection() {
    auto it = s_solids.find(s_pick.solid);
    if (!s_pick.solid || it == s_solids.end() || SolidName(it->second) != s_pick.name || s_brush.empty()) {
        Log("editor: nothing picked to copy");
        return;
    }
    std::string text = SelectionText(s_pick.name, s_brush);
    HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    if (!data) { Log("editor: clipboard allocation failed"); return; }
    void* p = GlobalLock(data);
    if (!p) { GlobalFree(data); Log("editor: clipboard lock failed"); return; }
    memcpy(p, text.c_str(), text.size() + 1);
    GlobalUnlock(data);
    if (!OpenClipboard(nullptr)) { GlobalFree(data); Log("editor: clipboard is unavailable"); return; }
    bool copied = EmptyClipboard() && SetClipboardData(CF_TEXT, data);
    CloseClipboard();
    if (!copied) GlobalFree(data);
    Log("editor: %s", copied ? "triangle IDs copied to clipboard" : "clipboard copy failed");
}

// The F4 page's command for the selection (see Config::editCommand).
void RunEditCommand(int cmd) {
    if (cmd == 6) {
        if (s_undo.empty()) {
            Log("editor: nothing to undo");
            return;
        }
        Undo u = s_undo.back();
        s_undo.pop_back();
        EditSet& cur = EditsFor(u.name);
        bool wheel = cur.wheel != u.set.wheel || cur.noWheel != u.set.noWheel;
        cur = u.set;
        cur.loaded = true;
        s_editsGen++;
        SaveEdits(u.name);
        ApplyEditsTo(u.name, wheel);
        Log("editor: %s: undone", u.name.c_str());
        return;
    }
    auto it = s_solids.find(s_pick.solid);
    if (!s_pick.solid || it == s_solids.end() || SolidName(it->second) != s_pick.name || s_brush.empty()) {
        Log("editor: nothing picked");
        return;
    }
    SolidInfo& info = it->second;
    if (cmd == 20 || cmd == 21) {
        if (info.interior) {
            Log("editor: Make mirror applies to exterior mirror glass; the interior rear-view mirror is unchanged");
            return;
        }
        if (info.trisDirty) BuildTris(s_pick.solid, info);
    }
    if (cmd==19) {
        if (!info.driver || !info.driver->ok) {
            Log("editor: Merge fingers requires a driver finger submesh");
            return;
        }
        if (info.trisDirty) BuildTris(s_pick.solid,info);
        // Use exactly the highlighted selection, as Mark as arms does. A driver
        // hand can be in another draw entry; entry 1 is not a reliable identity.
    }
    EditSet& e = EditsFor(s_pick.name);
    s_undo.push_back({s_pick.name, e});
    if (s_undo.size() > 100) s_undo.erase(s_undo.begin());
    std::map<uint64_t, std::array<uint16_t, 3>> wound;
    for (const TriRef& t : info.tris) wound[t.id] = {t.v[0], t.v[1], t.v[2]};
    bool wheel = false;
    for (uint64_t id : s_brush) {
        switch (cmd) {
            case 1:
                if (wound.count(id)) e.hide[id] = wound[id];
                e.skin.erase(id);
                e.black.erase(id);
                e.mirror.erase(id);
                e.show.erase(id);
                break;
            case 2:
                e.skin.erase(id);
                e.black.insert(id);
                e.mirror.erase(id);
                e.hide.erase(id);
                break;
            case 3:
                e.handbrake.erase(id);
                e.shifter.erase(id);
                e.wheel.insert(id);
                e.noWheel.erase(id);
                wheel = true;
                break;
            case 4:
                e.noWheel.insert(id);
                e.wheel.erase(id);
                wheel = true;
                break;
            case 5:
                e.arms.erase(id);
                if(info.driver) e.native.insert(id); // persistent opt-out, not re-added next frame
                e.skin.erase(id);
                e.handbrake.erase(id);
                e.shifter.erase(id);
                wheel |= e.wheel.erase(id) + e.noWheel.erase(id) > 0;
                e.hide.erase(id);
                e.black.erase(id);
                e.mirror.erase(id);
                e.show.erase(id);
                break;
            case 13:
            case 14:
                wheel |= e.wheel.erase(id) > 0;
                e.noWheel.insert(id);  // keep wheel auto-detection from claiming a marked prop
                e.handbrake.erase(id);
                e.shifter.erase(id);
                (cmd == 13 ? e.handbrake : e.shifter).insert(id);
                break;
            case 15:
                e.handbrake.erase(id);
                e.shifter.erase(id);
                break;
            case 16:
                e.native.erase(id);
                e.arms.insert(id);
                break;
            case 17:
                e.arms.erase(id);
                break;
            case 18:
                e.native.erase(id);
                e.skin.insert(id);
                e.black.erase(id);
                e.hide.erase(id);
                if (info.driver) e.show.insert(id);
                break;
            case 19:
                e.native.erase(id);
                e.arms.insert(id);
                e.skin.insert(id);
                e.show.insert(id);
                e.black.erase(id);
                e.hide.erase(id);
                break;
            case 20:
                e.mirror.insert(id);
                e.hide.erase(id);
                e.black.erase(id);
                break;
            case 21:
                e.mirror.erase(id);
                break;
            case 22:
                e.wipers.insert(id);
                e.hide.erase(id);
                e.black.erase(id);
                e.mirror.erase(id);
                break;
            case 23:
                e.wipers.erase(id);
                break;
            case 24:
                // Remove the stock wipers from the static model; the mod draws them again so they
                // can sweep when it rains.
                if (!e.hide.count(id) && wound.count(id)) e.hide[id] = wound[id];
                e.wipers.insert(id);
                e.black.erase(id);
                e.mirror.erase(id);
                e.skin.erase(id);
                break;
        }
    }
    static const char* kWhat[] = {"", "hidden", "black", "on the steering wheel", "off the steering wheel", "restored"};
    const char* what = cmd == 13 ? "marked as handbrake" : cmd == 14 ? "marked as shifter" :
                       cmd == 15 ? "control marks cleared" : cmd == 16 ? "marked as arms" :
                       cmd == 17 ? "arm marks cleared" : cmd == 18 ? "drawn as skin" :
                       cmd == 19 ? "merged as fingers (Arms/Skin/Show)" :
                       cmd == 20 ? "marked as mirror" : cmd == 21 ? "mirror marks cleared" :
                       cmd == 22 ? "marked as wipers" : cmd == 23 ? "wiper marks cleared" :
                       cmd == 24 ? "removed (animated when it rains)" : kWhat[cmd];
    Log("editor: %s: %u triangles %s", s_pick.name.c_str(), (unsigned)s_brush.size(), what);
    s_editsGen++;
    SaveEdits(s_pick.name);
    ApplyEditsTo(s_pick.name, wheel);
}

// For each player car solid as it is submitted (m: its matrix): its black triangles, picking and the
// selection. Once per solid per eye.
void NoteEditor(uint8_t* solid, SolidInfo& info, const float* m) {
    if (!s_eyeDone.insert(solid).second) return;
    void* ib = At<void*>(At<uint8_t*>(solid, kSolidPlat), kPlatIndexBuffer);
    UpdateBlack(solid, info);
    // In place of the originals (zero-area in the index buffer), 2 mm toward the eyes: clear of the
    // gauges' thin decal layers (needles, numbers) a fraction of a millimetre in front, which fought
    // with a copy just above the surface when not picked too.
    AddColor(ib, info.black, m, 0.002f, D3DCOLOR_ARGB(255, 0, 0, 0));
    if (!EditorLive() || !s_haveGaze) return;
    if (g_cfg.editPicking > 0.5f && s_eyeIndex == 0 && s_frameDone.insert(solid).second) PickIn(solid, info, m);
    if (solid != s_pick.solid || SolidName(info) != s_pick.name) return;
    if (info.trisDirty) BuildTris(solid, info);
    UpdateBrush(info);
    g_cfg.editPicked = (float)s_brush.size();
    std::set<uint64_t> brush(s_brush.begin(), s_brush.end());
    for (const TriRef& t : info.tris)
        if (brush.count(t.id))
            for (int k = 0; k < 3; k++) {
                V3 w = ToWorld(t.p[k], m);
                s_overlay.push_back(w.x);
                s_overlay.push_back(w.y);
                s_overlay.push_back(w.z);
            }
}

// After an eye's scene: the selection over everything, and the reticle where the gaze hits (or
// a metre ahead), a small cross facing the eyes.
void DrawEditorOverlay() {
    if (!EditorLive()) {
        s_overlay.clear();
        return;
    }
    if (!s_overlay.empty()) D3D_DrawOverlay(s_overlay.data(), (int)s_overlay.size() / 9, D3DCOLOR_ARGB(110, 255, 200, 0));
    s_overlay.clear();
    if (g_cfg.editPicking < 0.5f || !s_haveGaze) return;
    bool hit = s_pick.solid != nullptr;
    V3 c = hit ? s_pick.hitWorld : s_gazePos + s_gazeDir;
    float dist = sqrtf(Dot(Sub(c, s_gazePos), Sub(c, s_gazePos)));
    float r = std::max(0.002f, dist * 0.008f), t = r * 0.18f;
    V3 up = {0, 0, 1};
    V3 right = Cross(s_gazeDir, up);
    if (Dot(right, right) < 1e-6f) right = {1, 0, 0};
    right = Norm(right);
    up = Norm(Cross(right, s_gazeDir));
    std::vector<float> xyz;
    auto quad = [&](V3 a, V3 b, V3 cc, V3 d) {
        for (const V3& p : {a, b, cc, a, cc, d}) {
            xyz.push_back(p.x);
            xyz.push_back(p.y);
            xyz.push_back(p.z);
        }
    };
    quad(c - right * r + up * t, c + right * r + up * t, c + right * r - up * t, c - right * r - up * t);
    quad(c - right * t + up * r, c + right * t + up * r, c + right * t - up * r, c - right * t - up * r);
    D3D_DrawOverlay(xyz.data(), (int)xyz.size() / 9, hit ? D3DCOLOR_ARGB(255, 255, 60, 0) : D3DCOLOR_ARGB(255, 120, 120, 120));
}

// The seated driver's ranges set by OnSubmit: s_driverParts entries from s_driverEntry.
uint8_t* s_driverEntry;
int s_driverParts;

// Secondary mostly-arm materials are the separate native finger pieces. Share
// this test between visibility, skin material selection and IK membership.
bool AutomaticDriverFinger(const Driver& driver, int i) {
    if (i<=0 || i>=(int)driver.arms.size() || i>=(int)driver.body.size() ||
        i>=(int)driver.heads.size()) return false;
    int arms=driver.arms[i], total=driver.body[i]+arms+driver.heads[i];
    return arms>0 && arms*4>=total*3;
}
// The same entries are asked for every triangle of every driver draw (three to five passes an eye).
// The entries' index ranges are fixed for a given driver/mesh, so build them once and scan those.
struct DriverIndexRange { int begin, end; };
static std::vector<DriverIndexRange> s_fingerRanges, s_armRanges;
static const Driver *s_fingerDriver, *s_armDriver;
static uint8_t *s_fingerEntries, *s_armEntries;
static int s_fingerN = -1, s_armN = -1;

bool AutomaticDriverFingerIndex(const Driver& driver, uint8_t* entries, int n, int index) {
    if (&driver != s_fingerDriver || entries != s_fingerEntries || n != s_fingerN) {
        s_fingerDriver = &driver;
        s_fingerEntries = entries;
        s_fingerN = n;
        s_fingerRanges.clear();
        for (int i=1; i<n; ++i) {
            if (!AutomaticDriverFinger(driver,i)) continue;
            int begin=At<int>(entries+i*kEntrySize,kEntryFirstIndex)+driver.body[i]*3;
            s_fingerRanges.push_back({begin, begin + driver.arms[i]*3});
        }
    }
    for (const DriverIndexRange& r : s_fingerRanges)
        if (index>=r.begin && index<r.end) return true;
    return false;
}
// Full arm membership is independent of skin-material repair: jacket triangles
// must retain their original material. Only enable it for a fitted native rig.
bool AutomaticDriverArmIndex(const Driver& driver, uint8_t* entries, int n, int index) {
    if(!driver.rig.ok) return false;
    if (&driver != s_armDriver || entries != s_armEntries || n != s_armN) {
        s_armDriver = &driver;
        s_armEntries = entries;
        s_armN = n;
        s_armRanges.clear();
        for(int i=0;i<n && i<(int)driver.arms.size() && i<(int)driver.body.size();++i) {
            int begin=At<int>(entries+i*kEntrySize,kEntryFirstIndex)+driver.body[i]*3;
            s_armRanges.push_back({begin, begin + driver.arms[i]*3});
        }
    }
    for(const DriverIndexRange& r : s_armRanges)
        if(index>=r.begin && index<r.end) return true;
    return false;
}

// The triangles of the driver's hidden arms and head that the editor shows (EditSet::show), as runs
// of consecutive triangles per draw entry. Rebuilt when the edits or the hide switches change.
void UpdateShownRuns(uint8_t* solid, SolidInfo& info, Driver* d, bool hideArms, bool hideHead) {
    int key = s_editsGen * 4 + (hideArms ? 2 : 0) + (hideHead ? 1 : 0);
    if (d->runsKey == key && d->shownRuns.size() == d->body.size()) return;
    d->runsKey = key;
    d->shownRuns.assign(d->body.size(), {});
    const EditSet& e = EditsOf(info);
    uint8_t* plat = At<uint8_t*>(solid, kSolidPlat);
    uint8_t* entries = plat ? At<uint8_t*>(plat, kPlatEntries) : nullptr;
    const uint16_t* indices = plat ? At<uint16_t*>(plat, kPlatIndices) : nullptr;
    int numIndices = plat ? At<int>(plat, kPlatNumIndices) : 0;
    if (!entries || !indices) return;
    int shown = 0, dropped = 0;
    for (size_t i = 0; i < d->body.size(); i++) {
        int first = At<int>(entries + i * kEntrySize, kEntryFirstIndex);
        // The hidden parts' triangle ranges (in triangles from the entry's first index).
        int hidden[2][2] = {{0, 0}, {0, 0}};
        if (hideArms) hidden[0][0] = d->body[i], hidden[0][1] = d->arms[i];
        if (hideHead) hidden[1][0] = d->body[i] + d->arms[i], hidden[1][1] = d->heads[i];
        auto& runs = d->shownRuns[i];
        for (auto& h : hidden)
            for (int t = h[0]; t < h[0] + h[1]; t++) {
                int p = first + t * 3;
                if (p < 0 || p + 3 > numIndices) break;
                const uint16_t* v = indices + p;
                bool finger=!e.native.count(TriId(v[0],v[1],v[2])) && AutomaticDriverFinger(*d,(int)i) &&
                    t>=d->body[i] && t<d->body[i]+d->arms[i];
                if (!finger && !e.show.count(TriId(v[0], v[1], v[2]))) continue;
                shown++;
                if (!runs.empty() && runs.back().first + runs.back().second * 3 == p) runs.back().second++;
                else if ((int)runs.size() < kDriverPartSlots - 2) runs.push_back({p, 1});
                else dropped++;
            }
    }
    Log("cockpit: %s: %d hidden driver triangles shown by hand%s", SolidName(info).c_str(), shown,
        dropped ? " (some left out: too many separate pieces in one draw entry)" : "");
}

float DriverShiftAmount();
float DriverShiftTravel();

// The marked shifter and handbrake points of a solid, from its picker marks, cached by edits
// generation. Shared by the visible controls and the driver IK's handbrake target.
struct ControlMarks {
    int generation = -1;
    V3 shifter{}, brake{}, pivot{};
    bool shift = false, handbrake = false;
    uint8_t* vertices = nullptr;
    uint16_t* indices = nullptr;
    int numVertices = 0, numIndices = 0;
    uint32_t hash = 0;
    std::string name;
};

// Identify disconnected centre-console control geometry, welding UV/material
// seams first. Conservative dimensions reject the floor, seats and dashboard.
// No synthetic fixed target is supplied when the mesh has no suitable control.
void DetectConsoleControls(const SolidMesh& mesh, ControlMarks& picked) {
    std::vector<int> parent(mesh.numVerts);
    std::map<std::array<int,3>,int> welded;
    for(int i=0;i<mesh.numVerts;++i) parent[i]=i;
    auto root=[&](int i) { while(parent[i]!=i) { parent[i]=parent[parent[i]]; i=parent[i]; } return i; };
    for(int i=0;i<mesh.numVerts;++i) {
        V3 p=mesh.at(i);
        if(!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) continue;
        // Bound quantization before converting untrusted mesh coordinates.
        if(fabsf(p.x)>10 || fabsf(p.y)>10 || fabsf(p.z)>10) continue;
        std::array<int,3> key={{(int)lrintf(p.x*10000),(int)lrintf(p.y*10000),(int)lrintf(p.z*10000)}};
        auto inserted=welded.emplace(key,i);
        if(!inserted.second) parent[root(i)]=root(inserted.first->second);
    }
    std::set<int> used;
    for(int t=0;t+2<mesh.numIndices;t+=3) {
        int a=mesh.indices[t], b=mesh.indices[t+1], c=mesh.indices[t+2];
        if(a>=mesh.numVerts || b>=mesh.numVerts || c>=mesh.numVerts || a==b || b==c || a==c) continue;
        parent[root(b)]=root(a); parent[root(c)]=root(a);
        used.insert(a); used.insert(b); used.insert(c);
    }
    std::map<int,std::vector<int>> components;
    for(int i:used) components[root(i)].push_back(i);
    float shiftScore=-1, brakeScore=-1;
    bool needShift=!picked.shift, needBrake=!picked.handbrake;
    for(const auto& item:components) {
        const auto& ids=item.second;
        if(ids.size()<4) continue;
        V3 lo=mesh.at(ids[0]), hi=lo;
        for(int i:ids) {
            V3 p=mesh.at(i);
            lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};
            hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};
        }
        V3 extent=hi-lo, centre=(hi+lo)*0.5f;
        if(!std::isfinite(centre.x+centre.y+centre.z)) continue;
        bool shift=needShift && lo.x>0.10f && hi.x<0.65f && fabsf(centre.y)<0.13f &&
            lo.z>0.20f && hi.z<0.72f && extent.x>0.015f && extent.x<0.18f &&
            extent.y>0.015f && extent.y<0.18f && extent.z>0.025f && extent.z<0.27f;
        bool brake=needBrake && lo.x> -0.50f && hi.x<0.30f && fabsf(centre.y)<0.16f &&
            lo.z>0.20f && hi.z<0.65f && extent.x>0.12f && extent.x<0.42f &&
            extent.y<0.11f && extent.z<0.25f;
        if(shift && hi.z>shiftScore) {
            shiftScore=hi.z;
            // Average the knob's top cap, not a single arbitrary corner.
            V3 top{}; int count=0;
            for(int i:ids) { V3 p=mesh.at(i); if(p.z>=hi.z-0.015f) { top=top+p; ++count; } }
            picked.shifter=top*(1.0f/count); picked.shift=true;
        }
        if(brake && extent.x>brakeScore) {
            brakeScore=extent.x;
            // Forward end of a longitudinal lever is its grip; rear is hinge.
            V3 grip{}; int count=0;
            for(int i:ids) { V3 p=mesh.at(i); if(p.x>=hi.x-extent.x*0.25f) { grip=grip+p; ++count; } }
            picked.brake=grip*(1.0f/count); picked.brake.z=hi.z;
            picked.pivot={lo.x,centre.y,lo.z}; picked.handbrake=true;
        }
    }
}

const ControlMarks& ControlMarksFor(uint8_t* solid, SolidInfo& info) {
    static std::unordered_map<uint8_t*, ControlMarks> cache;
    ControlMarks& picked = cache[solid];
    SolidMesh mesh;
    if(!ReadMesh(solid,mesh)) { picked=ControlMarks{}; return picked; }
    if (picked.generation != s_editsGen || picked.vertices!=mesh.verts || picked.indices!=mesh.indices ||
        picked.numVertices!=mesh.numVerts || picked.numIndices!=mesh.numIndices ||
        picked.hash!=info.indexHash || picked.name!=SolidName(info)) {
        picked = ControlMarks{};
        picked.generation = s_editsGen;
        picked.vertices=mesh.verts; picked.indices=mesh.indices;
        picked.numVertices=mesh.numVerts; picked.numIndices=mesh.numIndices;
        picked.hash=info.indexHash; picked.name=SolidName(info);
        {
            const EditSet& edits = EditsFor(SolidName(info));
            float highestShift = -1e9f, highestBrake = -1e9f, lowest = 1e9f;
            for (int i = 0; i + 2 < mesh.numIndices; i += 3) {
                uint64_t id = TriId(mesh.indices[i], mesh.indices[i + 1], mesh.indices[i + 2]);
                bool shift = edits.shifter.count(id) != 0, brake = edits.handbrake.count(id) != 0;
                if (!shift && !brake) continue;
                for (int j = 0; j < 3; j++) {
                    if (mesh.indices[i + j] >= mesh.numVerts) continue;
                    V3 p = mesh.at(mesh.indices[i + j]);
                    if (shift && p.z > highestShift) { highestShift = p.z; picked.shifter = p; picked.shift = true; }
                    if (brake && p.z > highestBrake) { highestBrake = p.z; picked.brake = p; picked.handbrake = true; }
                    if (brake && p.z < lowest) { lowest = p.z; picked.pivot = p; }
                }
            }
            if(SolidName(info).find("_INTERIOR_A")!=std::string::npos) DetectConsoleControls(mesh,picked);
        }
    }
    return picked;
}

CustomDriver::Controls DriverControls(uint8_t* solid, SolidInfo& info, const float* matrix, const M4& world) {
    CustomDriver::Controls out;
    if (!info.wheel || !info.wheel->ok) return out;
    const Wheel& w = *info.wheel;
    // Exact same steering matrix as the visible wheel (including user pivot/location offsets).
    float turned[16];
    TurnedMatrix(w, 0, matrix, turned);
    auto local = [&](V3 p, const float* m) -> CustomDriver::Point {
        float rel[3];
        for (int a=0; a<3; a++) rel[a]=p.x*m[a]+p.y*m[4+a]+p.z*m[8+a]+m[12+a]-world.m[3][a];
        CustomDriver::Point result;
        float* dst=&result.x;
        for(int r=0;r<3;r++) {
            float len2=0; dst[r]=0;
            for(int a=0;a<3;a++) { dst[r]+=rel[a]*world.m[r][a]; len2+=world.m[r][a]*world.m[r][a]; }
            dst[r]/=std::max(len2,1e-8f);
        }
        return result;
    };
    V3 centre={w.centre[0],w.centre[1],w.centre[2]};
    out.centre=local(centre,turned);
    out.axis=CustomDriver::Unit(local(centre+V3{w.axis[0],w.axis[1],w.axis[2]},turned)-out.centre);
    out.radius=w.radius; out.degrees=DriverAnimationAngle(); out.wheel=true;
    out.handSpread=g_cfg.animatedDriverSpread;
    const ControlMarks& picked=ControlMarksFor(solid,info);
    out.hasShifter=picked.shift; out.hasHandbrake=picked.handbrake;
    out.shift=DriverShiftAmount(); out.brake=std::max(0.0f,std::min(1.0f,g_cockpitHandbrake));
    const EditSet& edits=EditsOf(info);
    CustomDriver::GateUnselectedControls(out,!edits.shifter.empty(),!edits.handbrake.empty());
    if(picked.shift) out.shifter=local(picked.shifter+V3{DriverShiftTravel(),0,0},matrix);
    if(picked.handbrake) {
        out.handbrake=local(picked.brake+V3{0,0,DriverHoverMotion::clearance},matrix);
    }
    return out;
}

// Returns the entry to submit (the first of the seated driver's ranges), or null for none.
uint8_t* __cdecl OnSubmit(uint8_t* model, const float* m, uint8_t* entry) {
    TraceDetailScope submitTiming(TRACE_COCKPIT_SUBMIT);
    uint8_t* solid = model ? At<uint8_t*>(model, kModelSolid) : nullptr;
    // Trace other cars/world geometry without preparing or editing their meshes.
    // A shared model name (or proximity to the camera) is not vehicle ownership.
    if (!DrawingPlayerCar()) {
        if (solid && Trace_On()) {
            char name[65] = {};
            memcpy(name, solid + kSolidName, 64);
            Trace_Model(name, s_curConn != nullptr, false,
                        entry ? (unsigned)std::max(0, At<int>(entry, kEntryTris)) : 0);
        }
        return entry;
    }
    SolidInfo* info = solid ? GetSolid(solid) : nullptr;
    if (info && Trace_On()) Trace_Model(info->name, s_curConn != nullptr, s_curConn && s_curPlayerCar,
                                      entry ? (unsigned)std::max(0, At<int>(entry, kEntryTris)) : 0);
    bool player = info && m && s_curConn && s_curPlayerCar;
    // Model names can exceed the string's inline storage; construct once, not
    // for each control/driver predicate in every split entry submission.
    const std::string playerName = player ? SolidName(*info) : std::string();
    const EditSet* playerEdits = player ? &EditsFor(playerName) : nullptr;
    // The car's window solids (windshield, rear and door glass): two-sided draws in the player
    // view, plus the interior tint whose opacity the menu controls.
    if (player && playerName.find("_WINDOW") != std::string::npos &&
        playerName.find("LIGHT") == std::string::npos) {
        void* ib = At<void*>(At<uint8_t*>(solid, kSolidPlat), kPlatIndexBuffer);
        if (ib) s_windowIbs.insert(ib);
        NoteWindowOverlay(solid, *info, m);
    }
    // Wipers marked in the editor: this solid's animated wiper draw (see Cockpit_WiperDraw).
    if (player && playerEdits && !playerEdits->wipers.empty()) {
        void* ib = At<void*>(At<uint8_t*>(solid, kSolidPlat), kPlatIndexBuffer);
        if (ib) s_wiperSolids[ib] = solid;
    }

    if (player && info->wheel && info->wheel->ok) {
        const Wheel& wheel=*info->wheel;
        float upLength=sqrtf(wheel.axis[0]*wheel.axis[0]+wheel.axis[2]*wheel.axis[2]);
        V3 pivot={wheel.centre[0],wheel.centre[1]-g_cfg.wheelPivot[0],wheel.centre[2]};
        if (upLength>1e-4f) {
            pivot.x-=wheel.axis[2]/upLength*g_cfg.wheelPivot[1];
            pivot.z+=wheel.axis[0]/upLength*g_cfg.wheelPivot[1];
        }
        M4 inv=MInvRigid(s_curBody);
        float moved[16]; TurnedMatrix(wheel,0,m,moved);
        s_nativeWheelPivot=ToWorld(ToWorld(pivot,moved),inv);
        s_nativeWheelOffset=s_nativeWheelPivot-ToWorld(ToWorld(pivot,m),inv);
        V3 end=ToWorld(ToWorld(pivot+V3{wheel.axis[0],wheel.axis[1],wheel.axis[2]},moved),inv);
        s_nativeWheelAxis=end-s_nativeWheelPivot;
        s_nativeWheelCar=s_head.worldId;
        s_nativeWheelValid=true;
    }
    // The player's driver is drawn with this matrix: mount the rig on it for Show IK.
    if (player && info->driver && info->driver->ok) {
        s_nativeDriverBody = s_curBody;
        memcpy(&s_driverIKWorld, m, sizeof(M4));
        s_driverIKWorldValid = true;
        Driver* fitted=info->driver;
        if(!fitted->rigTried) {
            fitted->rigTried=true;
            char path[MAX_PATH];
            snprintf(path,sizeof(path),"%sDRIVERMOD\\rigs\\%s.bin",GameDir(),fitted->car.c_str());
            if(playerName.find("_DRIVER_A")!=std::string::npos)
                DriverIK::LoadFile(path,&fitted->rig);
        }
        bool autoFingers=false;
        for (int i=1; i<(int)info->driver->arms.size(); ++i)
            autoFingers |= AutomaticDriverFinger(*info->driver,i);
        if (fitted->rig.ok || autoFingers || !playerEdits->arms.empty()) {
            void* ib = At<void*>(At<uint8_t*>(solid, kSolidPlat), kPlatIndexBuffer);
            s_armDraws[ib] = {solid, s_driverIKWorld};
        }
    }
    // Auto-detected console geometry is not enough to animate the driver's hands.
    // The control triangles must be selected explicitly for this car.
    if (player && playerName.find("_INTERIOR_A")!=std::string::npos) {
        if (playerEdits->shifter.empty()) s_shifterHoverValid = false;
        if (playerEdits->handbrake.empty()) s_handbrakeGripValid = false;
    }
    if (player && (!playerEdits->handbrake.empty() || !playerEdits->shifter.empty())) {
        const ControlMarks& picked = ControlMarksFor(solid, *info);
        if (picked.shift && !playerEdits->shifter.empty()) {
            float travel=DriverShiftTravel();
            s_shifterHoverWorld = ToWorld(picked.shifter + V3{travel,0,DriverHoverMotion::clearance}, m);
            s_shifterHoverValid = true;
            s_shifterVehicle=ToWorld(s_shifterHoverWorld,MInvRigid(s_curBody));
        }
        if (picked.handbrake && !playerEdits->handbrake.empty()) {
            s_handbrakeGripWorld = ToWorld(picked.brake + V3{0,0,DriverHoverMotion::clearance}, m);
            s_handbrakeGripValid = true;
            s_brakeVehicle=ToWorld(s_handbrakeGripWorld,MInvRigid(s_curBody));
        }
        if((picked.shift && !playerEdits->shifter.empty()) ||
           (picked.handbrake && !playerEdits->handbrake.empty())) {
            s_controlVehicleValid=true; s_controlVehicle=s_head.worldId;
        }
    }
    if (player && g_cfg.mirrors && g_cfg.disableMirrors <= 0.5f) NoteMirrors(solid, *info, m);
    if (player && info->wheel && info->wheel->dashOk && g_cfg.dash > 0.5f) {
        float origin[3], right[3], up[3], toEye[3];
        DashBasis(*info->wheel, m, origin, right, up, toEye);
        D3D_AddDash(At<void*>(At<uint8_t*>(solid, kSolidPlat), kPlatIndexBuffer), origin, right, up, toEye);
    }
    if (player) NoteEditor(solid, *info, m);
    if (player && m && info->wheel && info->wheel->ok && s_seatedConn && s_head.valid && g_cfg.showCustomDriverModel > 0.5f) {
        M4 world = s_seatedBody;
        for (int a = 0; a < 3; a++)
            world.m[3][a] += s_head.body[0]*world.m[0][a] + s_head.body[1]*world.m[1][a] + s_head.body[2]*world.m[2][a];
        D3D_AddCustomDriver(At<void*>(At<uint8_t*>(solid, kSolidPlat), kPlatIndexBuffer), &world.m[0][0], DriverControls(solid, *info, m, world));
    }
    Driver* d = info ? info->driver : nullptr;
    if (!d || !d->ok || !m) return entry;
    const float* h = d->head;
    float world[3];
    for (int a = 0; a < 3; a++) world[a] = h[0] * m[a] + h[1] * m[4 + a] + h[2] * m[8 + a] + m[12 + a];

    // Seat measurement: the head in the game camera's space.
    if (s_haveBase && s_curConn && s_curPlayerCar) {
        float rel[3], cam[3];
        for (int a = 0; a < 3; a++) rel[a] = world[a] - s_base.m[3][a];
        for (int r = 0; r < 3; r++) cam[r] = rel[0] * s_base.m[r][0] + rel[1] * s_base.m[r][1] + rel[2] * s_base.m[r][2];
        float dist = sqrtf(cam[0] * cam[0] + cam[1] * cam[1] + cam[2] * cam[2]);
        if (dist < s_frame.dist) {
            s_frame.dist = dist;
            memcpy(s_frame.cam, cam, sizeof(cam));
            if (s_frame.car != d->car) s_frame.car = d->car;
            s_frame.worldId = At<uint32_t>(s_curConn, kConnWorldId);
            if (s_curConn) {
                // Rows 0..2 of the body matrix are its axes: project onto them.
                const M4& b = s_curBody;
                for (int r = 0; r < 3; r++) {
                    float len2 = b.m[r][0] * b.m[r][0] + b.m[r][1] * b.m[r][1] + b.m[r][2] * b.m[r][2];
                    float dot = (world[0] - b.m[3][0]) * b.m[r][0] + (world[1] - b.m[3][1]) * b.m[r][1] +
                                (world[2] - b.m[3][2]) * b.m[r][2];
                    s_frame.body[r] = len2 > 1e-8f ? dot / len2 : 0.0f;
                }
            }
        }
    }

    bool hideArms = g_cfg.hideArms > 0.5f;
    if (!g_cfg.hideHead && !hideArms) return entry;
    void* cam = At<void*>((void*)kPlayerView, kViewCamera);
    if (!cam || Dist(world, &At<float>(cam, kCamPosition)) > kHeadNear) return entry;
    uint8_t* entries = At<uint8_t*>(At<uint8_t*>(solid, kSolidPlat), kPlatEntries);
    int i = (int)(entry - entries) / kEntrySize;
    if (entry < entries || i >= (int)d->body.size()) return entry;
    // The seated driver: body, then the arms unless hidden, then the head unless hidden, then the
    // hidden parts' triangles shown by hand. Copied from the live entry each time, so its vertex
    // buffer and data are the current ones.
    int first = At<int>(entry, kEntryFirstIndex);
    std::vector<std::pair<int, int>> ranges;
    ranges.push_back({first, d->body[i] + (hideArms ? 0 : d->arms[i])});
    if (!g_cfg.hideHead) {
        if (hideArms) ranges.push_back({first + (d->body[i] + d->arms[i]) * 3, d->heads[i]});
        else ranges[0].second += d->heads[i];
    }
    UpdateShownRuns(solid, *info, d, hideArms, g_cfg.hideHead);
    if (i < (int)d->shownRuns.size())
        for (auto& r : d->shownRuns[i]) ranges.push_back(r);
    s_driverParts = 0;
    for (auto& r : ranges) {
        if (r.second <= 0 || s_driverParts >= kDriverPartSlots) continue;
        uint8_t* part = &d->parts[(i * kDriverPartSlots + s_driverParts++) * kEntrySize];
        memcpy(part, entry, kEntrySize);
        At<int>(part, kEntryFirstIndex) = r.first;
        At<int>(part, kEntryTris) = r.second;
        At<int>(part, kEntryNumIndices) = r.second * 3;
    }
    if (!s_driverParts) return nullptr;
    s_driverEntry = &d->parts[i * kDriverPartSlots * kEntrySize];
    return s_driverEntry;
}

// Whether the car drawn now in the player view is the player's own (the camera's anchor).
bool DrawingPlayerCar() {
    return s_curConn && s_curPlayerCar;
}

// sub_6E0AB0 queues an entry for drawing, keeping pointers to it and to the matrix (its 6th
// argument, eViewPlatInterface::Render's local-to-world matrix) until the queue is drawn: turned
// matrices live in a ring of slots that outlasts a frame.
typedef void(__cdecl* SubmitEntry_t)(uint8_t* entry, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5,
                                     const float* matrix, uint32_t a7, uint32_t a8, uint32_t a9);
uint8_t* s_submitModel;
// The matrices the player car's entries were queued with in this eye (the depth split, game.cpp:
// the queue is drawn later through sub_6C8000 with the same matrix pointers).
const void* s_carMatrices[64];
int s_carMatrixCount;

void MarkPlayerCarMatrix(const float* matrix) {
    if (!matrix || !DrawingPlayerCar()) return;
    for (int i = 0; i < s_carMatrixCount; i++)
        if (s_carMatrices[i] == matrix) return;
    if (s_carMatrixCount < 64) s_carMatrices[s_carMatrixCount++] = matrix;
}
float s_turned[256][16];
unsigned s_turnedNext;

struct ControlDraw {
    uint8_t entry[kEntrySize];
    float matrix[16];
};
// Queue pointers must survive until both eyes have consumed the game's draw queue.
std::deque<ControlDraw> s_controlDraws;
CockpitControlMotion s_controlMotion;
double ControlTime() { return GetTickCount64() * 0.001; }
float DriverShiftTravel() { return s_controlMotion.Shift(ControlTime()); }
float DriverShiftAmount() {
    double t=(ControlTime()-s_controlMotion.shiftAt)/(0.22/1.5);
    if(s_controlMotion.shiftAt<0 || t<=0 || t>=1) return 0;
    float s=sinf(float(t)*3.14159265f);
    return s*s;
}

bool SubmitControls(uint8_t* entry, uint8_t* solid, const float* matrix,
                    uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5,
                    uint32_t a7, uint32_t a8, uint32_t a9) {
    if (!solid || !matrix || !DrawingPlayerCar()) return false;
    auto found = s_solids.find(solid);
    if (found == s_solids.end()) return false;
    const EditSet& edits = EditsOf(found->second);
    if (edits.shifter.empty()) return false;
    SolidMesh mesh;
    if (!ReadMesh(solid, mesh)) return false;
    int first = At<int>(entry, kEntryFirstIndex), count = At<int>(entry, kEntryTris);
    if (first < 0 || count <= 0 || first > mesh.numIndices || count > (mesh.numIndices - first) / 3) return false;
    auto kind = [&](int index) {
        uint64_t id = TriId(mesh.indices[index], mesh.indices[index + 1], mesh.indices[index + 2]);
        return edits.handbrake.count(id) ? 0 : edits.shifter.count(id) ? 2 : 0;
    };
    bool marked = false;
    for (int t = 0; t < count; t++) marked |= kind(first + t * 3) != 0;
    if (!marked) return false;
    float shift = s_controlMotion.Shift(ControlTime());
    for (int begin = 0; begin < count;) {
        int part = kind(first + begin * 3), end = begin + 1;
        while (end < count && kind(first + end * 3) == part) end++;
        s_controlDraws.emplace_back();
        ControlDraw& draw = s_controlDraws.back();
        memcpy(draw.entry, entry, kEntrySize);
        memcpy(draw.matrix, matrix, sizeof(draw.matrix));
        At<int>(draw.entry, kEntryFirstIndex) = first + begin * 3;
        At<int>(draw.entry, kEntryTris) = end - begin;
        At<int>(draw.entry, kEntryNumIndices) = (end - begin) * 3;
        for (int col = 0; col < 3; col++) {
            if (part == 2) draw.matrix[12 + col] += shift * matrix[col];
        }
        MarkPlayerCarMatrix(draw.matrix);
        ((SubmitEntry_t)kSubmitEntry)(draw.entry, a2, a3, a4, a5, draw.matrix, a7, a8, a9);
        begin = end;
    }
    return true;
}

// 6DAD0E passes a stack array of five resolved texture-info pointers as a5;
// a4 is the resolved shader and a8 the auxiliary binding. Raw entry bytes alone
// do not change those arguments. Keep geometry/UVs and route bindings separately.
bool SkinParts(uint8_t* entry, uint8_t* solid, std::vector<std::array<uint8_t, kEntrySize>>& parts,
               std::vector<bool>* skinRuns = nullptr) {
    auto found = s_solids.find(solid);
    if (found == s_solids.end() || !found->second.driver) return false;
    const auto& edits = EditsOf(found->second);
    const auto& skin = edits.skin;
    SolidMesh mesh;
    if (!ReadMesh(solid, mesh) || mesh.n < 1) return false;
    int first = At<int>(entry, kEntryFirstIndex), count = At<int>(entry, kEntryTris);
    if (first < 0 || count <= 0 || first > mesh.numIndices || count > (mesh.numIndices-first)/3) return false;
    // Some cars give the fingers their own DRIVER_PLAYER material, which can
    // resolve black despite the main driver's atlas being valid. Prepare has
    // already classified these secondary, mostly-arm entries. Use this solid's
    // main material, not a cached material from another car or LOD. Match index
    // ranges because entry can be a copied hidden-head/animated draw range.
    const Driver& driver = *found->second.driver;
    auto marked = [&](int t) {
        const uint16_t* v = mesh.indices + first + t*3;
        uint64_t id=TriId(v[0],v[1],v[2]);
        return skin.count(id)!=0 || (!edits.native.count(id) && !edits.black.count(id) &&
            AutomaticDriverFingerIndex(driver,mesh.entries,mesh.n,first+t*3));
    };
    bool any = false;
    for (int t=0; t<count; ++t) any |= marked(t);
    if (!any) return false;
    for (int begin=0; begin<count;) {
        bool useSkin = marked(begin);
        int end = begin+1;
        while (end<count && marked(end)==useSkin) ++end;
        parts.emplace_back();
        if (skinRuns) skinRuns->push_back(useSkin);
        uint8_t* part = parts.back().data();
        memcpy(part, entry, kEntrySize);
        if (useSkin) memcpy(part, mesh.entries, 0x18);
        At<int>(part,kEntryFirstIndex) = first+begin*3;
        At<int>(part,kEntryTris) = end-begin;
        At<int>(part,kEntryNumIndices) = (end-begin)*3;
        begin=end;
    }
    return true;
}

void CaptureDriverSkin(uint8_t* solid, uint8_t* entry, uint32_t shader, uint32_t textures, uint32_t aux) {
    auto it = s_solids.find(solid);
    SolidMesh mesh;
    if (it == s_solids.end() || !it->second.driver || !ReadMesh(solid, mesh) || entry != mesh.entries) return;
    Driver& driver = *it->second.driver;
    driver.skinBindingValid = shader && textures;
    if (!driver.skinBindingValid) return;
    driver.skinShader = shader;
    driver.skinAux = aux;
    memcpy(driver.skinTextures.data(), reinterpret_cast<const void*>(textures), sizeof(driver.skinTextures));

}

void __cdecl SubmitEntry(uint8_t* entry, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, const float* matrix,
                         uint32_t a7, uint32_t a8, uint32_t a9) {
    // Do not split an opponent's entries merely because its solid was previously
    // cached for the player (two instances can share the same model resources).
    if (!DrawingPlayerCar()) {
        OnSubmit(s_submitModel, matrix, entry); // retain model/category tracing
        ((SubmitEntry_t)kSubmitEntry)(entry, a2, a3, a4, a5, matrix, a7, a8, a9);
        return;
    }
    auto submit = [](uint8_t* entry, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, const float* matrix,
                     uint32_t a7, uint32_t a8, uint32_t a9) {
        uint8_t* solid = s_submitModel ? At<uint8_t*>(s_submitModel, kModelSolid) : nullptr;
        std::vector<std::array<uint8_t,kEntrySize>> skinParts;
        std::vector<bool> skinRuns;
        if (DrawingPlayerCar() && SkinParts(entry,solid,skinParts,&skinRuns)) {
            MarkPlayerCarMatrix(matrix);
            Driver& driver = *s_solids.find(solid)->second.driver;
            for (size_t i = 0; i < skinParts.size(); ++i) {
                s_controlDraws.emplace_back();
                ControlDraw& draw = s_controlDraws.back();
                memcpy(draw.entry,skinParts[i].data(),kEntrySize);
                bool skin = skinRuns[i] && driver.skinBindingValid;
                ((SubmitEntry_t)kSubmitEntry)(draw.entry,a2,a3,skin ? driver.skinShader : a4,
                    skin ? reinterpret_cast<uint32_t>(driver.skinTextures.data()) : a5,
                    matrix,a7,skin ? driver.skinAux : a8,a9);
            }
            return;
        }
        if (SubmitControls(entry, solid, matrix, a2, a3, a4, a5, a7, a8, a9)) return;
        MarkPlayerCarMatrix(matrix);
        ((SubmitEntry_t)kSubmitEntry)(entry, a2, a3, a4, a5, matrix, a7, a8, a9);
    };
    uint8_t* model = s_submitModel;
    s_driverEntry = nullptr;
    uint8_t* e = OnSubmit(model, matrix, entry);
    // Capture even when the main entry was entirely hidden by the seated view.
    if (DrawingPlayerCar() && model)
        CaptureDriverSkin(At<uint8_t*>(model,kModelSolid),entry,a4,a5,a8);
    if (!e) return;
    if (e == s_driverEntry) {
        for (int k = 0; k < s_driverParts; k++) submit(e + k * kEntrySize, a2, a3, a4, a5, matrix, a7, a8, a9);
        return;
    }
    uint8_t* solid = model ? At<uint8_t*>(model, kModelSolid) : nullptr;
    auto it = solid && e == entry ? s_solids.find(solid) : s_solids.end();
    Wheel* w = it != s_solids.end() ? it->second.wheel : nullptr;
    float angle = g_steeringWheelAngle;
    bool wheelOverHud = g_cfg.wheelOverHud > 0.5f;
    bool wheelMoved = g_cfg.wheelPosition[0] != 0 || g_cfg.wheelPosition[1] != 0;
    if (w && w->ok && matrix && (fabsf(angle) > 0.05f || wheelOverHud || wheelMoved) && DrawingPlayerCar()) {
        uint8_t* entries = At<uint8_t*>(At<uint8_t*>(solid, kSolidPlat), kPlatEntries);
        int i = (int)(entry - entries) / kEntrySize;
        if (entry >= entries && i < (int)w->turned.size() && w->turned[i] > 0) {
            // Copied from the live entry each time, like the seated driver's ranges.
            uint8_t* rest = &w->parts[(i * 2) * kEntrySize];
            uint8_t* wheel = &w->parts[(i * 2 + 1) * kEntrySize];
            memcpy(rest, entry, kEntrySize);
            memcpy(wheel, entry, kEntrySize);
            At<int>(rest, kEntryTris) = w->kept[i];
            At<int>(rest, kEntryNumIndices) = w->kept[i] * 3;
            At<int>(wheel, kEntryFirstIndex) = At<int>(entry, kEntryFirstIndex) + w->kept[i] * 3;
            At<int>(wheel, kEntryTris) = w->turned[i];
            At<int>(wheel, kEntryNumIndices) = w->turned[i] * 3;
            float* turned = s_turned[s_turnedNext++ % 256];
            TurnedMatrix(*w, angle, matrix, turned);
            if (w->kept[i] > 0) submit(rest, a2, a3, a4, a5, matrix, a7, a8, a9);
            if (wheelOverHud)
                D3D_RegisterWheelDraw(At<void*>(At<uint8_t*>(solid, kSolidPlat), kPlatIndexBuffer), At<int>(wheel, kEntryFirstIndex));
            submit(wheel, a2, a3, a4, a5, turned, a7, a8, a9);
            return;
        }
    }
    GlassSplit* split = it != s_solids.end() ? it->second.split : nullptr;
    if (split) {
        uint8_t* entries = At<uint8_t*>(At<uint8_t*>(solid, kSolidPlat), kPlatEntries);
        int i = (int)(entry - entries) / kEntrySize;
        for (size_t k = 0; k < split->entries.size(); k++) {
            const GlassSplit::Entry& se = split->entries[k];
            if (se.index != i) continue;
            int first = At<int>(entry, kEntryFirstIndex), offset = 0;
            for (int part = 0; part < 3; part++) {
                int count = se.count[part];
                if (!count) continue;
                uint8_t* pe = &split->parts[(k * 3 + part) * kEntrySize];
                memcpy(pe, entry, kEntrySize);
                At<int>(pe, kEntryFirstIndex) = first + offset * 3;
                At<int>(pe, kEntryTris) = count;
                At<int>(pe, kEntryNumIndices) = count * 3;
                submit(pe, a2, a3, a4, a5, matrix, a7, a8, a9);
                offset += count;
            }
            return;
        }
    }
    submit(e, a2, a3, a4, a5, matrix, a7, a8, a9);
}

// Replaces eViewPlatInterface::Render's call of sub_6E0AB0, keeping its arguments: the model
// (Render's first argument) is passed on the side.
__declspec(naked) void hkSubmitEntry() {
    __asm {
        mov eax, dword ptr [ebp + 8]
        mov s_submitModel, eax
        jmp SubmitEntry
    }
}

bool PatchCall(uintptr_t site, uintptr_t target, void* hook) {
    uint8_t* p = (uint8_t*)site;
    if (p[0] != 0xE8 || site + 5 + *(int32_t*)(p + 1) != target) {
        Log("cockpit: unexpected code at 0x%X, not patched", (unsigned)site);
        return false;
    }
    DWORD old;
    VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old);
    *(int32_t*)(p + 1) = (int32_t)((uintptr_t)hook - (site + 5));
    VirtualProtect(p, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 5);
    return true;
}

bool GameFocused() {
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
}

bool Held(int vk) { return vk > 0 && vk < 256 && (GetAsyncKeyState(vk) & 0x8000); }

}  // namespace

// Read by the gameplay mod through NfsmwVr_Setting("WiperSweep" / "WiperStrike").
float g_wiperSweep = 0.0f;
float g_wiperStrikes = 0.0f;

bool Cockpit_HasMarkedArmDraw(void* ib) {
    return (NativeSteeringActive() || s_armHover.progress > 0.0001f) && s_armDraws.count(ib) != 0;
}

bool Cockpit_IsRegisteredDriverDraw(void* ib) {
    return s_armDraws.count(ib) != 0;
}

bool Cockpit_IsSkinDraw(void* ib, unsigned first, unsigned triangles) {
    auto draw = s_armDraws.find(ib);
    if (draw == s_armDraws.end() || !triangles) return false;
    auto info = s_solids.find(draw->second.solid);
    SolidMesh mesh;
    if (info == s_solids.end() || !info->second.driver || !ReadMesh(draw->second.solid,mesh) ||
        first % 3 || first > (unsigned)mesh.numIndices ||
        triangles > ((unsigned)mesh.numIndices-first)/3) return false;
    const auto& edits = EditsOf(info->second);
    for (unsigned t=0; t<triangles; ++t) {
        const uint16_t* v = mesh.indices+first+t*3;
        uint64_t id = TriId(v[0],v[1],v[2]);
        if (edits.skin.count(id)) continue;
        if (edits.native.count(id) || edits.black.count(id) ||
            !AutomaticDriverFingerIndex(*info->second.driver,mesh.entries,mesh.n,first+t*3)) return false;
    }
    return true;
}

bool Cockpit_DriverDrawWork(void* ib, unsigned first, unsigned triangles, int base,
                            std::vector<uint16_t>& indices) {
    indices.clear();
    auto draw=s_armDraws.find(ib);
    if (draw==s_armDraws.end() || !triangles) return false;
    auto info=s_solids.find(draw->second.solid);
    SolidMesh mesh;
    if (info==s_solids.end() || !info->second.driver || !ReadMesh(draw->second.solid,mesh) ||
        first>(unsigned)mesh.numIndices || triangles>((unsigned)mesh.numIndices-first)/3) return false;
    const auto& edits=EditsOf(info->second);
    bool pose=Cockpit_HasMarkedArmDraw(ib), needed=false;
    for (unsigned t=0;t<triangles;++t) {
        const uint16_t* v=mesh.indices+first+t*3;
        uint64_t id=TriId(v[0],v[1],v[2]);
        bool automatic=!edits.native.count(id) && AutomaticDriverFingerIndex(*info->second.driver,mesh.entries,mesh.n,first+t*3);
        if (edits.skin.count(id) || (automatic && !edits.black.count(id)) ||
            (pose && (automatic || (!edits.native.count(id) && AutomaticDriverArmIndex(*info->second.driver,mesh.entries,mesh.n,first+t*3)) || edits.arms.count(id)))) { needed=true; break; }
    }
    if (!needed) return false;
    indices.reserve((size_t)triangles*3);
    for (unsigned i=0;i<triangles*3;++i) {
        long long index=(long long)mesh.indices[first+i]+base;
        if (index<0 || index>65535) { indices.clear(); return false; }
        indices.push_back((uint16_t)index);
    }
    return true;
}

bool Cockpit_RepairSkinUV(void* ib, unsigned first, unsigned triangles, int base,
                          unsigned stride, unsigned uvOffset, unsigned char* vertices, size_t bytes) {
    auto draw=s_armDraws.find(ib);
    if (draw==s_armDraws.end() || !vertices || stride<8 || uvOffset>stride-8) return false;
    auto info=s_solids.find(draw->second.solid);
    SolidMesh mesh;
    if (info==s_solids.end() || !info->second.driver || !ReadMesh(draw->second.solid,mesh) ||
        first>(unsigned)mesh.numIndices || triangles>((unsigned)mesh.numIndices-first)/3) return false;
    const auto& edits=EditsOf(info->second);
    std::vector<unsigned char> visited(bytes/stride,0);
    std::vector<unsigned> selected;
    for (unsigned t=0;t<triangles;++t) {
        const uint16_t* v=mesh.indices+first+t*3;
        uint64_t id=TriId(v[0],v[1],v[2]);
        if (!edits.skin.count(id) && (edits.native.count(id) || edits.black.count(id) ||
            !AutomaticDriverFingerIndex(*info->second.driver,mesh.entries,mesh.n,first+t*3))) continue;
        for (int j=0;j<3;++j) {
            long long index=(long long)v[j]+base;
            if (index>=0 && (unsigned long long)index<bytes/stride && !visited[(size_t)index]) {
                visited[(size_t)index]=1;
                selected.push_back((unsigned)index);
            }
        }
    }
    bool changed=false;
    std::sort(selected.begin(),selected.end());
    for (unsigned index:selected) {
        unsigned char* dst=vertices+(size_t)index*stride+uvOffset+4;
        float v; memcpy(&v,dst,sizeof(v));
        // Captured native hand islands encode V in [-1,0); the diffuse sampler
        // clamps, so decode the island rather than changing global addressing.
        if (v>=-1.0f && v<0.0f) {
            v+=1.0f; memcpy(dst,&v,sizeof(v)); changed=true;
        }
    }
    return changed;
}

bool Cockpit_DriverDrawInfo(void* ib, unsigned first, unsigned triangles,
                           char* name, size_t capacity, int* entry) {
    auto draw = s_armDraws.find(ib);
    if (draw == s_armDraws.end() || !name || !capacity || !entry || !triangles) return false;
    auto info = s_solids.find(draw->second.solid);
    SolidMesh mesh;
    if (info == s_solids.end() || !ReadMesh(draw->second.solid, mesh) ||
        first > (unsigned)mesh.numIndices || triangles > ((unsigned)mesh.numIndices-first)/3) return false;
    for (int i=0; i<mesh.n; ++i) {
        uint8_t* e=mesh.entries+i*kEntrySize;
        int begin=At<int>(e,kEntryFirstIndex), count=At<int>(e,kEntryTris);
        if (begin<0 || count<=0 || first<(unsigned)begin) continue;
        unsigned offset=first-(unsigned)begin;
        if (offset/3 >= (unsigned)count || offset%3 || triangles>(unsigned)count-offset/3) continue;
        snprintf(name,capacity,"%s",SolidName(info->second).c_str());
        *entry=i;
        return true;
    }
    return false;
}

// The visibility classifier is deliberately broad; it is not a skinning mask.
// Limit automatic deformation to the fitted arm capsules, with fixed shoulder
// roots and no influence below the elbow/wrist envelope (lap and jeans).
float NativeArmWeight(const DriverIK::Rig& rig, CustomDriver::Point p) {
    using namespace CustomDriver;
    float weight=0;
    for(int side=0;side<2;++side) {
        auto point=[](const float* v){return Point{v[0],v[1],v[2]};};
        Point shoulder=point(rig.bones[rig.arms[side*3]].head);
        Point elbow=point(rig.bones[rig.arms[side*3+1]].head);
        Point wrist=point(rig.bones[rig.arms[side*3+2]].head);
        if(p.z<std::min(elbow.z,wrist.z)-0.07f) continue;
        // Body-side vertices at the armpit remain anchored, unlike sleeves.
        float lateral=side ? shoulder.y-p.y : p.y-shoulder.y;
        if(lateral < -0.055f && p.x<elbow.x-0.06f) continue;
        for(int j=0;j<3;++j) {
            const auto& bone=rig.bones[rig.arms[side*3+j]];
            Point a=point(bone.head), d=point(bone.tail)-a;
            float t=std::max(0.0f,std::min(1.0f,Dot(p-a,d)/std::max(1e-10f,Dot(d,d))));
            float distance=Length(p-(a+d*t));
            float radius=j==0 ? 0.085f : 0.075f;
            float w=1-DriverSteering::Phase(distance,radius-0.02f,radius);
            if(j==0) w*=DriverSteering::Phase(t,0.05f,0.35f);
            weight=std::max(weight,w);
        }
    }
    return weight;
}

#include "driver_binding_cache.h"

bool Cockpit_PoseMarkedArms(void* ib, unsigned first, unsigned triangles, int base,
                           unsigned stride, unsigned positionOffset, int normalOffset,
                           unsigned char* vertices, size_t bytes) {
    TraceDetailScope poseTiming(TRACE_DRIVER_POSE);
    auto draw = s_armDraws.find(ib);
    if (draw == s_armDraws.end() || !vertices || stride < 12 || positionOffset + 12 > stride ||
        (!NativeSteeringActive() && s_armHover.progress <= 0.0001f)) return false;
    const DriverIK::Rig* rig = DriverRig();
    if (!rig) return false;
    SolidMesh mesh;
    if (!ReadMesh(draw->second.solid, mesh) || first > (unsigned)mesh.numIndices ||
        triangles > ((unsigned)mesh.numIndices - first) / 3) return false;
    auto info = s_solids.find(draw->second.solid);
    if (info == s_solids.end()) return false;
    const auto& edits = EditsOf(info->second);
    const auto& marks = edits.arms;
    auto point = [](const float* p) { return CustomDriver::Point{p[0], p[1], p[2]}; };
    int candidates[2][4];
    for(int side=0;side<2;++side) {
        for(int j=0;j<3;++j) candidates[side][j]=rig->arms[side*3+j];
        candidates[side][3]=rig->Find(side ? "JNT_hand_tip.R" : "JNT_hand_tip.L");
    }
    auto buildBinding = [&](CustomDriver::Point p) {
        DriverBindingCache::Binding result;
        result.weight=NativeArmWeight(*rig,p);
        float best=1e9f;
        for(int side=0;side<2;++side) for(int b:candidates[side]) if(b>=0) {
            auto a=point(rig->bones[b].head), d=point(rig->bones[b].tail)-a;
            float t=std::max(0.0f,std::min(1.0f,CustomDriver::Dot(p-a,d)/
                std::max(1e-10f,CustomDriver::Dot(d,d))));
            float distance=CustomDriver::Length(p-(a+d*t));
            if(distance<best) { best=distance; result.bone=b; result.side=side; }
        }
        return result;
    };
    static DriverBindingCache bindings;
    bindings.Prepare(*rig);
    // The triangle classification depends only on the mesh, edits, rig and draw range, not on the
    // per-frame steering pose. It used to be rebuilt every frame, walking every triangle of the
    // driver's draws with two set lookups each; keep it until one of those inputs changes.
    struct PoseSelectionEntry {
        void* ib = nullptr;
        unsigned first = 0, count = 0;
        int base = 0, editsGen = -1;
        unsigned worldId = 0;                  // the car connection (guards pointer/address reuse)
        size_t armsSize = 0, nativeSize = 0;   // direct edits that predate a generation bump
        const Driver* driver = nullptr;
        const uint8_t* entries = nullptr;
        const uint16_t* indices = nullptr;
        int numVerts = 0, numIndices = 0;
        const DriverIK::Rig* rig = nullptr;
        bool rigOk = false;
        std::unique_ptr<DriverPoseSelection> selection;
    };
    static std::vector<PoseSelectionEntry> s_poseSelections;
    const Driver* poseDriver = info->second.driver;
    const bool poseRigOk = poseDriver && poseDriver->rig.ok;
    DriverPoseSelection* poseSelection = nullptr;
    for (const PoseSelectionEntry& e : s_poseSelections)
        if (e.ib == ib && e.first == first && e.count == triangles && e.base == base &&
            e.editsGen == s_editsGen && e.worldId == s_head.worldId &&
            e.armsSize == marks.size() && e.nativeSize == edits.native.size() &&
            e.driver == poseDriver && e.entries == mesh.entries &&
            e.indices == mesh.indices && e.numVerts == mesh.numVerts && e.numIndices == mesh.numIndices &&
            e.rig == rig && e.rigOk == poseRigOk) {
            poseSelection = e.selection.get();
            break;
        }
    if (!poseSelection) {
        if (s_poseSelections.size() >= 4) s_poseSelections.erase(s_poseSelections.begin());
        s_poseSelections.push_back(PoseSelectionEntry{ib, first, triangles, base, s_editsGen, s_head.worldId,
            marks.size(), edits.native.size(), poseDriver,
            mesh.entries, mesh.indices, mesh.numVerts, mesh.numIndices, rig, poseRigOk,
            std::make_unique<DriverPoseSelection>((unsigned)std::max(0, mesh.numVerts))});
        DriverPoseSelection& selection = *s_poseSelections.back().selection;
    for (unsigned t = 0; t < triangles; ++t) {
        const uint16_t* v = mesh.indices + first + t * 3;
        bool manual=marks.count(TriId(v[0], v[1], v[2]))!=0;
        if (manual || (!edits.native.count(TriId(v[0],v[1],v[2])) && info->second.driver &&
            (AutomaticDriverFingerIndex(*info->second.driver,mesh.entries,mesh.n,first+t*3) ||
             AutomaticDriverArmIndex(*info->second.driver,mesh.entries,mesh.n,first+t*3))))
            for (int j = 0; j < 3; ++j) if ((int)v[j] + base >= 0) {
                selection.Add(v[j],manual);
            }
        if(manual && info->second.driver && info->second.driver->rig.ok) {
            auto p0=point(mesh.pos(v[0])), p1=point(mesh.pos(v[1])), p2=point(mesh.pos(v[2]));
            auto binding=buildBinding((p0+p1+p2)*(1.0f/3.0f));
            if(binding.side>=0 &&
               (binding.bone==rig->arms[binding.side*3+2] ||
                binding.bone==candidates[binding.side][3])) {
                for(int j=0;j<3;++j) {
                    auto p=point(mesh.pos(v[j]));
                    auto vertexBinding=bindings.Get(v[j],p,[&]() { return buildBinding(p); });
                    selection.SetBoneOverride(v[j],vertexBinding.bone,vertexBinding.side,2);
                }
            } else if(binding.side>=0 &&
               (binding.bone==rig->arms[binding.side*3] ||
                binding.bone==rig->arms[binding.side*3+1])) {
                int upper=rig->arms[binding.side*3], fore=rig->arms[binding.side*3+1];
                int hand=rig->arms[binding.side*3+2], tip=candidates[binding.side][3];
                for(int j=0;j<3;++j) {
                    auto p=point(mesh.pos(v[j]));
                    auto vertexBinding=bindings.Get(v[j],p,[&]() { return buildBinding(p); });
                    if(vertexBinding.bone!=hand && vertexBinding.bone!=tip) continue;
                    float best=1e9f;
                    int sleeveBone=-1;
                    for(int b:{upper,fore}) if(b>=0) {
                        auto a=point(rig->bones[b].head), d=point(rig->bones[b].tail)-a;
                        float t=std::max(0.0f,std::min(1.0f,CustomDriver::Dot(p-a,d)/
                            std::max(1e-10f,CustomDriver::Dot(d,d))));
                        float distance=CustomDriver::Length(p-(a+d*t));
                        if(distance<best) { best=distance; sleeveBone=b; }
                    }
                    if(sleeveBone>=0) selection.SetBoneOverride(v[j],sleeveBone,binding.side,1);
                }
            }
        }
    }
    // Manual priority is recorded during selection, independent of triangle order.
    // Shared sleeve/body seam vertices must not drag adjacent torso triangles.
    // Inspect all entries, not just this draw's subrange.
    if(info->second.driver && info->second.driver->rig.ok) {
        Driver& driver=*info->second.driver;
        std::vector<int> layout;
        int entries=std::min(mesh.n,(int)std::min(driver.body.size(),driver.arms.size()));
        layout.reserve((size_t)std::max(0,entries)*4);
        for(int i=0;i<entries;++i) {
            uint8_t* entry=mesh.entries+i*kEntrySize;
            layout.push_back(At<int>(entry,kEntryFirstIndex));
            layout.push_back(At<int>(entry,kEntryTris));
            layout.push_back(driver.body[i]);
            layout.push_back(driver.arms[i]);
        }
        const auto& excluded=driver.seamCache.Get(mesh.indices,(size_t)mesh.numIndices,
            (unsigned)std::max(0,mesh.numVerts),layout,edits.native,
            [&](std::vector<unsigned char>& mask) {
        for(int i=0;i<entries;++i) {
            uint8_t* entry=mesh.entries+i*kEntrySize;
            int begin=At<int>(entry,kEntryFirstIndex), count=At<int>(entry,kEntryTris);
            if(begin<0 || count<0 || begin>mesh.numIndices || count>(mesh.numIndices-begin)/3) continue;
            for(int t=0;t<count;++t) {
                const uint16_t* v=mesh.indices+begin+t*3;
                if(t>=driver.body[i] && t<driver.body[i]+driver.arms[i] &&
                    !edits.native.count(TriId(v[0],v[1],v[2]))) continue;
                for(int j=0;j<3;++j) if(v[j]<mask.size()) mask[v[j]]=1;
            }
        }
        });
        for(unsigned local:selection.Ordered())
            if(local<excluded.size() && excluded[local]) selection.ExcludeAutomatic(local);
    }
        selection.Ordered();
        poseSelection = &selection;
    }
    DriverPoseSelection& selection = *poseSelection;
    std::vector<V3> head, tail;
    DriverArmPose(draw->second.world, head, tail);
    if (head.empty()) return false;
    const bool steering = NativeSteeringActive();
    const int control = ControlSide();
    DriverSteering::Grip grips[2]{};
    float forearmRoll[2]{};
    CustomDriver::Point wheelAxis{};
    if (steering) {
        M4 inv=MInvRigid(draw->second.world);
        V3 axis=ToWorld(ToWorld(s_nativeWheelPivot+s_nativeWheelAxis,s_nativeDriverBody),inv)-
            ToWorld(ToWorld(s_nativeWheelPivot,s_nativeDriverBody),inv);
        wheelAxis={axis.x,axis.y,axis.z};
    }
    for (int side=0;side<2;++side) {
        for (int j=0;j<3;++j) candidates[side][j]=rig->arms[side*3+j];
        candidates[side][3]=rig->Find(side ? "JNT_hand_tip.R" : "JNT_hand_tip.L");
        if (!steering) continue;
        grips[side]=NativeGrip(*rig,side,draw->second.world);
        int fore=rig->arms[side*3+1], hand=rig->arms[side*3+2];
        if (fore<0 || hand<0 || (side==control && (s_armControlBlend>0 || s_armControlOverride))) continue;
        auto restDir=point(rig->bones[fore].tail)-point(rig->bones[fore].head);
        CustomDriver::Point posedDir{tail[fore].x-head[fore].x,tail[fore].y-head[fore].y,tail[fore].z-head[fore].z};
        auto restHand=point(rig->bones[hand].tail)-point(rig->bones[hand].head);
        CustomDriver::Point posedHand{tail[hand].x-head[hand].x,tail[hand].y-head[hand].y,tail[hand].z-head[hand].z};
        forearmRoll[side]=DriverSteering::SleeveCuffRoll(
            side,restDir,posedDir,restHand,posedHand,wheelAxis,grips[side].roll);
    }
    std::vector<CustomDriver::PreparedRotation> rotations;
    rotations.reserve(rig->bones.size());
    for(size_t b=0;b<rig->bones.size();++b)
        rotations.emplace_back(point(rig->bones[b].tail)-point(rig->bones[b].head),
            CustomDriver::Point{tail[b].x-head[b].x,tail[b].y-head[b].y,tail[b].z-head[b].z});
    CustomDriver::PreparedTwist handTwists[2]={
        {wheelAxis,grips[0].roll*(control==0 ? 1-s_armControlBlend : 1)},
        {wheelAxis,grips[1].roll*(control==1 ? 1-s_armControlBlend : 1)}};
    CustomDriver::PreparedRotation handRotations[2]={
        {{1,0,0},{1,0,0}},{{1,0,0},{1,0,0}}};
    CustomDriver::PreparedRotation tipRotations[2]={
        {{1,0,0},{1,0,0}},{{1,0,0},{1,0,0}}};
    if(steering) for(int side=0;side<2;++side) {
        int hand=rig->arms[side*3+2];
        if(hand<0) continue;
        auto restDir=point(rig->bones[hand].tail)-point(rig->bones[hand].head);
        CustomDriver::Point posedDir{tail[hand].x-head[hand].x,tail[hand].y-head[hand].y,tail[hand].z-head[hand].z};
        handRotations[side]=CustomDriver::PreparedRotation(handTwists[side].Apply(restDir),posedDir);
        int tip=candidates[side][3];
        if(tip>=0) {
            auto tipRest=point(rig->bones[tip].tail)-point(rig->bones[tip].head);
            CustomDriver::Point tipDir{tail[tip].x-head[tip].x,tail[tip].y-head[tip].y,tail[tip].z-head[tip].z};
            tipRotations[side]=CustomDriver::PreparedRotation(handTwists[side].Apply(tipRest),tipDir);
        }
    }
    // Rest-space classification is independent of animation and editor marks.
    // Exact rig/point comparisons also handle same-address asset reloads.
    bool changed = false;
    for (unsigned local : selection.Ordered()) {
        if (!selection.IsSelected(local)) continue;
        unsigned v=local+base;
        int localVertex = (int)local;
        if ((size_t)v * stride + positionOffset + 12 > bytes || localVertex < 0 || localVertex >= mesh.numVerts) continue;
        auto p = point(mesh.pos(localVertex));
        auto binding=bindings.Get(local,p,[&]() { return buildBinding(p); });
        DriverPoseSelection::BoneOverride sleeveBinding;
        if(selection.GetBoneOverride(local,sleeveBinding)) {
            binding.bone=sleeveBinding.bone;
            binding.side=sleeveBinding.side;
        }
        float automaticWeight=selection.IsAutomatic(local) && info->second.driver && info->second.driver->rig.ok ?
            binding.weight : 1.0f;
        if(automaticWeight<=0) continue;
        int nearest=(binding.side==control || steering) ? binding.bone : -1;
        if (nearest < 0) continue;
        int b = nearest;
        auto restHead = point(rig->bones[b].head), restDir = point(rig->bones[b].tail)-restHead;
        CustomDriver::Point posedHead{head[b].x,head[b].y,head[b].z};
        CustomDriver::Point posedDir{tail[b].x-head[b].x,tail[b].y-head[b].y,tail[b].z-head[b].z};
        float cuffRoll=0;
        if(steering) for(int side=0;side<2;++side) {
            if(b!=rig->arms[side*3+1] || (side==control && (s_armControlBlend>0 || s_armControlOverride))) continue;
            float along=CustomDriver::Dot(p-restHead,restDir)/std::max(1e-10f,CustomDriver::Dot(restDir,restDir));
            cuffRoll=forearmRoll[side]*DriverSteering::CuffWeight(along);
        }
        auto deformDirection = [&](CustomDriver::Point value) {
            if (steering) for (int side=0;side<2;++side) {
                if (b!=rig->arms[side*3+2] && b!=candidates[side][3]) continue;
                // Hand-tip bones may have a different rest direction from the hand.
                if(b==rig->arms[side*3+2])
                    return handRotations[side].Apply(handTwists[side].Apply(value));
                return tipRotations[side].Apply(handTwists[side].Apply(value));
            }
            auto rotated=rotations[b].Apply(value);
            return cuffRoll!=0 ? CustomDriver::Twist(rotated,posedDir,cuffRoll) : rotated;
        };
        float* dst = (float*)(vertices + (size_t)v*stride + positionOffset);
        auto result = CustomDriver::Mix(p,posedHead + deformDirection(p-restHead),automaticWeight);
        memcpy(dst,&result,12);
        if (normalOffset >= 0 && (unsigned)normalOffset+12 <= stride) {
            float* normal = (float*)(vertices+(size_t)v*stride+normalOffset);
            auto original=point(normal);
            auto rotated = CustomDriver::Unit(CustomDriver::Mix(original,deformDirection(original),automaticWeight));
            memcpy(normal,&rotated,12);
        }
        changed = true;
    }
    return changed;
}

// The values a car model with none of its own starts from: the mod's built-in defaults, captured
// before the config is read. Called once from DllMain before LoadConfig (the F4 menu writes the
// current car's values to [VR]) and before any car is seen or any layout auto-places.
void Cockpit_PerCarDefaults() {
    for (PerCar* p : {&s_hudCar, &s_dashCar, &s_dashLayoutCar, &s_mirrorCar, &s_pivotCar, &s_wheelPositionCar, &s_animatedDriverCar})
        for (int i = 0; i < p->n; i++) p->base[i] = *p->v[i];
    Log("cockpit: per-car defaults captured");
}

void Cockpit_Install() {
    if (!PatchCall(kCallSubmitEntry, kSubmitEntry, (void*)&hkSubmitEntry)) return;
    PatchCall(kCallCarCameraInside, kBoxIsInside, (void*)&hkCarCameraInside);
    for (uintptr_t site : kCallsPartVisible) PatchCall(site, kPlatVisibleState, (void*)&hkPartVisible);
    void** slot = (void**)kCarRenderConnVtbl + kVtblOnRender;
    if (*slot == (void*)kOnRender) {
        DWORD old;
        VirtualProtect(slot, 4, PAGE_READWRITE, &old);
        *slot = (void*)&hkOnRender;
        VirtualProtect(slot, 4, old, &old);
    } else {
        Log("cockpit: unexpected CarRenderConn::OnRender, body-locked seat unavailable");
    }
    Log("cockpit: installed (seat %s, hide head %s)", g_cfg.seat ? "on" : "off", g_cfg.hideHead ? "on" : "off");
}

bool Cockpit_ApplySeat(M4& view) {
    s_seatedConn = nullptr;
    s_base = MInvRigid(view);
    s_haveBase = true;
    if (!g_cfg.seat || !s_head.valid || Now() - s_head.time > 0.5) return false;
    float d = sqrtf(s_head.cam[0] * s_head.cam[0] + s_head.cam[1] * s_head.cam[1] + s_head.cam[2] * s_head.cam[2]);
    if (d > kSeatRange) return false;
    if (s_head.car != s_offsetCar) {
        if (s_offsetDirty) SaveOffset();
        LoadOffset(s_head.car);
    }
    uint8_t* anchor = ViewAnchor((uint8_t*)kPlayerView);
    if (!anchor || At<uint32_t>(anchor, kAnchorWorldId) != s_head.worldId) return false;
    // The previous car may have unloaded. Resolve the current connection by id, rather than
    // inspecting yesterday's pointer (which may already be freed or reused by a traffic car).
    auto find = (uint8_t*(__cdecl*)(uint32_t))kFindConn;
    uint8_t* conn = find(s_head.worldId);
    if (!conn || At<uintptr_t>(conn, 0) != kCarRenderConnVtbl) return false;
    M4 w = s_base;
    {
        // Fixed to the car body as it is drawn this frame. Camera rows: right = -left, down = -up,
        // forward; the body's rows are forward, left, up.
        M4 b = BodyMatrix(conn, DrawsWithPov(conn, (uint8_t*)kPlayerView));
        s_seatedBody = b;
        const int row[3] = {1, 2, 0};
        const float sgn[3] = {-1, -1, 1};
        for (int i = 0; i < 3; i++) {
            const float* r = b.m[row[i]];
            float len = sqrtf(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
            if (len < 1e-6f) return false;
            for (int a = 0; a < 3; a++) w.m[i][a] = sgn[i] * r[a] / len;
        }
        // The head, then the seat offset (right, up, forward) along the car's axes.
        const float* h = s_head.body;
        float local[3] = {h[0] + s_offset[2], h[1] - s_offset[0], h[2] + s_offset[1]};
        for (int a = 0; a < 3; a++)
            w.m[3][a] = b.m[3][a] + local[0] * b.m[0][a] + local[1] * b.m[1][a] + local[2] * b.m[2][a];
    }
    s_seatedConn = conn;
    view = MInvRigid(w);
    return true;
}

void Cockpit_EndFrame() {
    TraceDetailScope frameTiming(TRACE_COCKPIT_FRAME);
    static double armTime = Now();
    double now = Now();
    float dt = (float)std::max(0.0, std::min(0.1, now - armTime));
    armTime = now;
    UpdateDriverAnimationSmoothing(dt);
    UpdateDriverHover(dt);
    // Triangle editor: while picking, the candidate along the gaze chosen with up / down (layer,
    // nearest first) is the pick; otherwise the last one stays (locked) for the page's commands.
    if (g_cfg.editPicking > 0.5f) {
        std::sort(s_hits.begin(), s_hits.end(), [](const Pick& a, const Pick& b) { return a.dist < b.dist; });
        int n = (int)s_hits.size();
        int layer = std::max(0, std::min(n - 1, (int)lrintf(g_cfg.editLayer)));
        if (n && g_cfg.editLayer > (float)(n - 1)) g_cfg.editLayer = (float)(n - 1);
        Pick next = n ? s_hits[layer] : Pick();
        if (next.solid != s_pick.solid || next.id != s_pick.id) s_brushFor = 0;
        s_pick = next;
        if (!n) g_cfg.editPicked = 0;
    } else if (!EditorLive()) {
        s_pick = Pick();
    }
    s_hits.clear();
    s_frameDone.clear();
    s_eyeIndex = -1;
    s_hashSerial++;
    s_controlDraws.clear();
    s_controlMotion.Update(ControlTime(), g_cockpitShiftUp, g_cockpitShiftDown, s_head.valid, s_shifterHandReady);
    int cmd = (int)lrintf(g_cfg.editCommand);
    if (cmd) {
        g_cfg.editCommand = 0;
        if ((cmd >= 1 && cmd <= 6) || (cmd >= 13 && cmd <= 24)) RunEditCommand(cmd);
        if (cmd == 10) CentreWheelPivot();                             // F4 > Menu and testing
        if (cmd == 11) g_cfg.wheelPivot[0] = g_cfg.wheelPivot[1] = 0;
        if (cmd == 12) CopySelection();
        if (cmd == 9) {   // picking cancelled: no selection
            s_pick = Pick();
            s_brush.clear();
            s_brushFor = 0;
            g_cfg.editPicked = 0;
        }
    }
    for (Driver* d : s_retired) delete d;
    s_retired.clear();
    for (Wheel* w : s_retiredWheels) delete w;
    s_retiredWheels.clear();
    for (GlassSplit* g : s_retiredSplits) delete g;
    s_retiredSplits.clear();
    s_bodyMismatch = 0;
    s_bodyDraws = 0;
    if (s_frame.dist < 1e8f) {
        // The seat is a fixed point in the body. Re-measuring it each frame makes LOD changes
        // and different head meshes move the eyes around inside the cockpit.
        bool newCar = !s_head.valid || s_head.worldId != s_frame.worldId || s_head.car != s_frame.car;
        s_head.valid = true;
        memcpy(s_head.cam, s_frame.cam, sizeof(s_head.cam));
        s_head.car = s_frame.car;
        s_head.worldId = s_frame.worldId;
        if (newCar) memcpy(s_head.body, s_frame.body, sizeof(s_head.body));
        s_head.time = Now();
    }
    UpdateHudCar(s_head.valid ? s_head.car : std::string());
    s_frame.dist = 1e9f;
    s_frame.worldId = 0;
    s_seatedConn = nullptr;
    s_haveBase = false;
}

void Cockpit_Keys() {
    static double last = Now();
    double now = Now();
    float dt = (float)std::min(now - last, 0.1);
    last = now;
    if (!g_cfg.seat || s_offsetCar.empty()) return;
    // GetForegroundWindow + GetWindowThreadProcessId every frame for keys that are mostly idle:
    // 250 ms of staleness is invisible for a held seat key.
    static bool focused;
    static double focusAt;
    if (now - focusAt > 0.25) {
        focusAt = now;
        focused = GameFocused();
    }
    if (!focused) return;
    bool reset = Held(g_cfg.keySeatReset) || g_seatInput[3] > 0.5f;
    if (reset) {
        s_offset[0] = s_offset[1] = s_offset[2] = 0;
        s_offsetDirty = true;
    }
    const int keys[3][2] = {{g_cfg.keySeatLeft, g_cfg.keySeatRight},
                            {g_cfg.keySeatDown, g_cfg.keySeatUp},
                            {g_cfg.keySeatBack, g_cfg.keySeatForward}};
    bool moving = false;
    for (int a = 0; a < 3; a++) {
        int dir = (Held(keys[a][1]) ? 1 : 0) - (Held(keys[a][0]) ? 1 : 0);
        if (!dir && g_seatInput[a] != 0.0f) dir = g_seatInput[a] > 0 ? 1 : -1;
        if (!dir) continue;
        s_offset[a] = std::max(-kSeatLimit, std::min(kSeatLimit, s_offset[a] + dir * kSeatSpeed * dt));
        moving = s_offsetDirty = true;
    }
    if (s_offsetDirty && !moving && !reset) SaveOffset();
}

bool Cockpit_IsPlayerCarMatrix(const void* matrix) {
    for (int i = 0; i < s_carMatrixCount; i++)
        if (s_carMatrices[i] == matrix) return true;
    return false;
}

bool Cockpit_IsWindowDraw(void* indexBuffer) { return WindowDraw(indexBuffer); }

// --- wipers ---------------------------------------------------------------------------------------
// The wiper triangles the editor selected. They are hidden in the game's static model ("Remove
// wipers") and drawn again by the d3d9 side, rotated around the hinge the editor's selection gives
// (the lowest point) about the blade's own normal, sweeping while the player view's rain is on.
struct WiperDrawData {
    bool valid = false;
    std::vector<uint16_t> tris;   // original windings, three vertices each
    std::vector<unsigned> verts;  // unique vertices to rotate
    int entryFirst = -1, entryCount = 0;
    float pivot[3] = {0, 0, 0}, axis[3] = {0, 0, 1};
};
static std::unordered_map<void*, WiperDrawData> s_wiperCache;

// Rain intensity of the player view (0 = dry), read from the game's Rain object.
float GameRainIntensity() {
    constexpr uintptr_t kPlayerRain = 0x9196B8;
    constexpr int kRainIntensity = 0x28C;
    uintptr_t rain = *(uintptr_t*)kPlayerRain;
    if (rain < 0x10000 || rain >= 0x7FFF0000) return 0.0f;
    float v = *(float*)(rain + kRainIntensity);
    return v >= 0.0f && v <= 10.0f ? v : 0.0f;
}

bool Cockpit_WiperDraw(void* ib, unsigned first, unsigned count, std::vector<uint16_t>& indices,
                       std::vector<unsigned>& verts, float pivot[3], float axis[3], float* angle) {
    auto sit = s_wiperSolids.find(ib);
    if (sit == s_wiperSolids.end()) return false;
    uint8_t* solid = sit->second;
    auto infoIt = s_solids.find(solid);
    if (infoIt == s_solids.end()) return false;
    SolidInfo& info = infoIt->second;
    const EditSet& edits = EditsOf(info);
    if (edits.wipers.empty()) return false;
    // The static wipers are removed first ("Remove wipers"); the animated copy replaces them.
    bool removed = false;
    for (uint64_t id : edits.wipers) removed |= edits.hide.count(id) != 0;
    if (!removed) return false;
    static int cacheGen = -1;
    if (cacheGen != s_editsGen) {
        s_wiperCache.clear();
        cacheGen = s_editsGen;
    }
    WiperDrawData& c = s_wiperCache[ib];
    if (!c.valid) {
        SolidMesh mesh;
        if (!ReadMesh(solid, mesh)) return false;
        if (info.trisDirty) BuildTris(solid, info);
        c.valid = true;
        std::set<unsigned> unique;
        float normal[3] = {0, 0, 0}, minZ = 1e9f;
        V3 low{};
        size_t k = 0;
        for (int i = 0; i < mesh.n && k < info.tris.size(); i++) {
            uint8_t* e = mesh.entries + i * kEntrySize;
            int ef = At<int>(e, kEntryFirstIndex), et = At<int>(e, kEntryTris);
            for (int t = 0; t < et && k < info.tris.size(); t++, k++) {
                if (!edits.wipers.count(info.tris[k].id)) continue;
                std::array<uint16_t, 3> v = {info.tris[k].v[0], info.tris[k].v[1], info.tris[k].v[2]};
                auto hv = edits.hide.find(info.tris[k].id);
                if (hv != edits.hide.end()) v = hv->second;
                if (c.entryFirst < 0) c.entryFirst = ef, c.entryCount = et;
                if (ef != c.entryFirst || v[0] >= mesh.numVerts || v[1] >= mesh.numVerts || v[2] >= mesh.numVerts)
                    continue;
                for (int j = 0; j < 3; j++) {
                    c.tris.push_back(v[j]);
                    unique.insert(v[j]);
                }
                auto P = [](const float* f) { return V3{f[0], f[1], f[2]}; };
                V3 p0 = P(mesh.pos(v[0])), p1 = P(mesh.pos(v[1])), p2 = P(mesh.pos(v[2]));
                V3 n = Cross(Sub(p1, p0), Sub(p2, p0));
                normal[0] += n.x, normal[1] += n.y, normal[2] += n.z;
                for (const V3& p : {p0, p1, p2})
                    if (p.z < minZ) minZ = p.z, low = p;
            }
        }
        if (c.tris.empty()) { c.valid = false; return false; }
        c.verts.assign(unique.begin(), unique.end());
        float len = sqrtf(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
        if (len > 1e-6f) c.axis[0] = normal[0] / len, c.axis[1] = normal[1] / len, c.axis[2] = normal[2] / len;
        c.pivot[0] = low.x, c.pivot[1] = low.y, c.pivot[2] = low.z;
    }
    if (c.tris.empty() || first != (unsigned)c.entryFirst || count != (unsigned)c.entryCount) return false;
    indices = c.tris;
    verts = c.verts;
    memcpy(pivot, c.pivot, sizeof(c.pivot));
    memcpy(axis, c.axis, sizeof(c.axis));
    // A sweep from the rest position to about 50 degrees and back while it rains; parked when dry.
    constexpr float kPi = 3.14159265f;
    float rain = GameRainIntensity();
    float phase = (float)(Now() * 2.0 * kPi * 0.9);
    *angle = rain > 0.02f ? 0.9f * (0.5f - 0.5f * cosf(phase)) : 0.0f;
    g_wiperSweep = *angle;
    // One count per half sweep (out and back): the gameplay mod plays Stitch_Static_MB_1 on the
    // out stroke and _2 on the return, alternating every stroke.
    if (rain > 0.02f) {
        int half = (int)(phase / kPi);
        if (s_wiperHalf < 0) s_wiperHalf = half;
        if (half != s_wiperHalf) {
            s_wiperHalf = half;
            g_wiperStrikes += 1.0f;
        }
    } else {
        s_wiperHalf = -1;
    }
    return true;
}



int Cockpit_PlayerCarMatrices() { return s_carMatrixCount; }

void Cockpit_BeginEye() {
    s_armDraws.clear();
    s_carMatrixCount = 0;
    s_mirrorSolids.clear();
    s_manualMirrorSolids.clear();
    s_windowIbs.clear();
    s_windowOverlaySolids.clear();
    s_wiperSolids.clear();
    s_wiperCache.clear();
    s_eyeDone.clear();
    s_driverIKWorldValid = false;
    s_handbrakeGripValid = false;
    s_shifterHoverValid = false;
    s_controlVehicleValid = false;
    s_eyeIndex++;
}

void Cockpit_EndEye() {
    DrawEditorOverlay();
    DrawDriverIK();
}

void Cockpit_SetGaze(const float pos[3], const float dir[3]) {
    s_gazePos = {pos[0], pos[1], pos[2]};
    s_gazeDir = Norm({dir[0], dir[1], dir[2]});
    s_haveGaze = Dot(s_gazeDir, s_gazeDir) > 0.5f;
}
