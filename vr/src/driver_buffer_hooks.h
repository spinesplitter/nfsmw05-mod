#pragma once
#include <d3d9.h>
#include <unordered_map>

// Included after the render-thread-owned s_driverStreams cache. Observe the real
// buffer without changing COM identity. Preserve originals per driver/overlay vtable.
struct DriverBufferHooks {
    typedef HRESULT (WINAPI* Lock)(IDirect3DVertexBuffer9*,UINT,UINT,void**,DWORD);
    typedef ULONG (WINAPI* Release)(IDirect3DVertexBuffer9*);
    Lock lock;
    Release release;
};
static std::unordered_map<void**,DriverBufferHooks> s_driverBufferHooks;
static HRESULT WINAPI DriverBufferLock(IDirect3DVertexBuffer9* vb, UINT offset, UINT bytes, void** data, DWORD flags) {
    auto original=s_driverBufferHooks.at(*(void***)vb).lock;
    if (!(flags&D3DLOCK_READONLY)) s_driverStreams.Invalidate(vb);
    return original(vb,offset,bytes,data,flags);
}
static ULONG WINAPI DriverBufferRelease(IDirect3DVertexBuffer9* vb) {
    auto original=s_driverBufferHooks.at(*(void***)vb).release;
    ULONG refs=original(vb);
    if (!refs) s_driverStreams.Invalidate(vb);
    return refs;
}
static bool ObserveDriverBuffer(IDirect3DVertexBuffer9* vb) {
    void** vt=*(void***)vb;
    auto found=s_driverBufferHooks.find(vt);
    if (found!=s_driverBufferHooks.end())
        return vt[11]==(void*)&DriverBufferLock && vt[2]==(void*)&DriverBufferRelease;
    DWORD old;
    if (!VirtualProtect(vt,14*sizeof(void*),PAGE_EXECUTE_READWRITE,&old)) return false;
    s_driverBufferHooks.emplace(vt,DriverBufferHooks{(DriverBufferHooks::Lock)vt[11],
                                                   (DriverBufferHooks::Release)vt[2]});
    vt[11]=(void*)&DriverBufferLock;
    vt[2]=(void*)&DriverBufferRelease;
    DWORD ignored;
    VirtualProtect(vt,14*sizeof(void*),old,&ignored);
    return true;
}