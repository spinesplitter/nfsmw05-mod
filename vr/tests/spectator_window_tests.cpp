#include "../src/spectator_window.h"
#include <assert.h>
#include <stdio.h>

static LPARAM lastSize;
static unsigned sizes, keys, destroyed;

static LRESULT CALLBACK Original(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    if (message == WM_SIZE) {
        lastSize = lp;
        ++sizes;
    }
    if (message == WM_KEYDOWN) ++keys;
    if (message == WM_NCDESTROY) ++destroyed;
    return DefWindowProcA(window, message, wp, lp);
}

int main() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = Original;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "SpectatorHudIsolationTest";
    assert(RegisterClassA(&wc));
    HWND window = CreateWindowA(wc.lpszClassName, "test", WS_OVERLAPPEDWINDOW,
                                0, 0, 800, 600, nullptr, nullptr, wc.hInstance, nullptr);
    assert(window);

    SpectatorHudWindow isolation;
    assert(!isolation.Attach(window, 0, 1050));
    assert(isolation.Attach(window, 1012, 1050));
    SetWindowLongA(window, GWL_STYLE, WS_POPUP);
    assert(SetWindowPos(window, nullptr, 0, 0, 1920, 1080,
                        SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED));
    RECT client = {};
    assert(GetClientRect(window, &client));
    assert(client.right == 1920 && client.bottom == 1080);

    SendMessageA(window, WM_SIZE, SIZE_RESTORED, MAKELPARAM(1920, 1080));
    assert(LOWORD(lastSize) == 1012 && HIWORD(lastSize) == 1050 && sizes);
    SendMessageA(window, WM_SIZE, SIZE_MINIMIZED, 0);
    assert(lastSize == 0);
    SendMessageA(window, WM_KEYDOWN, 'A', 0);
    assert(keys == 1);
    assert(isolation.Attach(window, 1000, 1040));
    SendMessageA(window, WM_SIZE, SIZE_RESTORED, MAKELPARAM(2560, 1440));
    assert(LOWORD(lastSize) == 1000 && HIWORD(lastSize) == 1040);
    assert(DestroyWindow(window));
    assert(destroyed == 1);
    puts("PASS: fullscreen client, virtual HUD resize, minimize, input, reconfigure and destruction");
}
