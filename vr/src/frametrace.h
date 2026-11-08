// Per-frame trace ([VR] KeyTrace, default Delete): 30 s of frames, one CSV row each, to see
// what makes the frame time move from frame to frame (the 5 s "perf:" lines only have averages and
// maxima). Measurement only: nothing is drawn differently, and the GPU timestamp queries exist and
// are issued only while a trace runs. Written to nfsmw_frametrace_NNN.csv next to speed.exe by a
// thread of its own when the 30 s are up.
#pragma once
#include <d3d9.h>

// Render thread. Each call is a no-op unless a trace is running.
void Trace_KeyCheck(int vk, IDirect3DDevice9* dev);   // once a frame: starts a trace on the key
bool Trace_On();
// Render-thread-only fast gate for the assembly boundary wrapper.
extern unsigned char g_traceRenderBoundaries;
// CPU wall-time partition of each scene; boundaries are verified caller addresses.
void Trace_RenderBoundary(unsigned site);
void Trace_RenderBegin(int eye);
void Trace_RenderEnd();
// Nested CPU diagnostics; these overlap existing phases and must not be added to them.
enum TraceDetail { TRACE_MIRROR_DRAW, TRACE_MIRROR_MAP, TRACE_MIRROR_RESTORE, TRACE_HUD_PHASE,
    TRACE_PLAYER_CAR, TRACE_OTHER_CAR, TRACE_SOLID_CACHE,
    TRACE_RENDER_SETUP, TRACE_PREDISPLAY,
    TRACE_DRIVER_DRAW, TRACE_DRIVER_POSE, TRACE_DRIVER_READBACK, TRACE_DRIVER_UPLOAD,
    TRACE_COCKPIT_SUBMIT, TRACE_COCKPIT_FRAME, TRACE_DASH_DRAW, TRACE_DASH_COMPOSE,
    TRACE_HUD_PROBE, TRACE_CUSTOM_DRIVER, TRACE_DETAIL_COUNT };
// Counts are render submissions across all views/eyes, not simulation population or impacts.
void Trace_Model(const char* name, bool car, bool player, unsigned triangles);
void Trace_CacheRebuild();
void Trace_Detail(TraceDetail detail, double seconds);
class TraceDetailScope {
    TraceDetail detail_;
    double start_;
public:
    explicit TraceDetailScope(TraceDetail detail);
    ~TraceDetailScope();
};
void Trace_EyeBegin(int eye);                       // around the eye's scene render (GPU timestamps)
void Trace_EyeEnd(int eye, double cpuSeconds, unsigned draws);
void Trace_BeforeGpuWait();                          // the frame's last GPU command
void Trace_AfterGpuWait();                           // the GPU is idle: this frame's timestamps are ready
void Trace_HudProbe();                               // the HUD probe copied the HUD this frame
// The end of the frame (after Present): phase times in seconds (ProfSlot order, PROF_COUNT of them),
// the frame's length and the part outside the phases.
void Trace_Frame(const double* phases, int count, double frame, double game);
