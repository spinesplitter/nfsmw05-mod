#include <assert.h>
#include <stdio.h>
#include <d3d9.h>
#include "../src/driver_stream_cache.h"
static DriverStreamCache s_driverStreams;
static unsigned mirrorInvalidations;
static void InvalidateMirrorMappedStreams(IDirect3DVertexBuffer9*) { ++mirrorInvalidations; }
#include "../src/driver_buffer_hooks.h"

struct FakeBuffer { void** vt; ULONG refs; unsigned char bytes[32]; unsigned locks; };
static HRESULT WINAPI Lock(IDirect3DVertexBuffer9* vb, UINT offset, UINT, void** data, DWORD) {
    auto* b=(FakeBuffer*)vb;
    ++b->locks;
    *data=b->bytes+offset;
    return S_OK;
}
static ULONG WINAPI Release(IDirect3DVertexBuffer9* vb) { return --((FakeBuffer*)vb)->refs; }
int main() {
    void* vt[14]={}; vt[2]=(void*)&Release; vt[11]=(void*)&Lock;
    FakeBuffer b{vt,2,{1},0};
    auto* vb=(IDirect3DVertexBuffer9*)&b;
    assert(ObserveDriverBuffer(vb) && ObserveDriverBuffer(vb));
    unsigned reads=0;
    auto read=[&](std::vector<unsigned char>& bytes) {
        ++reads; void* data=nullptr;
        assert(SUCCEEDED(vb->Lock(0,0,&data,D3DLOCK_READONLY)));
        bytes.assign((unsigned char*)data,(unsigned char*)data+32);
        return true;
    };
    auto get=[&]() { return s_driverStreams.Get(vb,0,4,32,32,read); };
    assert((*get())[0]==1 && reads==1);
    assert((*get())[0]==1 && reads==1);
    void* data=nullptr;
    assert(SUCCEEDED(vb->Lock(0,0,&data,0))); b.bytes[0]=2;
    assert(mirrorInvalidations==1);
    assert((*get())[0]==2 && reads==2);
    assert(vb->Release()==1);
    assert((*get())[0]==2 && reads==2);
    assert(vb->Release()==0);
    assert(mirrorInvalidations==2);
    b.refs=1; b.bytes[0]=3; // same-address resource reuse
    assert((*get())[0]==3 && reads==3);
    // An independent vtable must retain its own originals.
    void* other[14]={}; other[2]=(void*)&Release; other[11]=(void*)&Lock;
    FakeBuffer c{other,1,{4},0};
    assert(ObserveDriverBuffer((IDirect3DVertexBuffer9*)&c));
    assert(SUCCEEDED(((IDirect3DVertexBuffer9*)&c)->Lock(0,0,&data,D3DLOCK_READONLY)));
    assert(c.locks==1 && b.locks==4);
    assert((*get())[0]==3 && reads==3);
    // Mirror draws use a whole-buffer byte identity, not the driver's layout.
    s_driverStreams.Clear();
    s_driverStreams.ResetStats();
    auto mirror=[&]() { return s_driverStreams.Get(vb,0,1,32,32,read); };
    const auto* source=mirror();
    assert(source && (*source)[0]==3);
    std::vector<unsigned char> mapped=*source;
    mapped[0]=99;
    assert((*mirror())[0]==3); // a different mirror/eye cannot see mapped UV writes
    assert(s_driverStreams.GetStats().hits==1 && s_driverStreams.GetStats().reads==1);
    assert(s_driverStreams.GetStats().bytes==32);
    // Exact game call sequence: cleanup -1 occurs BETWEEN eye zero and eye one.
    s_driverStreams.BeginEye(-1);
    s_driverStreams.BeginEye(1);
    assert((*mirror())[0]==3 && s_driverStreams.GetStats().reads==1);
    s_driverStreams.BeginEye(-1);
    assert((*mirror())[0]==3 && s_driverStreams.GetStats().reads==1);
    assert(SUCCEEDED(vb->Lock(0,0,&data,0))); b.bytes[0]=5;
    assert((*mirror())[0]==5);
    assert(s_driverStreams.GetStats().invalidations==1);
    assert(s_driverStreams.GetStats().reads==2 && s_driverStreams.GetStats().bytes==64);
    s_driverStreams.BeginEye(0);
    assert((*mirror())[0]==5 && s_driverStreams.GetStats().reads==3);
    auto fail=[](std::vector<unsigned char>&) { return false; };
    assert(!s_driverStreams.Get(vb,1,1,31,31,fail));
    assert(s_driverStreams.GetStats().failures==1);
    // A later overlay replacing a hook disables reuse, rather than chaining recursively.
    vt[11]=(void*)&Lock;
    assert(!ObserveDriverBuffer(vb));
    puts("PASS: real hook dispatch, read-only reuse, write/release invalidation, multiple vtables and overlay fallback");
}