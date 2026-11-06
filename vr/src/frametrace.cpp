// Per-frame trace (see frametrace.h).
#include "common.h"
#include "frametrace.h"
#include "d3d9hook.h"
#include "game.h"
#include <stdio.h>
#include <string.h>

namespace {

constexpr double kTraceSeconds = 30.0;
constexpr int kMaxRows = 4096;                 // 30 s at up to ~136 fps
constexpr uintptr_t kSimTime = 0x9885D8;       // float: the game's time (Sim::GetTime), steps with its physics
enum { TS_EYE0_BEGIN, TS_EYE0_END, TS_EYE1_BEGIN, TS_EYE1_END, TS_FRAME_END, TS_COUNT };
enum { OBJ_PLAYER, OBJ_TRAFFIC_HINT, OBJ_COP_HINT, OBJ_OTHER_CAR, OBJ_BREAKABLE_HINT, OBJ_OTHER, OBJ_COUNT };
const char* objectLabels[] = {"player", "traffic_hint", "cop_hint", "other_car_unknown_role", "breakable_hint", "other_world"};
struct ModelSample { char name[65] = {}; unsigned category = 0, submits = 0, triangles = 0; };
constexpr unsigned kSamplesPerCategory = 8;
constexpr unsigned kModelSamples = OBJ_COUNT * kSamplesPerCategory;
struct RenderSpan { unsigned from, to, eye, calls; double ms; };
constexpr unsigned kRenderSpans = 128;
int renderEye = -1;
unsigned renderPrevious = 0;
double renderStart = 0;

struct Row {
    double t;                        // frame end, seconds since the trace started
    double frame, game;              // the frame's length, and the part outside the measured phases (ms)
    double phase[PROF_COUNT];        // ms
    double eyeCpu[2];                // each eye's scene render on the CPU (ms)
    unsigned draws[2];
    double gpuEye[2];                // each eye's scene on the GPU (ms, -1 unknown)
    double gpuFrame;                 // the first eye's start to the frame's last command, on the GPU (ms)
    float simStep;                   // game time this frame (s): the physics steps it covers
    int envFaces;                    // reflection cube faces drawn (bit per face)
    int rearView;                    // the rear view drawn
    int shadow;                      // 0 per eye, 1 shared (the second eye's pass dropped), 2 sharing paused
    int probe;                       // the HUD probe copied the HUD
    double detail[TRACE_DETAIL_COUNT]; // nested CPU measurements (ms), not additive phases
    unsigned objectSubmits[OBJ_COUNT] = {}, objectTriangles[OBJ_COUNT] = {};
    unsigned cacheRebuilds = 0, modelOverflow = 0;
    ModelSample models[kModelSamples];
    RenderSpan spans[kRenderSpans] = {};
    unsigned spanOverflow = 0;
};

struct {
    bool on = false;
    double start = 0;
    int n = 0, index = 0;
    Row* rows = nullptr;
    Row cur{};
    float lastSim = 0;
    IDirect3DDevice9* dev = nullptr;
    IDirect3DQuery9* ts[TS_COUNT] = {};
    IDirect3DQuery9* disjoint = nullptr;
    IDirect3DQuery9* freq = nullptr;
    bool issued[TS_COUNT] = {};
} s;

void ReleaseQueries() {
    for (auto*& q : s.ts)
        if (q) q->Release(), q = nullptr;
    if (s.disjoint) s.disjoint->Release(), s.disjoint = nullptr;
    if (s.freq) s.freq->Release(), s.freq = nullptr;
}

bool CreateQueries() {
    for (auto*& q : s.ts)
        if (FAILED(s.dev->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &q))) return false;
    return SUCCEEDED(s.dev->CreateQuery(D3DQUERYTYPE_TIMESTAMPDISJOINT, &s.disjoint)) &&
           SUCCEEDED(s.dev->CreateQuery(D3DQUERYTYPE_TIMESTAMPFREQ, &s.freq));
}

void Stamp(int i) {
    if (!s.ts[i]) return;
    s.ts[i]->Issue(D3DISSUE_END);
    s.issued[i] = true;
}

DWORD WINAPI WriteCsv(LPVOID p) {
    Row* rows = (Row*)p;
    int n = rows ? (int)rows[0].t : 0;   // the count and the file number travel in a header row
    int index = rows ? rows[0].envFaces : 0;
    rows++;
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%snfsmw_frametrace_%03d.csv", GameDir(), index);
    FILE* f = fopen(path, "w");
    if (f) {
        fprintf(f, "t_s,frame_ms,game_ms,headset_wait_ms,eyes_cpu_ms,gpu_wait_ms,submit_ms,spectator_ms,present_ms,update_ms,"
                   "eye0_cpu_ms,eye1_cpu_ms,eye0_draws,eye1_draws,eye0_gpu_ms,eye1_gpu_ms,gpu_frame_ms,game_step_s,"
                    "physics_steps,env_faces,env_face_count,rear_view,shadow,hud_probe,mirror_draw_cpu_ms,mirror_map_cpu_ms,mirror_restore_cpu_ms,hud_phase_cpu_ms,player_car_cpu_ms,other_car_cpu_ms,solid_cache_cpu_ms");
        for (const char* label : objectLabels) fprintf(f, ",%s_submits,%s_triangles", label, label);
        fprintf(f, ",solid_cache_rebuilds,model_overflow_submits\n");
        for (int i = 0; i < n; i++) {
            const Row& r = rows[i];
            int faces = 0;
            for (int k = 0; k < 6; k++) faces += r.envFaces >> k & 1;
            fprintf(f, "%.4f,%.3f,%.3f", r.t, r.frame, r.game);
            for (int k = 0; k < PROF_COUNT; k++) fprintf(f, ",%.3f", r.phase[k]);
            fprintf(f, ",%.3f,%.3f,%u,%u,%.3f,%.3f,%.3f,%.5f,%.0f,%d,%d,%d,%d,%d", r.eyeCpu[0], r.eyeCpu[1], r.draws[0],
                    r.draws[1], r.gpuEye[0], r.gpuEye[1], r.gpuFrame, r.simStep, r.simStep > 0 ? r.simStep / 0.00971f : 0.0f,
                    r.envFaces, faces, r.rearView, r.shadow, r.probe);
            for (double value : r.detail) fprintf(f, ",%.3f", value);
            for (unsigned k = 0; k < OBJ_COUNT; ++k) fprintf(f, ",%u,%u", r.objectSubmits[k], r.objectTriangles[k]);
            fprintf(f, ",%u,%u", r.cacheRebuilds, r.modelOverflow);
            fprintf(f, "\n");
        }
        fclose(f);
        Log("trace: %d frames written to %s", n, path);
    } else {
        Log("trace: cannot write %s", path);
    }
    snprintf(path, sizeof(path), "%snfsmw_frametrace_%03d_models.csv", GameDir(), index);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "frame_index,t_s,frame_ms,category,model,submits,triangles\n");
        for (int i = 0; i < n; ++i)
            for (const auto& model : rows[i].models)
                if (model.submits) fprintf(f, "%d,%.4f,%.3f,%s,%s,%u,%u\n", i, rows[i].t, rows[i].frame,
                    objectLabels[model.category], model.name, model.submits, model.triangles);
        fclose(f);
    }
    snprintf(path, sizeof(path), "%snfsmw_frametrace_%03d_passes.csv", GameDir(), index);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "frame_index,eye,from_site,to_site,cpu_ms,calls,overflow\n");
        for (int i = 0; i < n; ++i) {
            for (const auto& span : rows[i].spans)
                if (span.calls) fprintf(f, "%d,%u,%08X,%08X,%.6f,%u,%u\n", i, span.eye,
                    span.from, span.to, span.ms, span.calls, rows[i].spanOverflow);
        }
        fclose(f);
    } else Log("trace: cannot write pass timings %s", path);
    free(rows - 1);
    return 0;
}

void Finish() {
    s.on = false;
    ReleaseQueries();
    Row* out = (Row*)malloc(sizeof(Row) * (s.n + 1));
    if (!out) return;
    out[0] = Row{};
    out[0].t = s.n;
    out[0].envFaces = s.index;
    memcpy(out + 1, s.rows, sizeof(Row) * s.n);
    HANDLE t = CreateThread(nullptr, 0, WriteCsv, out, 0, nullptr);
    if (t) {
        SetThreadPriority(t, THREAD_PRIORITY_BELOW_NORMAL);
        CloseHandle(t);
    } else {
        free(out);
    }
}

}  // namespace

void Trace_KeyCheck(int vk, IDirect3DDevice9* dev) {
    static bool down;
    bool now = vk > 0 && vk < 256 && (GetAsyncKeyState(vk) & 0x8001) != 0;
    bool pressed = now && !down;
    down = now;
    if (!pressed || s.on || !dev) return;
    if (!s.rows) s.rows = (Row*)malloc(sizeof(Row) * kMaxRows);
    if (!s.rows) return;
    s.dev = dev;
    // Keep earlier sessions and the user's latest capture, even after restarting the game.
    char existing[MAX_PATH];
    do {
        ++s.index;
        snprintf(existing, sizeof(existing), "%snfsmw_frametrace_%03d.csv", GameDir(), s.index);
    } while (GetFileAttributesA(existing) != INVALID_FILE_ATTRIBUTES);
    if (!CreateQueries()) {
        ReleaseQueries();
        Log("trace: GPU timestamps unavailable, CPU times only");
    }
    s.on = true;
    s.n = 0;
    s.start = D3D_ProfNow();
    s.cur = Row{};
    s.lastSim = *(float*)kSimTime;
    if (s.disjoint) s.disjoint->Issue(D3DISSUE_BEGIN);
    Log("trace: recording %.0f s of frames", kTraceSeconds);
}

bool Trace_On() { return s.on; }
void Trace_RenderBegin(int eye) {
    renderEye = s.on ? eye : -1;
    renderPrevious = 0;
    if (renderEye >= 0) renderStart = D3D_ProfNow();
}
void Trace_RenderBoundary(unsigned site) {
    if (!s.on || renderEye < 0) return;
    double now = D3D_ProfNow();
    bool stored = false;
    for (auto& span : s.cur.spans) {
        if (!span.calls || (span.from == renderPrevious && span.to == site && span.eye == (unsigned)renderEye)) {
            span.from = renderPrevious; span.to = site; span.eye = renderEye;
            span.ms += (now - renderStart) * 1000.0;
            ++span.calls;
            stored = true;
            break;
        }
    }
    if (!stored) ++s.cur.spanOverflow;
    renderPrevious = site;
    // Include bookkeeping in the following interval: no hidden gaps in the partition.
    renderStart = now;
}
void Trace_RenderEnd() {
    Trace_RenderBoundary(0xFFFFFFFFu);
    renderEye = -1;
}
void Trace_CacheRebuild() { if (s.on) ++s.cur.cacheRebuilds; }
void Trace_Model(const char* name, bool car, bool player, unsigned triangles) {
    if (!s.on || !name) return;
    char clean[65] = {};
    for (unsigned i = 0; i < 64 && name[i]; ++i) {
        unsigned char c = (unsigned char)name[i];
        clean[i] = c >= 'a' && c <= 'z' ? c - 'a' + 'A' :
                   ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-') ? c : '_';
    }
    // These are model-name hints only. A racer model alone cannot prove AI role.
    unsigned category = player ? OBJ_PLAYER : !strncmp(clean, "TRAF", 4) ? OBJ_TRAFFIC_HINT :
        !strncmp(clean, "COP", 3) ? OBJ_COP_HINT : car ? OBJ_OTHER_CAR :
        (strstr(clean, "BREAK") || strstr(clean, "DEBRIS")) ? OBJ_BREAKABLE_HINT : OBJ_OTHER;
    ++s.cur.objectSubmits[category];
    s.cur.objectTriangles[category] += triangles;
    for (unsigned i = category * kSamplesPerCategory; i < (category + 1) * kSamplesPerCategory; ++i) {
        auto& model = s.cur.models[i];
        if (!model.submits || (model.category == category && !strcmp(model.name, clean))) {
            memcpy(model.name, clean, sizeof(clean));
            model.category = category;
            ++model.submits;
            model.triangles += triangles;
            return;
        }
    }
    ++s.cur.modelOverflow;
}
void Trace_Detail(TraceDetail detail, double seconds) {
    if (s.on && detail >= 0 && detail < TRACE_DETAIL_COUNT) s.cur.detail[detail] += seconds * 1000.0;
}
TraceDetailScope::TraceDetailScope(TraceDetail detail) : detail_(detail), start_(s.on ? D3D_ProfNow() : 0) {}
TraceDetailScope::~TraceDetailScope() {
    if (start_ > 0) Trace_Detail(detail_, D3D_ProfNow() - start_);
}

void Trace_EyeBegin(int eye) {
    if (!s.on || eye < 0 || eye > 1) return;
    Stamp(eye ? TS_EYE1_BEGIN : TS_EYE0_BEGIN);
}

void Trace_EyeEnd(int eye, double cpuSeconds, unsigned draws) {
    if (!s.on || eye < 0 || eye > 1) return;
    Stamp(eye ? TS_EYE1_END : TS_EYE0_END);
    s.cur.eyeCpu[eye] += cpuSeconds * 1000.0;
    s.cur.draws[eye] += draws;
}

void Trace_BeforeGpuWait() {
    if (!s.on) return;
    Stamp(TS_FRAME_END);
    if (s.disjoint) s.disjoint->Issue(D3DISSUE_END);
    if (s.freq) s.freq->Issue(D3DISSUE_END);
}

void Trace_AfterGpuWait() {
    if (!s.on) return;
    s.cur.gpuEye[0] = s.cur.gpuEye[1] = s.cur.gpuFrame = -1;
    UINT64 f = 0, v[TS_COUNT] = {};
    BOOL bad = TRUE;
    bool ok = s.freq && s.disjoint && s.freq->GetData(&f, sizeof(f), 0) == S_OK && f > 0 &&
              s.disjoint->GetData(&bad, sizeof(bad), 0) == S_OK && !bad;
    for (int i = 0; i < TS_COUNT && ok; i++)
        ok = s.issued[i] && s.ts[i]->GetData(&v[i], sizeof(v[i]), 0) == S_OK;
    if (ok) {
        double ms = 1000.0 / (double)f;
        s.cur.gpuEye[0] = (double)(v[TS_EYE0_END] - v[TS_EYE0_BEGIN]) * ms;
        s.cur.gpuEye[1] = (double)(v[TS_EYE1_END] - v[TS_EYE1_BEGIN]) * ms;
        s.cur.gpuFrame = (double)(v[TS_FRAME_END] - v[TS_EYE0_BEGIN]) * ms;
    }
    for (bool& i : s.issued) i = false;
    if (s.disjoint) s.disjoint->Issue(D3DISSUE_BEGIN);   // the next frame's window
}

void Trace_HudProbe() {
    if (s.on) s.cur.probe = 1;
}

void Trace_Frame(const double* phases, int count, double frame, double game) {
    if (!s.on) return;
    Row& r = s.cur;
    double now = D3D_ProfNow();
    r.t = now - s.start;
    r.frame = frame * 1000.0;
    r.game = game * 1000.0;
    for (int i = 0; i < PROF_COUNT && i < count; i++) r.phase[i] = phases[i] * 1000.0;
    float sim = *(float*)kSimTime;
    r.simStep = sim - s.lastSim;
    s.lastSim = sim;
    Game_TraceInfo(&r.envFaces, &r.rearView, &r.shadow);
    if (s.n < kMaxRows) s.rows[s.n++] = r;
    s.cur = Row{};
    if (now - s.start >= kTraceSeconds || s.n >= kMaxRows) Finish();
}
