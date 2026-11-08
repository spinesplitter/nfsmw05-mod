#include "../src/frametrace.cpp"
#include <cassert>
#include <cmath>
#include <initializer_list>

static double clockSeconds = 1;
static bool benchmarkClock;
double D3D_ProfNow() {
    if (!benchmarkClock) return clockSeconds;
    static LARGE_INTEGER frequency = [] { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f; }();
    LARGE_INTEGER counter; QueryPerformanceCounter(&counter);
    return double(counter.QuadPart) / frequency.QuadPart;
}
void Log(const char*, ...) {}
const char* GameDir() { return "./"; }
void Game_TraceInfo(int* faces, int* rear, int* shadow) { *faces = *rear = *shadow = 0; }

int main() {
    s.cur = Row{};
    { TraceDetailScope scope(TRACE_MIRROR_MAP); clockSeconds += 1; }
    assert(s.cur.detail[TRACE_MIRROR_MAP] == 0);
    s.on = true;
    { TraceDetailScope outer(TRACE_MIRROR_DRAW);
      { TraceDetailScope inner(TRACE_MIRROR_MAP); clockSeconds += 0.002; }
      clockSeconds += 0.001;
    }
    assert(std::fabs(s.cur.detail[TRACE_MIRROR_MAP] - 2) < 0.00001);
    assert(std::fabs(s.cur.detail[TRACE_MIRROR_DRAW] - 3) < 0.00001);
    Trace_Detail(TRACE_MIRROR_RESTORE, 0.001);
    Trace_Detail(TRACE_MIRROR_RESTORE, 0.002);
    assert(std::fabs(s.cur.detail[TRACE_MIRROR_RESTORE] - 3) < 0.00001);
    { TraceDetailScope scope(TRACE_RENDER_SETUP); clockSeconds += .004; }
    { TraceDetailScope scope(TRACE_PREDISPLAY); clockSeconds += .005; }
    assert(std::fabs(s.cur.detail[TRACE_RENDER_SETUP] - 4) < .00001);
    assert(std::fabs(s.cur.detail[TRACE_PREDISPLAY] - 5) < .00001);
    for (int detail = TRACE_DRIVER_DRAW; detail < TRACE_DETAIL_COUNT; ++detail) {
        { TraceDetailScope scope(static_cast<TraceDetail>(detail)); clockSeconds += .006; }
        assert(std::fabs(s.cur.detail[detail] - 6) < .00001);
    }
    // Exercise the actual CSV writer: every detail must match its named column.
    char oldDirectory[MAX_PATH], tempDirectory[MAX_PATH], testDirectory[MAX_PATH];
    assert(GetCurrentDirectoryA(MAX_PATH, oldDirectory));
    assert(GetTempPathA(MAX_PATH, tempDirectory));
    assert(GetTempFileNameA(tempDirectory, "trc", 0, testDirectory));
    assert(DeleteFileA(testDirectory));
    assert(CreateDirectoryA(testDirectory, nullptr));
    assert(SetCurrentDirectoryA(testDirectory));
    Row* csvRows = (Row*)calloc(2, sizeof(Row));
    assert(csvRows);
    csvRows[0].t = 1;
    csvRows[0].envFaces = 1;
    for (int d = 0; d < TRACE_DETAIL_COUNT; ++d) csvRows[1].detail[d] = 100 + d;
    WriteCsv(csvRows);
    FILE* csv = fopen("nfsmw_frametrace_001.csv", "r");
    assert(csv);
    char header[4096], values[4096];
    assert(fgets(header, sizeof(header), csv) && fgets(values, sizeof(values), csv));
    fclose(csv);
    const char* detailNames[] = {"mirror_draw_cpu_ms", "mirror_map_cpu_ms", "mirror_restore_cpu_ms",
        "hud_phase_cpu_ms", "player_car_cpu_ms", "other_car_cpu_ms", "solid_cache_cpu_ms",
        "render_setup_cpu_ms", "predisplay_cpu_ms", "driver_draw_cpu_ms", "driver_pose_cpu_ms",
        "driver_readback_cpu_ms", "driver_upload_cpu_ms", "cockpit_submit_cpu_ms", "cockpit_frame_cpu_ms",
        "dash_draw_cpu_ms", "dash_compose_cpu_ms", "hud_probe_cpu_ms", "custom_driver_cpu_ms"};
    static_assert(sizeof(detailNames)/sizeof(detailNames[0]) == TRACE_DETAIL_COUNT, "CSV detail names");
    char *nameContext = nullptr, *valueContext = nullptr;
    char* name = strtok_s(header, ",\r\n", &nameContext);
    char* value = strtok_s(values, ",\r\n", &valueContext);
    int detailsSeen = 0;
    while (name && value) {
        for (int d = 0; d < TRACE_DETAIL_COUNT; ++d) if (!strcmp(name, detailNames[d])) {
            assert(std::fabs(atof(value) - (100 + d)) < .00001);
            ++detailsSeen;
        }
        name = strtok_s(nullptr, ",\r\n", &nameContext);
        value = strtok_s(nullptr, ",\r\n", &valueContext);
    }
    assert(!name && !value && detailsSeen == TRACE_DETAIL_COUNT);
    assert(DeleteFileA("nfsmw_frametrace_001.csv"));
    assert(DeleteFileA("nfsmw_frametrace_001_models.csv"));
    assert(DeleteFileA("nfsmw_frametrace_001_passes.csv"));
    assert(SetCurrentDirectoryA(oldDirectory));
    assert(RemoveDirectoryA(testDirectory));
    puts("CSV detail ordering tests passed: driver, cockpit, dashboard and HUD diagnostics");
    s.on = false;
    Trace_Detail(TRACE_HUD_PHASE, 1);
    assert(s.cur.detail[TRACE_HUD_PHASE] == 0);
    Trace_Model("TRAFSEDAN", true, false, 12);
    Trace_CacheRebuild();
    assert(s.cur.objectSubmits[OBJ_TRAFFIC_HINT] == 0 && s.cur.cacheRebuilds == 0);
    s.on = true;
    Trace_Model("trafsedan", true, false, 12);
    Trace_Model("TRAFSEDAN", true, false, 8);
    Trace_Model("BMWM3", true, true, 30);
    Trace_Model("BMWM3", true, false, 40);
    Trace_Model("COPSPORT", true, false, 50);
    Trace_Model("break_glass", false, false, 3);
    Trace_Model("world,\"mesh\n", false, false, 2);
    Trace_CacheRebuild();
    assert(s.cur.objectSubmits[OBJ_TRAFFIC_HINT] == 2 && s.cur.objectTriangles[OBJ_TRAFFIC_HINT] == 20);
    assert(s.cur.objectSubmits[OBJ_PLAYER] == 1 && s.cur.objectSubmits[OBJ_OTHER_CAR] == 1);
    assert(s.cur.objectSubmits[OBJ_COP_HINT] == 1 && s.cur.objectSubmits[OBJ_BREAKABLE_HINT] == 1);
    assert(s.cur.cacheRebuilds == 1);
    assert(!strcmp(s.cur.models[OBJ_OTHER * kSamplesPerCategory].name, "WORLD__MESH_"));
    for (unsigned i = 0; i < kSamplesPerCategory + 4; ++i) {
        char name[32]; snprintf(name, sizeof(name), "WORLD_%u", i);
        Trace_Model(name, false, false, 1);
    }
    assert(s.cur.modelOverflow == 5);
    assert(s.cur.objectSubmits[OBJ_OTHER] == kSamplesPerCategory + 5);
    // Full world samples do not crowd out another category.
    Trace_Model("DEBRIS_NEW", false, false, 6);
    assert(s.cur.models[OBJ_BREAKABLE_HINT * kSamplesPerCategory + 1].submits == 1);
    s.cur = Row{};
    assert(s.cur.cacheRebuilds == 0 && s.cur.objectSubmits[OBJ_PLAYER] == 0);
    puts("Frame trace detail tests passed: disabled, nested and accumulated timings");
    s.cur = Row{};
    s.on = false;
    Trace_RenderBegin(0); clockSeconds += .001; Trace_RenderEnd();
    assert(s.cur.spans[0].calls == 0);
    s.on = true;
    Trace_RenderBegin(0);
    clockSeconds += .002; Trace_RenderBoundary(0x6DE749);
    clockSeconds += .003; Trace_RenderBoundary(0x6DE749);
    clockSeconds += .004; Trace_RenderEnd();
    Trace_RenderBegin(1);
    clockSeconds += .005; Trace_RenderEnd();
    double total = 0;
    for (const auto& span : s.cur.spans) total += span.ms;
    assert(std::fabs(total - 14) < .00001);
    assert(s.cur.spans[0].from == 0 && s.cur.spans[0].eye == 0);
    assert(s.cur.spans[1].from == 0x6DE749 && s.cur.spans[1].to == 0x6DE749);
    assert(s.cur.spans[3].eye == 1 && s.cur.spans[3].to == 0xFFFFFFFFu);
    s.cur = Row{};
    Trace_RenderBegin(0);
    for (unsigned i = 1; i <= kRenderSpans + 3; ++i) Trace_RenderBoundary(i);
    Trace_RenderEnd();
    assert(s.cur.spanOverflow == 4);
    puts("Render partition tests passed: disabled, loops, independent eyes, conservation, bounded overflow");
    // Local timing only: no game/GPU workload, so not a live tracing-overhead result.
    benchmarkClock = true;
    for (bool enabled : {false, true}) {
        s.on = enabled;
        s.cur = Row{};
        const unsigned frames = 10000;
        const double begin = D3D_ProfNow();
        for (unsigned frame = 0; frame < frames; ++frame) {
            s.cur = Row{};
            for (unsigned eye = 0; eye < 2; ++eye) {
                Trace_RenderBegin(eye);
                for (unsigned site = 1; site <= 20; ++site) {
                    Trace_RenderBoundary(0x6DE000 + site * 16);
                    Trace_RenderBoundary(0x806DE000 + site * 16);
                }
                Trace_RenderEnd();
            }
            assert(!s.cur.spanOverflow);
        }
        const double elapsed = D3D_ProfNow() - begin;
        printf("Synthetic partition bookkeeping %s: %.6f ms/frame (two eyes, 20 before/after sites; no wrapper/GPU)\n",
               enabled ? "enabled" : "disabled", elapsed * 1000 / frames);
    }
}