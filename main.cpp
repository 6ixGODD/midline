#define UNICODE

#include <windows.h>

#ifndef _MSC_VER
typedef ULONG PROPID;
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib")

// ============== Global State ==============
class OverlayState {
public:
    static constexpr int kMinThickness = 1;
    static constexpr int kMaxThickness = 400;
    static constexpr int kMinAlpha = 10;
    static constexpr int kMaxAlpha = 255;
    static constexpr float kMinTaper = 0.0f;   // top width = 0% of bottom
    static constexpr float kMaxTaper = 2.0f;   // top width = 200% of bottom

    void AdjustThickness(int delta) {
        thickness_ = std::clamp(thickness_ + delta, kMinThickness, kMaxThickness);
        std::printf("Thickness:  %dpx\n", thickness_);
    }

    void AdjustAlpha(int delta) {
        alpha_ = std:: clamp(alpha_ + delta, kMinAlpha, kMaxAlpha);
        std::printf("Opacity: %d/255\n", alpha_);
    }

    void AdjustTaper(float delta) {
        taper_ = std::clamp(taper_ + delta, kMinTaper, kMaxTaper);
        std::printf("Taper: %.0f%% (top/bottom ratio)\n", taper_ * 100.0f);
    }

    [[nodiscard]] int thickness() const { return thickness_; }
    [[nodiscard]] int alpha() const { return alpha_; }
    [[nodiscard]] float taper() const { return taper_; }

private:
    int thickness_ = 2;
    int alpha_ = 140;
    float taper_ = 1.0f;  // 1.0 = no taper (straight line), <1.0 = top narrower, >1.0 = top wider
};

OverlayState g_state;
HWND g_hwnd = nullptr;
HHOOK g_mouse_hook = nullptr;

// ============== Layered Window Rendering ==============
void RedrawLayeredWindow(HWND hwnd) {
    RECT rect;
    GetWindowRect(hwnd, &rect);
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;

    HDC screen_dc = GetDC(nullptr);
    HDC mem_dc = CreateCompatibleDC(screen_dc);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(mem_dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    auto old_bitmap = static_cast<HBITMAP>(SelectObject(mem_dc, bitmap));

    memset(bits, 0, width * height * 4);

    Gdiplus::Graphics graphics(mem_dc);
    graphics.SetSmoothingMode(Gdiplus:: SmoothingModeAntiAlias);

    const int center_x = width / 2;
    const auto base_thickness = static_cast<float>(g_state.thickness());
    const float taper = g_state.taper();
    const auto alpha = static_cast<BYTE>(g_state.alpha());

    if (std::abs(taper - 1.0f) < 0.01f) {
        // No taper - draw simple line
        Gdiplus:: Pen pen(Gdiplus::Color(alpha, 255, 255, 255), base_thickness);
        graphics.DrawLine(&pen, center_x, 0, center_x, height);
    } else {
        // Draw tapered trapezoid
        const float bottom_half_width = base_thickness / 2.0f;
        const float top_half_width = bottom_half_width * taper;

        Gdiplus:: PointF points[4] = {
            {static_cast<float>(center_x) - top_half_width, 0.0f},                          // top-left
            {static_cast<float>(center_x) + top_half_width, 0.0f},                          // top-right
            {static_cast<float>(center_x) + bottom_half_width, static_cast<float>(height)}, // bottom-right
            {static_cast<float>(center_x) - bottom_half_width, static_cast<float>(height)}  // bottom-left
        };

        Gdiplus::SolidBrush brush(Gdiplus::Color(alpha, 255, 255, 255));
        graphics.FillPolygon(&brush, points, 4);
    }

    POINT dst_pos = {rect.left, rect.top};
    POINT src_pos = {0, 0};
    SIZE wnd_size = {width, height};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};

    UpdateLayeredWindow(hwnd, screen_dc, &dst_pos, &wnd_size, mem_dc, &src_pos, 0, &blend, ULW_ALPHA);

    SelectObject(mem_dc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(mem_dc);
    ReleaseDC(nullptr, screen_dc);
}

// ============== Low-Level Mouse Hook ==============
LRESULT CALLBACK LowLevelMouseProc(int code, WPARAM wparam, LPARAM lparam) {
    if (code >= 0 && wparam == WM_MOUSEWHEEL) {
        const bool ctrl_pressed = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool alt_pressed = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
        const bool shift_pressed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

        if (ctrl_pressed || alt_pressed || shift_pressed) {
            const auto* hook_data = reinterpret_cast<MSLLHOOKSTRUCT*>(lparam);
            const auto delta = static_cast<short>(HIWORD(hook_data->mouseData));

            if (shift_pressed) {
                // Shift + Scroll: adjust taper (perspective)
                g_state.AdjustTaper(delta > 0 ? 0.05f : -0.05f);
            } else if (ctrl_pressed) {
                // Ctrl + Scroll: adjust thickness
                g_state.AdjustThickness(delta > 0 ? 1 : -1);
            } else {
                // Alt + Scroll: adjust opacity
                g_state.AdjustAlpha(delta > 0 ? 10 : -10);
            }

            RedrawLayeredWindow(g_hwnd);
            return 1;
        }
    }
    return CallNextHookEx(g_mouse_hook, code, wparam, lparam);
}

// ============== Window Procedure ==============
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_CREATE:
            PostMessage(hwnd, WM_USER + 1, 0, 0);
            return 0;

        case WM_USER + 1:
            RedrawLayeredWindow(hwnd);
            return 0;

        case WM_DISPLAYCHANGE:  {
            const int w = GetSystemMetrics(SM_CXSCREEN);
            const int h = GetSystemMetrics(SM_CYSCREEN);
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, w, h, SWP_NOACTIVATE);
            RedrawLayeredWindow(hwnd);
            return 0;
        }

        case WM_DESTROY:
            if (g_mouse_hook) {
                UnhookWindowsHookEx(g_mouse_hook);
                g_mouse_hook = nullptr;
            }
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProc(hwnd, msg, wparam, lparam);
    }
}

// ============== Keep Window on Top ==============
void CALLBACK TopMostTimerProc(HWND, UINT, UINT_PTR, DWORD) {
    SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

// ============== Entry Point ==============
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
    AllocConsole();
    std::freopen("CONOUT$", "w", stdout);

    std::printf("================================================\n");
    std::printf("  Midline - Center Line Overlay with Perspective\n");
    std::printf("================================================\n");
    std::printf("  Ctrl  + Scroll :  Adjust thickness (1-%d px)\n", OverlayState::kMaxThickness);
    std::printf("  Alt   + Scroll :  Adjust opacity\n");
    std::printf("  Shift + Scroll : Adjust taper (perspective)\n");
    std::printf("                   < 100%% = top narrower (default view)\n");
    std::printf("                   > 100%% = top wider\n");
    std::printf("================================================\n");

    Gdiplus::GdiplusStartupInput gdiplus_input;
    ULONG_PTR gdiplus_token;
    Gdiplus::GdiplusStartup(&gdiplus_token, &gdiplus_input, nullptr);

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"CenterLineOverlay";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassEx(&wc);

    const int screen_width = GetSystemMetrics(SM_CXSCREEN);
    const int screen_height = GetSystemMetrics(SM_CYSCREEN);

    g_hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
        L"CenterLineOverlay",
        L"Midline",
        WS_POPUP,
        0, 0, screen_width, screen_height,
        nullptr, nullptr, instance, nullptr
    );

    if (!g_hwnd) {
        std::printf("Failed to create window!\n");
        return 1;
    }

    g_mouse_hook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, instance, 0);
    if (!g_mouse_hook) {
        std::printf("Failed to install mouse hook!\n");
    }

    ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
    SetTimer(g_hwnd, 1, 300, TopMostTimerProc);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Gdiplus::GdiplusShutdown(gdiplus_token);
    return static_cast<int>(msg.wParam);
}

int main() {
    return wWinMain(GetModuleHandle(nullptr), nullptr, nullptr, SW_SHOW);
}