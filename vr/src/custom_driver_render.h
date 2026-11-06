// Included by d3d9proxy.cpp after the dashboard state helpers.
#include "custom_driver.h"
static CustomDriver::Asset s_customDriver;
static IDirect3DTexture9* s_customDriverTexture;
static bool s_customDriverLoaded, s_customDriverAttempted, s_customDriverDrawn;
static void* s_customDriverIb;
static D3DMATRIX s_customDriverWorld;
static CustomDriver::AnimationRig s_customDriverRig;
static CustomDriver::Controls s_customDriverControls;
static std::vector<CustomDriver::Vertex> s_customDriverPosed;

static void ReleaseCustomDriver() {
    if (s_customDriverTexture) s_customDriverTexture->Release();
    s_customDriverTexture = nullptr;
    s_customDriverIb = nullptr;
}
void D3D_AddCustomDriver(void* ib, const float* world, const CustomDriver::Controls& controls) {
    if (!s_colorEye || s_customDriverIb || !ib || !world || g_cfg.showCustomDriverModel < 0.5f) return;
    s_customDriverIb = ib;
    memcpy(&s_customDriverWorld, world, sizeof(s_customDriverWorld));
    s_customDriverControls = controls;
}
static void DrawCustomDriver() {
    if (!s_colorEye || s_customDriverDrawn || !s_customDriverIb || s_customDriverIb != s_curIndices ||
        g_cfg.showCustomDriverModel < 0.5f || !DrawingPlayerView()) return;
    s_customDriverDrawn = true;
    TraceDetailScope driverTiming(TRACE_CUSTOM_DRIVER);
    if (!s_customDriverAttempted) {
        s_customDriverAttempted = true;
        char path[MAX_PATH]; snprintf(path, sizeof(path), "%sdrivermod\\driver-runtime.bin", GameDir());
        s_customDriverLoaded = s_customDriver.Load(path);
        snprintf(path, sizeof(path), "%sdrivermod\\driver-animation.bin", GameDir());
        bool animated = s_customDriverLoaded && s_customDriverRig.Load(path, s_customDriver.vertices.size());
        Log("custom driver: separate overlay rig %s (no NIS)", animated ? "loaded" : "unavailable; static fallback");
        Log("custom driver: %s, %u triangle vertices", s_customDriverLoaded ? "loaded" : "load failed",
            (unsigned)s_customDriver.vertices.size());
    }
    if (!s_customDriverLoaded) return;
    if (!s_customDriverTexture) {
        if (FAILED(s_dev->CreateTexture(s_customDriver.width, s_customDriver.height, 1, D3DUSAGE_DYNAMIC,
                D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &s_customDriverTexture, nullptr))) return;
        D3DLOCKED_RECT lock;
        if (FAILED(s_customDriverTexture->LockRect(0, &lock, nullptr, D3DLOCK_DISCARD))) {
            ReleaseCustomDriver(); return;
        }
        for (uint32_t y = 0; y < s_customDriver.height; y++)
            memcpy((char*)lock.pBits + y*lock.Pitch, s_customDriver.pixels.data() + y*s_customDriver.width,
                s_customDriver.width*sizeof(uint32_t));
        s_customDriverTexture->UnlockRect(0);
    }
    auto* saved = SavedStateBlock();
    if (!saved) return;
    saved->Capture();
    SetPanelState(s_dashEye.view, s_dashEye.proj);
    s_dev->SetTransform(D3DTS_WORLD, &s_customDriverWorld);
    s_dev->SetTexture(0, s_customDriverTexture);
    // The finger/detail UV island occupies a second tile.
    s_dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    s_dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
    s_dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    s_dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    s_dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    s_dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    s_customDriverRig.Pose(s_customDriver.vertices, s_customDriverPosed, s_customDriverControls);
    s_dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, (UINT)s_customDriverPosed.size()/3,
        s_customDriverPosed.data(), sizeof(CustomDriver::Vertex));
    saved->Apply();
}