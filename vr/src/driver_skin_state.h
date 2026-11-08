#pragma once
#include <d3d9.h>

// Native finger sheets need both faces after posing. Their atlas alpha is not
// coverage: retain depth testing/writes and shading, but do not fade the skin.
// Template permits regression testing without a live Direct3D device.
template<class Device> class DriverSkinState {
    Device* device_;
    DWORD saved_[3]{};
    bool active_ = false;
public:
    DriverSkinState(Device* device, bool skin) : device_(device) {
        if (!skin) return;
        if (FAILED(device_->GetRenderState(D3DRS_ALPHABLENDENABLE,&saved_[0])) ||
            FAILED(device_->GetRenderState(D3DRS_ALPHATESTENABLE,&saved_[1])) ||
            FAILED(device_->GetRenderState(D3DRS_CULLMODE,&saved_[2]))) return;
        active_ = true;
        device_->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
        device_->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);
        device_->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
    }
    ~DriverSkinState() {
        if (!active_) return;
        device_->SetRenderState(D3DRS_ALPHABLENDENABLE,saved_[0]);
        device_->SetRenderState(D3DRS_ALPHATESTENABLE,saved_[1]);
        device_->SetRenderState(D3DRS_CULLMODE,saved_[2]);
    }
    DriverSkinState(const DriverSkinState&) = delete;
    DriverSkinState& operator=(const DriverSkinState&) = delete;
};