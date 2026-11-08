// Required-hook validation uses a synthetic v1.3 address layout and never launches the game.
#include "../src/game.cpp"
#include "../src/mirror_policy.h"
#include <assert.h>
Config g_cfg;
unsigned char g_traceRenderBoundaries = 0;
volatile uint8_t gameAddressPadding[0x600000];
void Log(const char*, ...) {}
bool D3D_EnsureXr() { return false; }
void D3D_BeginEyes() {}
void D3D_EyePassBegin(int) {}
void D3D_EyeRenderProfile(int, double) {}
void D3D_ResetForNextEye() {}
void D3D_CaptureEye(int) {}
void D3D_BeginHudPhase() {}
void D3D_EndFlatDash() {}
void D3D_DropDraws(bool) {}
void D3D_ProfUpdateDone() {}
bool Xr_Available() { return false; }
bool Xr_BeginFrame() { return false; }
void Xr_RecommendedSize(int* w, int* h) { *w = *h = 0; }
const EyeView& Xr_Eye(int) { static EyeView e; return e; }
bool Cockpit_ApplySeat(M4&) { return false; }
void Cockpit_EndFrame() {}
void Cockpit_BeginEye() {}
bool Cockpit_IsPlayerCarMatrix(const void*) { return false; }
int Cockpit_PlayerCarMatrices() { return 0; }
void Cockpit_SetGaze(const float*, const float*) {}
void Cockpit_EndEye() {}
void D3D_BeginMirrorEye(const float*, const float*) {}
void D3D_AddMirror(void*, const float*, int, const float*, int) {}
void D3D_RegisterMirrorGlass(void*, int, int, const float*, const uint16_t*, int) {}
void D3D_SetGlassCorners(void*, int, const float*) {}
void D3D_AddMirrorGlassPart(void*, int, const float*, const uint16_t*, int) {}
void D3D_EndMirrorEye(void*) {}
void D3D_SetMirrorsDisabled(bool) {}
void D3D_SetRearViewTarget(void*, float) {}
void D3D_BeginWheelEye(bool, const float*, const float*, const float*) {}
void D3D_EndWheelEye() {}
void D3D_EndDashEye() {}
void D3D_BeginDashEye(bool, const float*, const float*) {}
double D3D_ProfNow() { return 0; }
void D3D_ProfAdd(int, double) {}
void Trace_RenderBegin(int) {}
static unsigned boundarySite;
void Trace_RenderBoundary(unsigned site) {
    boundarySite = site;
    // Deliberately clobber caller-saved floating state inside the instrumentation.
    __asm { fldz }
    __asm { xorps xmm0, xmm0 }
}
void Trace_RenderEnd() {}
TraceDetailScope::TraceDetailScope(TraceDetail detail) : detail_(detail), start_(0) {}
TraceDetailScope::~TraceDetailScope() {}
void D3D_HideTextures(void*, void*, void*) {}
void* D3D_TextureOfSurface(void*) { return nullptr; }
static void __stdcall Resolution(int* w, int* h) { *w = 1920; *h = 1080; }
static void Call(uintptr_t site, uintptr_t target) {
    *(uint8_t*)site = 0xE8;
    *(int32_t*)(site + 1) = (int32_t)(target - site - 5);
}
int main() {
    static_assert(sizeof(void*) == 4, "test game layout");
    assert((uintptr_t)gameAddressPadding < kGetResolution &&
           (uintptr_t)gameAddressPadding + sizeof(gameAddressPadding) > kPlayerView);
    {
        DWORD oldProtect;
        assert(VirtualProtect((void*)0x6E2F50, 1, PAGE_EXECUTE_READWRITE, &oldProtect));
        *(unsigned char*)0x6E2F50 = 0xC3; // synthetic original target: ret
        FlushInstructionCache(GetCurrentProcess(), (void*)0x6E2F50, 1);
        double value = 0;
        unsigned xmmValue = 0;
        // Idle must bypass callbacks without touching the original FP state.
        __asm {
            fld1
            mov eax, 0x12345678
            movd xmm0, eax
            call hkRenderBoundary
            movd xmmValue, xmm0
            fstp value
        }
        assert(value == 1.0 && xmmValue == 0x12345678);
        assert(boundarySite == 0);
        g_traceRenderBoundaries = 1;
        __asm {
            fld1
            mov eax, 0x12345678
            movd xmm0, eax
            call hkRenderBoundary
            movd xmmValue, xmm0
            fstp value
        }
        assert(value == 1.0 && xmmValue == 0x12345678);
        assert(boundarySite != 0);
        puts("Render boundary x87/SSE preservation test passed");
        g_traceRenderBoundaries = 0;
        boundarySite = 0;
        LARGE_INTEGER frequency, begin, end;
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&begin);
        const unsigned frames = 10000;
        for (unsigned frame = 0; frame < frames; ++frame)
            for (unsigned boundary = 0; boundary < 40; ++boundary) hkRenderBoundary();
        QueryPerformanceCounter(&end);
        printf("Synthetic wrapper overhead: %.6f ms/frame (40 invocations, mock callback/target; no GPU)\n",
               double(end.QuadPart - begin.QuadPart) * 1000 / frequency.QuadPart / frames);
        assert(boundarySite == 0);
    }
    assert(!IsSharedViewForBothEyes(0));
    assert(!IsSharedViewForBothEyes(1));
    assert(IsSharedViewForBothEyes(3));
    assert(!IsSharedViewForBothEyes(15));
    assert(IsSharedViewForBothEyes(18));
    assert(IsSharedViewForBothEyes(23));
    // Environment map faces in turn: every face refreshed once per 6 / N frames, N a frame; only
    // eye-independent views are ever skipped; the rear view every RearViewEvery'th frame.
    for (int n = 1; n <= 6; n++) {
        g_cfg.envMapFacesPerFrame = n;
        g_cfg.rearViewEvery = 2;
        int refreshed[kEnvFaces] = {}, rearDrawn = 0;
        for (int f = 0; f < 12; f++) {
            PickViewsThisFrame();
            int faces = 0;
            for (int i = 0; i < kNumViews; i++)
                if (s_viewSkipped[i]) assert(IsSharedViewForBothEyes(i));
            for (int i = 0; i < kEnvFaces; i++)
                if (!s_viewSkipped[kFirstEnvFace + i]) faces++, refreshed[i]++;
            assert(faces == n);
            rearDrawn += !s_viewSkipped[kRearView];
        }
        for (int i = 0; i < kEnvFaces; i++) assert(refreshed[i] == 12 * n / kEnvFaces);
        assert(rearDrawn == 6);
    }
    g_cfg = Config();
    // A live disable overrides cadence without changing preferences; re-enable refreshes normally.
    g_cfg.rearViewEvery = 1;
    g_cfg.disableMirrors = 1;
    assert(!MirrorPatchWanted(true, true, true, true));
    assert(SuppressMirrorGlassDraw(true, true, 12, 12));
    assert(!SuppressMirrorGlassDraw(true, false, 12, 12));
    assert(!SuppressMirrorGlassDraw(true, true, 12, 13));
    for (int i = 0; i < 8; ++i) {
        PickViewsThisFrame();
        assert(s_viewSkipped[kRearView]);
    }
    BeginMirrors(nullptr); // must return before dereferencing a view
    EndMirrors();
    assert(g_cfg.mirrors && g_cfg.gameMirrors && g_cfg.rearViewEvery == 1);
    g_cfg.disableMirrors = 0;
    assert(MirrorPatchWanted(true, true, false, true));
    PickViewsThisFrame();
    assert(!s_viewSkipped[kRearView]);
    s_mirrorPatched = false;
    g_cfg = Config();
    puts("Mirror disable tests passed: live override, disabled eye work, preserved preferences, re-enable");
    // Depth split: a D3D perspective projection (near 0.3, far 3000) times a view; the world ends up
    // in [kCarDepth, 1] with its planes kept, the car's variant in [0, kCarDepth] with NearZ.
    {
        g_cfg.nearZ = 0.09f;
        float n = 0.3f, f = 3000.0f;
        M4 P = {};
        P.m[0][0] = 1.2f; P.m[1][1] = 1.1f;
        P.m[2][2] = f / (f - n); P.m[2][3] = 1.0f; P.m[3][2] = -n * f / (f - n);
        M4 V = {};   // a rotation about y and a translation
        V.m[0][0] = 0.8f; V.m[0][2] = -0.6f; V.m[1][1] = 1.0f; V.m[2][0] = 0.6f; V.m[2][2] = 0.8f;
        V.m[3][0] = 5.0f; V.m[3][1] = -2.0f; V.m[3][2] = 7.0f; V.m[3][3] = 1.0f;
        M4 VP = MMul(V, P);
        assert(ZColumnIs(P, P.m[2][2], P.m[3][2]) && ZColumnIs(VP, P.m[2][2], P.m[3][2]));
        M4 car = CarDepthVariant(VP), world = VP;
        WorldDepthRemap(world);
        auto depthAt = [&](const M4& m, float d) {   // z/w of a world point at view depth d
            // world point whose view-space position is (0.1, 0.2, d): solve with the inverse view
            M4 W = MInvRigid(V);
            float x = 0.1f, y = 0.2f, p[4];
            for (int j = 0; j < 4; j++) p[j] = x * W.m[0][j] + y * W.m[1][j] + d * W.m[2][j] + W.m[3][j];
            float z = 0, w = 0;
            for (int i = 0; i < 4; i++) z += p[i] * m.m[i][2], w += p[i] * m.m[i][3];
            return z / w;
        };
        assert(fabsf(depthAt(world, n) - kCarDepth) < 1e-4f);
        assert(fabsf(depthAt(world, f) - 1.0f) < 1e-4f);
        assert(fabsf(depthAt(car, g_cfg.nearZ)) < 1e-4f);
        assert(fabsf(depthAt(car, kCarFarZ) - kCarDepth) < 1e-4f);
        assert(depthAt(car, 2.0f) < depthAt(world, n));
        g_cfg = Config();
    }
    // Stereo glow/specular passes: identical world depth, player-car variants,
    // aliased platform storage, all-or-nothing validation and eye-to-eye reset.
    {
        assert(Config().worldNearZ == 0.5f);
        uint8_t player[kViewStride] = {}, glow[kViewStride] = {};
        uint8_t pp[0x140] = {}, gp[0x140] = {};
        At<uint8_t*>(player, kViewPlatInfo) = pp;
        At<uint8_t*>(glow, kViewPlatInfo) = gp;
        M4 P = {};
        P.m[0][0] = P.m[1][1] = 1;
        P.m[2][2] = 3000.0f / 2999.5f;
        P.m[2][3] = 1;
        P.m[3][2] = -0.5f * P.m[2][2];
        auto reset = [&]() {
            memset(pp, 0, sizeof(pp)); memset(gp, 0, sizeof(gp));
            for (auto plat : {pp, gp}) {
                memcpy(plat + kPlatProjection, &P, sizeof(P));
                memcpy(plat + kPlatViewProj, &P, sizeof(P));
                memcpy(plat + kPlatProjAlt, &P, sizeof(P));
                memcpy(plat + kPlatViewProjAlt, &P, sizeof(P));
            }
        };
        for (int eye = 0; eye < 2; eye++) {
            reset();
            assert(SplitPlayerDepth(player));
            assert(SplitPlayerDepth(glow, false));
            assert(s_carViewProjCount == 4);
            assert(memcmp(pp, gp, sizeof(pp)) == 0);
            assert(memcmp(&s_carViewProj[0], &s_carViewProj[2], sizeof(M4)) == 0);
            uint8_t before[sizeof(gp)]; memcpy(before, gp, sizeof(gp));
            assert(SplitPlayerDepth(glow, false));
            assert(s_carViewProjCount == 4 && memcmp(before, gp, sizeof(gp)) == 0);
        }
        reset();
        *(float*)(gp + kPlatProjection) = NAN;
        assert(!SplitPlayerDepth(glow, false, true));
        assert(memcmp(pp + kPlatProjection, &P, sizeof(P)) == 0);
        reset();
        // Exercise group prevalidation with one active malformed companion.
        void* companion = (void*)(kViews + 4 * kViewStride);
        At<uint8_t*>(companion, kViewPlatInfo) = gp;
        At<void*>(companion, kViewCamera) = player;
        At<int8_t>(companion, kViewActive) = 1;
        s_viewArgs[4].have = true;
        *(float*)(gp + kPlatProjection + 3 * sizeof(float)) = 2;
        assert(!SplitCameraDepth(player, player));
        assert(memcmp(pp + kPlatProjection, &P, sizeof(P)) == 0);
        reset();
        assert(SplitCameraDepth(player, player));
        assert(s_carViewProjCount == 4 && memcmp(pp, gp, sizeof(pp)) == 0);
        memset(companion, 0, kViewStride); s_viewArgs[4].have = false;
        s_carViewProjCount = 0; s_carProjSet = false;
        puts("Stereo companion depth tests passed (both eyes, aliases, invalid projections)");
    }
    // Shared shadow map guard: identical states pass, a 3 cm eye offset in a shadow matrix does not,
    // a NaN on one side only does not.
    {
        static ShadowState a, b;
        memset(&a, 0, sizeof(a));
        for (int i = 0; i < 16; i++) a.tex[i] = 0.1f * i;
        a.plat[12] = 1234.5f;
        b = a;
        assert(ShadowStateDiff(a, b) == 0.0f);
        b.plat[12] += 0.03f;
        int at = -2;
        assert(ShadowStateDiff(a, b, &at) > 1e-5f && at == 12);
        char what[64];
        DescribeShadowField(at, what, sizeof(what));
        assert(strstr(what, "view [3][0]"));
        b = a;
        b.tex[3] += 0.001f;
        assert(ShadowStateDiff(a, b) > 1e-4f);
        b = a;
        float nan = 0.0f;
        nan = nan / nan;
        b.light[1] = nan;
        assert(ShadowStateDiff(a, b) > 1.0f);
    }
    // A saturated first-eye flare pool must not leak into the second eye. Exercise
    // the actual second-eye hook, including exclusions and an empty baseline.
    {
        s_secondEye = true;
        s_flareKeepCount = 2;
        memset(s_flareKeep, 0x37, 2 * kFlareSize);
        s_flareKeepXclude[0] = 4;
        s_flareKeepXclude[1] = 8;
        memset((void*)kFlarePool, 0xEE, kFlareMax * kFlareSize);
        *(int*)kFlarePoolCount = kFlareMax;
        hkPreDisplay();
        assert(*(int*)kFlarePoolCount == 2);
        assert(memcmp((void*)kFlarePool, s_flareKeep, 2 * kFlareSize) == 0);
        assert(memcmp((void*)kFlarePoolXclude, s_flareKeepXclude, 2 * sizeof(uint32_t)) == 0);
        s_flareKeepCount = 0;
        hkPreDisplay();
        assert(*(int*)kFlarePoolCount == 0);
        s_secondEye = false;
        puts("Stereo flare pool restoration tests passed");
    }
    // Quantify distant-world precision independently of the cockpit near plane.
    // In D24, a 1 cm separation at 250 m is below one depth unit with n=0.3,
    // but above one with n=0.5 (the stock world near plane), even after the split.
    {
        auto depthUnits = [](double n) {
            const double f = 3000.0, z = 250.0, dz = 0.01;
            const double B = -n * f / (f - n);
            return (1.0 - kCarDepth) * B * (1.0 / (z + dz) - 1.0 / z) * 16777215.0;
        };
        assert(depthUnits(0.3) < 1.0);
        assert(depthUnits(0.5) > 1.0);
        printf("D24 units for 1 cm at 250 m: world near 0.3 = %.3f, stock 0.5 = %.3f\n",
               depthUnits(0.3), depthUnits(0.5));
    }
    uint8_t flares[3 * kFlareSize] = {};
    uint32_t exclusions[3] = {4, 5, 6};
    flares[kFlareTypeOffset] = 0x15;
    flares[kFlareSize + kFlareTypeOffset] = 0x03;
    flares[2 * kFlareSize + kFlareTypeOffset] = 0x15;
    assert(RemoveSunLensFlares(flares, exclusions, 3) == 1);
    assert(flares[kFlareTypeOffset] == 0x03 && exclusions[0] == 5);
    Call(kCallViewSetup, kViewSetup); Call(kCallRenderScene, kRenderScene);
    const uint8_t original[] = {0xA1, 0x1C, 0x18, 0x90, 0x00, 0x83, 0xF8, 0x05};
    memcpy((void*)kGetResolution, original, sizeof(original));
    assert(Game_ValidateHooks());
    *(uint8_t*)kCallRenderScene = 0x90;
    uint8_t saved[5]; memcpy(saved, (void*)kCallViewSetup, sizeof(saved));
    assert(!Game_InstallHooks());
    assert(memcmp(saved, (void*)kCallViewSetup, sizeof(saved)) == 0);
    assert(memcmp(original, (void*)kGetResolution, sizeof(original)) == 0);
    Call(kCallRenderScene, kRenderScene);
    *(uint8_t*)kGetResolution = 0xE9;
    *(int32_t*)(kGetResolution + 1) = (int32_t)((uintptr_t)&Resolution - kGetResolution - 5);
    assert(Game_ValidateHooks());
    s_prevGetResolution = Resolution;
    strcpy(g_cfg.resolution, "game");
    int w, h; hkGetResolution(&w, &h);
    assert(w == 1920 && h == 1080);
    *(int32_t*)(kGetResolution + 1) = (int32_t)(1 - kGetResolution - 5);
    assert(!Game_ValidateHooks());
    memcpy((void*)kGetResolution, original, sizeof(original));
    *(uint8_t*)kGetResolution = 0x90;
    assert(!Game_ValidateHooks());
    puts("VR hook validation tests passed");
}
