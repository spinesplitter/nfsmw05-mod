#include "../src/driver_skin_state.h"
#include <assert.h>
#include <stdio.h>
struct Device {
    DWORD blend=TRUE, test=TRUE, cull=D3DCULL_CW;
    int reads=0, writes=0, fail=0;
    DWORD& Value(D3DRENDERSTATETYPE state) {
        assert(state==D3DRS_ALPHABLENDENABLE || state==D3DRS_ALPHATESTENABLE || state==D3DRS_CULLMODE);
        return state==D3DRS_ALPHABLENDENABLE ? blend : state==D3DRS_ALPHATESTENABLE ? test : cull;
    }
    HRESULT GetRenderState(D3DRENDERSTATETYPE state, DWORD* value) {
        if (++reads==fail) return E_FAIL;
        *value=Value(state); return S_OK;
    }
    HRESULT SetRenderState(D3DRENDERSTATETYPE state, DWORD value) {
        ++writes; Value(state)=value; return S_OK;
    }
};
int main() {
    Device d;
    { DriverSkinState<Device> guard(&d,false); }
    assert(d.reads==0 && d.writes==0);
    { DriverSkinState<Device> guard(&d,true);
      assert(d.blend==FALSE && d.test==FALSE && d.cull==D3DCULL_NONE); }
    assert(d.blend==TRUE && d.test==TRUE && d.cull==D3DCULL_CW && d.writes==6);
    for(int fail=1;fail<=3;++fail) {
        Device broken; broken.fail=fail;
        { DriverSkinState<Device> guard(&broken,true); }
        assert(broken.writes==0 && broken.blend==TRUE && broken.test==TRUE && broken.cull==D3DCULL_CW);
    }
    puts("DRIVER SKIN STATE TESTS PASSED");
}