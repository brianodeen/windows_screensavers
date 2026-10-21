#include "Engine/Renderer.h"
#include "Engine/Config.h"
#include <windows.h>
#include <windowsx.h>
#include <string>
#include <vector>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <cmath>

using namespace ScreensaverEngine;

enum class RunMode {
    StudioWindowed,     // Interactive development / gallery window (default for .exe)
    ScreensaverFull,    // /s : Fullscreen Windows screensaver with independent window per monitor
    PreviewChild,       // /p <HWND> : Embedded inside Windows Screen Saver Settings dialog
    ConfigDialog        // /c : Multi-Monitor Configuration dialog
};

struct MonitorWindow {
    int monitorIndex = 0;
    HWND hwnd = nullptr;
    std::unique_ptr<Renderer> renderer;
    bool hasInitialMouse = false;
    POINT initialMousePos = { 0, 0 };
};

struct AppState {
    RunMode mode = RunMode::StudioWindowed;
    ScreensaverConfig config;
    HINSTANCE hInstance = nullptr;
    const char* windowClassName = "ScreensaverStudioWindowClass";

    std::vector<MonitorDesc> connectedMonitors;
    std::vector<MonitorWindow> windows;

    bool paused = false;
    bool isStudioPrimaryFullscreen = false;
    bool isMultiMonitorStudioActive = false;
    WINDOWPLACEMENT windowedPlacement = { sizeof(WINDOWPLACEMENT) };
    float gracePeriodTimer = 0.0f;
};

static AppState* g_app = nullptr;

static MonitorWindow* FindMonitorWindow(HWND hwnd) {
    if (!g_app) return nullptr;
    for (auto& mw : g_app->windows) {
        if (mw.hwnd == hwnd) return &mw;
    }
    return nullptr;
}

static void UpdateStudioWindowTitle() {
    if (!g_app || g_app->mode != RunMode::StudioWindowed || g_app->windows.empty()) return;
    HWND primaryHwnd = g_app->windows[0].hwnd;
    if (!primaryHwnd || !g_app->windows[0].renderer) return;

    auto& r0 = *g_app->windows[0].renderer;
    char mon2Info[96] = "";
    if (g_app->windows.size() > 1 && g_app->windows[1].renderer) {
        std::snprintf(mon2Info, sizeof(mon2Info), " | Mon 2: %s", g_app->windows[1].renderer->GetActiveEffectName());
    }

    char title[320];
    std::snprintf(
        title,
        sizeof(title),
        "Screensaver Studio | Mon 1 [%d/%d]: %s%s | %.1fx %s | [Left/Right: Mon1] [ [ / ]: Mon2] [M: Dual-Mon] [C: Config] [F11: Full]",
        r0.GetActiveEffectIndex() + 1,
        r0.GetEffectCount(),
        r0.GetActiveEffectName(),
        mon2Info,
        r0.GetSpeedMultiplier(),
        g_app->paused ? "(PAUSED)" : "");
    SetWindowTextA(primaryHwnd, title);
}

static void ToggleStudioPrimaryFullscreen(HWND hwnd) {
    if (!g_app) return;
    DWORD style = GetWindowLongA(hwnd, GWL_STYLE);
    if (!g_app->isStudioPrimaryFullscreen) {
        MONITORINFO mi = { sizeof(mi) };
        if (GetWindowPlacement(hwnd, &g_app->windowedPlacement) &&
            GetMonitorInfoA(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLongA(hwnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(
                hwnd,
                HWND_TOP,
                mi.rcMonitor.left,
                mi.rcMonitor.top,
                mi.rcMonitor.right - mi.rcMonitor.left,
                mi.rcMonitor.bottom - mi.rcMonitor.top,
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            g_app->isStudioPrimaryFullscreen = true;
        }
    } else {
        SetWindowLongA(hwnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(hwnd, &g_app->windowedPlacement);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        g_app->isStudioPrimaryFullscreen = false;
    }
}

// Spawns or closes secondary monitor windows while in Interactive Studio Mode ('M' key)
static void ToggleMultiMonitorStudioPreview() {
    if (!g_app || g_app->mode != RunMode::StudioWindowed) return;

    if (g_app->isMultiMonitorStudioActive) {
        // Close secondary monitor windows (index >= 1)
        for (size_t i = 1; i < g_app->windows.size(); ++i) {
            if (g_app->windows[i].renderer) {
                g_app->windows[i].renderer->Shutdown();
            }
            if (g_app->windows[i].hwnd && IsWindow(g_app->windows[i].hwnd)) {
                DestroyWindow(g_app->windows[i].hwnd);
            }
        }
        g_app->windows.resize(1);
        g_app->isMultiMonitorStudioActive = false;
        UpdateStudioWindowTitle();
        return;
    }

    g_app->connectedMonitors = EnumerateConnectedMonitors();
    if (g_app->connectedMonitors.size() <= 1) {
        MessageBoxA(g_app->windows[0].hwnd,
            "Only 1 active display monitor was detected by Windows.\n"
            "You can still configure Monitor 2's screensaver in Settings ('C') for when it is connected!",
            "Multi-Monitor Preview", MB_ICONINFORMATION);
        return;
    }

    for (size_t i = 1; i < g_app->connectedMonitors.size() && i < static_cast<size_t>(kMaxMonitors); ++i) {
        const auto& mon = g_app->connectedMonitors[i];
        int x = mon.bounds.left;
        int y = mon.bounds.top;
        int w = mon.bounds.right - mon.bounds.left;
        int h = mon.bounds.bottom - mon.bounds.top;

        HWND secHwnd = CreateWindowExA(
            WS_EX_TOPMOST,
            g_app->windowClassName,
            "Screensaver Studio - Secondary Monitor",
            WS_POPUP | WS_VISIBLE,
            x, y, w, h,
            nullptr,
            nullptr,
            g_app->hInstance,
            nullptr);

        if (secHwnd) {
            MonitorWindow mw;
            mw.monitorIndex = static_cast<int>(i);
            mw.hwnd = secHwnd;
            mw.renderer = std::make_unique<Renderer>();
            if (mw.renderer->Initialize(secHwnd, w, h, static_cast<int>(i), g_app->config)) {
                g_app->windows.push_back(std::move(mw));
            } else {
                DestroyWindow(secHwnd);
            }
        }
    }

    // Keep keyboard focus on the primary Studio window
    SetForegroundWindow(g_app->windows[0].hwnd);
    g_app->isMultiMonitorStudioActive = (g_app->windows.size() > 1);
    UpdateStudioWindowTitle();
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE: {
        if (g_app && wParam != SIZE_MINIMIZED) {
            if (auto* mw = FindMonitorWindow(hwnd)) {
                if (mw->renderer) {
                    mw->renderer->Resize(LOWORD(lParam), HIWORD(lParam));
                }
            }
        }
        return 0;
    }

    case WM_SETCURSOR: {
        if (g_app && g_app->mode == RunMode::ScreensaverFull) {
            SetCursor(nullptr);
            return TRUE;
        }
        break;
    }

    case WM_MOUSEMOVE: {
        if (g_app && g_app->mode == RunMode::ScreensaverFull && g_app->gracePeriodTimer > 0.5f) {
            if (auto* mw = FindMonitorWindow(hwnd)) {
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                if (!mw->hasInitialMouse) {
                    mw->initialMousePos = pt;
                    mw->hasInitialMouse = true;
                } else {
                    int dx = std::abs(pt.x - mw->initialMousePos.x);
                    int dy = std::abs(pt.y - mw->initialMousePos.y);
                    if (dx > 12 || dy > 12) {
                        PostQuitMessage(0);
                    }
                }
            }
        }
        return 0;
    }

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN: {
        if (g_app && g_app->mode == RunMode::ScreensaverFull && g_app->gracePeriodTimer > 0.5f) {
            PostQuitMessage(0);
            return 0;
        }
        break;
    }

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        if (!g_app) break;

        if (g_app->mode == RunMode::ScreensaverFull) {
            if (g_app->gracePeriodTimer > 0.3f) {
                PostQuitMessage(0);
            }
            return 0;
        }

        if (g_app->mode == RunMode::StudioWindowed && !g_app->windows.empty()) {
            bool shiftHeld = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

            switch (wParam) {
            case VK_ESCAPE:
                if (g_app->isMultiMonitorStudioActive) {
                    ToggleMultiMonitorStudioPreview();
                } else if (g_app->isStudioPrimaryFullscreen) {
                    ToggleStudioPrimaryFullscreen(g_app->windows[0].hwnd);
                } else {
                    PostQuitMessage(0);
                }
                break;

            case VK_RIGHT:
                g_app->windows[0].renderer->NextEffect();
                UpdateStudioWindowTitle();
                break;

            case VK_LEFT:
                g_app->windows[0].renderer->PrevEffect();
                UpdateStudioWindowTitle();
                break;

            case VK_OEM_6: // ']' key: Next effect on Monitor 2
            case VK_TAB:
                if (g_app->windows.size() > 1 && g_app->windows[1].renderer) {
                    g_app->windows[1].renderer->NextEffect();
                    UpdateStudioWindowTitle();
                }
                break;

            case VK_OEM_4: // '[' key: Previous effect on Monitor 2
                if (g_app->windows.size() > 1 && g_app->windows[1].renderer) {
                    g_app->windows[1].renderer->PrevEffect();
                    UpdateStudioWindowTitle();
                }
                break;

            case VK_UP:
                for (auto& mw : g_app->windows) {
                    if (mw.renderer) mw.renderer->AdjustSpeed(0.25f);
                }
                UpdateStudioWindowTitle();
                break;

            case VK_DOWN:
                for (auto& mw : g_app->windows) {
                    if (mw.renderer) mw.renderer->AdjustSpeed(-0.25f);
                }
                UpdateStudioWindowTitle();
                break;

            case VK_SPACE:
                g_app->paused = !g_app->paused;
                UpdateStudioWindowTitle();
                break;

            case VK_F11:
                ToggleStudioPrimaryFullscreen(g_app->windows[0].hwnd);
                break;

            case 'M':
                ToggleMultiMonitorStudioPreview();
                break;

            case 'C':
                if (ShowConfigurationDialog(g_app->hInstance, g_app->windows[0].hwnd, g_app->config)) {
                    for (auto& mw : g_app->windows) {
                        if (mw.renderer) mw.renderer->ApplyConfig(g_app->config);
                    }
                    UpdateStudioWindowTitle();
                }
                break;

            default:
                if (wParam >= '1' && wParam <= '9') {
                    int idx = static_cast<int>(wParam - '1');
                    if (shiftHeld && g_app->windows.size() > 1 && g_app->windows[1].renderer) {
                        g_app->windows[1].renderer->SelectEffect(idx);
                    } else {
                        g_app->windows[0].renderer->SelectEffect(idx);
                    }
                    UpdateStudioWindowTitle();
                }
                break;
            }
            return 0;
        }
        break;
    }

    case WM_DESTROY:
        if (g_app && !g_app->windows.empty() && hwnd == g_app->windows[0].hwnd) {
            PostQuitMessage(0);
        }
        return 0;
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static void ParseCommandLineArgs(const char* cmdLine, RunMode& outMode, HWND& outParentHwnd) {
    outMode = RunMode::StudioWindowed;
    outParentHwnd = nullptr;

    char modulePath[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
    std::string exePath(modulePath);
    bool isScrBinary = false;
    if (exePath.size() >= 4) {
        std::string ext = exePath.substr(exePath.size() - 4);
        if (_stricmp(ext.c_str(), ".scr") == 0) {
            isScrBinary = true;
        }
    }

    if (!cmdLine || cmdLine[0] == '\0') {
        outMode = isScrBinary ? RunMode::ConfigDialog : RunMode::StudioWindowed;
        return;
    }

    const char* p = cmdLine;
    while (*p == ' ' || *p == '\t') ++p;
    if (*p == '/' || *p == '-') ++p;

    char flag = static_cast<char>(tolower(*p));
    if (flag == 's') {
        outMode = RunMode::ScreensaverFull;
    } else if (flag == 'w') {
        outMode = RunMode::StudioWindowed;
    } else if (flag == 'c') {
        outMode = RunMode::ConfigDialog;
        ++p;
        while (*p == ':' || *p == ' ' || *p == '\t') ++p;
        if (*p != '\0') {
            outParentHwnd = reinterpret_cast<HWND>(static_cast<uintptr_t>(std::strtoull(p, nullptr, 10)));
        }
    } else if (flag == 'p') {
        outMode = RunMode::PreviewChild;
        ++p;
        while (*p == ':' || *p == ' ' || *p == '\t') ++p;
        if (*p != '\0') {
            outParentHwnd = reinterpret_cast<HWND>(static_cast<uintptr_t>(std::strtoull(p, nullptr, 10)));
        }
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int) {
    SetProcessDPIAware();

    AppState app;
    app.hInstance = hInstance;
    app.config.LoadFromRegistry();

    HWND parentHwnd = nullptr;
    ParseCommandLineArgs(lpCmdLine, app.mode, parentHwnd);

    if (app.mode == RunMode::ConfigDialog) {
        ShowConfigurationDialog(hInstance, parentHwnd, app.config);
        return 0;
    }

    g_app = &app;

    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = app.windowClassName;
    RegisterClassExA(&wc);

    app.connectedMonitors = EnumerateConnectedMonitors();

    if (app.mode == RunMode::ScreensaverFull) {
        // Create an independent fullscreen window + D3D11 Renderer on EACH connected monitor
        int numMonitors = (std::min)(static_cast<int>(app.connectedMonitors.size()), kMaxMonitors);
        for (int i = 0; i < numMonitors; ++i) {
            const auto& mon = app.connectedMonitors[i];
            int x = mon.bounds.left;
            int y = mon.bounds.top;
            int w = mon.bounds.right - mon.bounds.left;
            int h = mon.bounds.bottom - mon.bounds.top;

            HWND hwnd = CreateWindowExA(
                WS_EX_TOPMOST,
                app.windowClassName,
                "Windows Screensaver Studio",
                WS_POPUP | WS_VISIBLE,
                x, y, w, h,
                nullptr,
                nullptr,
                hInstance,
                nullptr);

            if (hwnd) {
                MonitorWindow mw;
                mw.monitorIndex = i;
                mw.hwnd = hwnd;
                mw.renderer = std::make_unique<Renderer>();
                if (mw.renderer->Initialize(hwnd, w, h, i, app.config)) {
                    app.windows.push_back(std::move(mw));
                } else {
                    DestroyWindow(hwnd);
                }
            }
        }
    } else if (app.mode == RunMode::PreviewChild) {
        if (!parentHwnd || !IsWindow(parentHwnd)) {
            return 0;
        }
        RECT rc = {};
        GetClientRect(parentHwnd, &rc);
        int w = (std::max)(static_cast<int>(rc.right - rc.left), 1);
        int h = (std::max)(static_cast<int>(rc.bottom - rc.top), 1);

        HWND hwnd = CreateWindowExA(
            0,
            app.windowClassName,
            "Windows Screensaver Studio Preview",
            WS_CHILD | WS_VISIBLE,
            0, 0, w, h,
            parentHwnd,
            nullptr,
            hInstance,
            nullptr);

        if (hwnd) {
            MonitorWindow mw;
            mw.monitorIndex = 0;
            mw.hwnd = hwnd;
            mw.renderer = std::make_unique<Renderer>();
            if (mw.renderer->Initialize(hwnd, w, h, 0, app.config)) {
                app.windows.push_back(std::move(mw));
            }
        }
    } else {
        // Studio Windowed mode: open primary Studio window on Monitor 1 (and press 'M' to span Monitor 2!)
        int clientW = 1280, clientH = 720;
        DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
        RECT rc = { 0, 0, clientW, clientH };
        AdjustWindowRect(&rc, style, FALSE);
        int winW = rc.right - rc.left;
        int winH = rc.bottom - rc.top;

        const auto& mon0 = app.connectedMonitors[0];
        int monW = mon0.bounds.right - mon0.bounds.left;
        int monH = mon0.bounds.bottom - mon0.bounds.top;
        int x = mon0.bounds.left + (monW - winW) / 2;
        int y = mon0.bounds.top + (monH - winH) / 2;

        HWND hwnd = CreateWindowExA(
            0,
            app.windowClassName,
            "Windows Screensaver Studio",
            style,
            x, y, winW, winH,
            nullptr,
            nullptr,
            hInstance,
            nullptr);

        if (hwnd) {
            MonitorWindow mw;
            mw.monitorIndex = 0;
            mw.hwnd = hwnd;
            mw.renderer = std::make_unique<Renderer>();
            if (mw.renderer->Initialize(hwnd, clientW, clientH, 0, app.config)) {
                app.windows.push_back(std::move(mw));
            }
        }
    }

    if (app.windows.empty()) {
        MessageBoxA(nullptr, "Failed to initialize Direct3D 11 screensaver windows.", "Error", MB_ICONERROR);
        return 1;
    }

    UpdateStudioWindowTitle();

    LARGE_INTEGER freq, prevTime;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prevTime);

    MSG msg = {};
    while (true) {
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                for (auto& mw : app.windows) {
                    if (mw.renderer) mw.renderer->Shutdown();
                }
                return 0;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        if (app.mode == RunMode::PreviewChild && !IsWindow(parentHwnd)) {
            break;
        }

        LARGE_INTEGER currTime;
        QueryPerformanceCounter(&currTime);
        float dt = static_cast<float>(currTime.QuadPart - prevTime.QuadPart) / static_cast<float>(freq.QuadPart);
        prevTime = currTime;

        if (dt > 0.1f) dt = 0.1f;
        app.gracePeriodTimer += dt;

        float stepDt = app.paused ? 0.0f : dt;

        // Render secondary monitors first without blocking on VSync, then render Monitor 1 with VSync
        // so all monitors present in lockstep at full display refresh rate!
        for (size_t i = 1; i < app.windows.size(); ++i) {
            if (app.windows[i].renderer) {
                app.windows[i].renderer->SyncWithPrimary(*app.windows[0].renderer);
                app.windows[i].renderer->UpdateAndRender(stepDt, false);
            }
        }

        if (app.windows[0].renderer) {
            app.windows[0].renderer->UpdateAndRender(stepDt, true);
        }
    }

    for (auto& mw : app.windows) {
        if (mw.renderer) mw.renderer->Shutdown();
    }
    return 0;
}
