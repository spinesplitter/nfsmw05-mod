#pragma once
#include <windows.h>

class SpectatorHudWindow {
    HWND window_ = nullptr;
    WNDPROC previous_ = nullptr;
    int width_ = 0;
    int height_ = 0;

    static const char* Key() {
        return "NFSMW.VR.HudWindow";
    }

    static LRESULT CALLBACK Proc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
        auto self = (SpectatorHudWindow*)GetPropA(window, Key());
        if (!self || !self->previous_) return DefWindowProcA(window, message, wp, lp);
        WNDPROC previous = self->previous_;
        if (message == WM_SIZE) lp = self->Size(wp, lp);
        if (message == WM_NCDESTROY) {
            RemovePropA(window, Key());
            self->window_ = nullptr;
            self->previous_ = nullptr;
        }
        return CallWindowProcA(previous, window, message, wp, lp);
    }

public:
    LPARAM Size(WPARAM wp, LPARAM actual) const {
        return wp != SIZE_MINIMIZED && width_ > 0 && height_ > 0
            ? MAKELPARAM(width_, height_)
            : actual;
    }

    bool Attach(HWND window, int width, int height) {
        if (!window || width <= 0 || height <= 0 || width > 65535 || height > 65535) return false;
        if (window_ && window_ != window) return false;
        width_ = width;
        height_ = height;
        if (window_) return true;
        if (!SetPropA(window, Key(), this)) return false;
        SetLastError(0);
        auto previous = (WNDPROC)SetWindowLongPtrA(window, GWLP_WNDPROC, (LONG_PTR)&Proc);
        if (!previous) {
            RemovePropA(window, Key());
            return false;
        }
        previous_ = previous;
        window_ = window;
        return true;
    }
};
