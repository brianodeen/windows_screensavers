#include "Config.h"
#include "../Effects/EffectRegistry.h"
#include <algorithm>
#include <cstdio>
#include <string>

namespace ScreensaverEngine {

static const char* kRegistryKeyPath = "Software\\ScreensaverStudio";

const char* GetNighttimeModeLabel(int mode) {
    switch (mode) {
    case kNighttimeSoftDim:      return "Nighttime Soft & Dim (40%)";
    case kNighttimeUltraDark:    return "Ultra-Dark Sleep (20%)";
    case kNighttimeAutoSchedule: return "Auto-Night (10PM-7AM, 40%)";
    default:                     return "Off (100%)";
    }
}

namespace {
    BOOL CALLBACK MonitorEnumProc(HMONITOR hMon, HDC, LPRECT, LPARAM dwData) {
        auto* list = reinterpret_cast<std::vector<MonitorDesc>*>(dwData);
        MONITORINFOEXA mi = {};
        mi.cbSize = sizeof(MONITORINFOEXA);
        if (GetMonitorInfoA(hMon, &mi)) {
            MonitorDesc desc;
            desc.hMonitor = hMon;
            desc.bounds = mi.rcMonitor;
            desc.isPrimary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
            desc.displayName = mi.szDevice;
            list->push_back(desc);
        }
        return TRUE;
    }
}

std::vector<MonitorDesc> EnumerateConnectedMonitors() {
    std::vector<MonitorDesc> monitors;
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, reinterpret_cast<LPARAM>(&monitors));

    std::sort(monitors.begin(), monitors.end(), [](const MonitorDesc& a, const MonitorDesc& b) {
        if (a.isPrimary != b.isPrimary) return a.isPrimary;
        if (a.bounds.left != b.bounds.left) return a.bounds.left < b.bounds.left;
        return a.bounds.top < b.bounds.top;
    });

    if (monitors.empty()) {
        MonitorDesc fallback;
        fallback.bounds = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
        fallback.isPrimary = true;
        fallback.displayName = "Primary Display";
        monitors.push_back(fallback);
    }

    return monitors;
}

ScreensaverConfig::ScreensaverConfig() {
    monitorEffectIndex[0] = kEffectAutoCycle;
    for (int i = 1; i < kMaxMonitors; ++i) {
        monitorEffectIndex[i] = kEffectAutoCycleOffset;
    }
}

void ScreensaverConfig::LoadFromRegistry() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, kRegistryKeyPath, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(DWORD);
        DWORD val = 0;

        if (RegQueryValueExA(hKey, "SelectedEffect", nullptr, nullptr, reinterpret_cast<LPBYTE>(&val), &size) == ERROR_SUCCESS) {
            monitorEffectIndex[0] = static_cast<int>(val);
        }

        for (int i = 0; i < kMaxMonitors; ++i) {
            char valueName[32];
            std::snprintf(valueName, sizeof(valueName), "Monitor%dEffect", i);
            size = sizeof(DWORD);
            if (RegQueryValueExA(hKey, valueName, nullptr, nullptr, reinterpret_cast<LPBYTE>(&val), &size) == ERROR_SUCCESS) {
                monitorEffectIndex[i] = static_cast<int>(val);
            }
        }

        size = sizeof(DWORD);
        if (RegQueryValueExA(hKey, "CycleInterval", nullptr, nullptr, reinterpret_cast<LPBYTE>(&val), &size) == ERROR_SUCCESS) {
            cycleIntervalSeconds = static_cast<int>(val);
        }
        size = sizeof(DWORD);
        if (RegQueryValueExA(hKey, "SpeedPercent", nullptr, nullptr, reinterpret_cast<LPBYTE>(&val), &size) == ERROR_SUCCESS) {
            speedPercent = static_cast<int>(val);
        }
        size = sizeof(DWORD);
        if (RegQueryValueExA(hKey, "NighttimeMode", nullptr, nullptr, reinterpret_cast<LPBYTE>(&val), &size) == ERROR_SUCCESS) {
            nighttimeMode = std::clamp(static_cast<int>(val), 0, 3);
        }
        size = sizeof(DWORD);
        if (RegQueryValueExA(hKey, "VSync", nullptr, nullptr, reinterpret_cast<LPBYTE>(&val), &size) == ERROR_SUCCESS) {
            vsync = (val != 0);
        }

        RegCloseKey(hKey);
    }
}

void ScreensaverConfig::SaveToRegistry() const {
    HKEY hKey = nullptr;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, kRegistryKeyPath, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        for (int i = 0; i < kMaxMonitors; ++i) {
            char valueName[32];
            std::snprintf(valueName, sizeof(valueName), "Monitor%dEffect", i);
            DWORD val = static_cast<DWORD>(monitorEffectIndex[i]);
            RegSetValueExA(hKey, valueName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&val), sizeof(DWORD));
        }

        DWORD val = static_cast<DWORD>(cycleIntervalSeconds);
        RegSetValueExA(hKey, "CycleInterval", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&val), sizeof(DWORD));

        val = static_cast<DWORD>(speedPercent);
        RegSetValueExA(hKey, "SpeedPercent", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&val), sizeof(DWORD));

        val = static_cast<DWORD>(nighttimeMode);
        RegSetValueExA(hKey, "NighttimeMode", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&val), sizeof(DWORD));

        val = vsync ? 1 : 0;
        RegSetValueExA(hKey, "VSync", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&val), sizeof(DWORD));

        RegCloseKey(hKey);
    }
}

namespace {
    constexpr int IDC_COMBO_MON_BASE  = 200; // 200..207 for monitors 0..7
    constexpr int IDC_COMBO_SPEED     = 102;
    constexpr int IDC_COMBO_CYCLE     = 103;
    constexpr int IDC_CHECK_VSYNC     = 104;
    constexpr int IDC_COMBO_NIGHTTIME = 105;
    constexpr int IDC_BTN_OK          = IDOK;
    constexpr int IDC_BTN_CANCEL      = IDCANCEL;

    struct DialogState {
        ScreensaverConfig* config = nullptr;
        bool accepted = false;
        bool closed = false;
        int numMonitorControls = 2;
        HWND comboMonitor[kMaxMonitors] = {};
        HWND lblMonitorDesc[kMaxMonitors] = {};
        HWND comboSpeed = nullptr;
        HWND comboCycle = nullptr;
        HWND comboNighttime = nullptr;
        HWND checkVsync = nullptr;
        std::vector<std::unique_ptr<IScreensaver>> effects;
        std::vector<MonitorDesc> monitors;
    };

    int ComboSelToEffectIndex(int monitorIdx, int comboSel) {
        if (monitorIdx == 0) {
            return (comboSel <= 0) ? kEffectAutoCycle : (comboSel - 1);
        } else {
            if (comboSel == 0) return kEffectAutoCycleOffset;
            if (comboSel == 1) return kEffectMirrorPrimary;
            if (comboSel == 2) return kEffectAutoCycle;
            return comboSel - 3;
        }
    }

    int EffectIndexToComboSel(int monitorIdx, int effectIndex, int effectCount) {
        if (monitorIdx == 0) {
            if (effectIndex >= 0 && effectIndex < effectCount) return effectIndex + 1;
            return 0;
        } else {
            if (effectIndex == kEffectAutoCycleOffset) return 0;
            if (effectIndex == kEffectMirrorPrimary)   return 1;
            if (effectIndex == kEffectAutoCycle)       return 2;
            if (effectIndex >= 0 && effectIndex < effectCount) return effectIndex + 3;
            return 0;
        }
    }

    void UpdateMonitorDescription(DialogState* state, int monIdx) {
        if (!state || !state->comboMonitor[monIdx] || !state->lblMonitorDesc[monIdx]) return;
        int sel = static_cast<int>(SendMessageA(state->comboMonitor[monIdx], CB_GETCURSEL, 0, 0));
        int effIdx = ComboSelToEffectIndex(monIdx, sel);

        if (effIdx == kEffectAutoCycle) {
            SetWindowTextA(state->lblMonitorDesc[monIdx], "Automatically cycles through all screensavers in your library.");
        } else if (effIdx == kEffectAutoCycleOffset) {
            SetWindowTextA(state->lblMonitorDesc[monIdx], "Cycles through all effects, offset so this monitor shows a different visual than Monitor 1.");
        } else if (effIdx == kEffectMirrorPrimary) {
            SetWindowTextA(state->lblMonitorDesc[monIdx], "Renders the same effect currently playing on Monitor 1 at this monitor's native resolution.");
        } else if (effIdx >= 0 && effIdx < static_cast<int>(state->effects.size())) {
            SetWindowTextA(state->lblMonitorDesc[monIdx], state->effects[effIdx]->GetDescription());
        }
    }

    LRESULT CALLBACK ConfigWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        auto* state = reinterpret_cast<DialogState*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));

        switch (msg) {
        case WM_CREATE: {
            auto* cs = reinterpret_cast<CREATESTRUCTA*>(lParam);
            state = reinterpret_cast<DialogState*>(cs->lpCreateParams);
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

            HFONT hFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

            auto makeControl = [&](const char* cls, const char* text, DWORD style, int x, int y, int w, int h, int id) -> HWND {
                HWND ctrl = CreateWindowExA(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                    x, y, w, h, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), cs->hInstance, nullptr);
                SendMessageA(ctrl, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
                return ctrl;
            };

            int yCursor = 16;
            for (int m = 0; m < state->numMonitorControls; ++m) {
                char label[128];
                if (m < static_cast<int>(state->monitors.size())) {
                    const auto& mon = state->monitors[m];
                    int w = mon.bounds.right - mon.bounds.left;
                    int h = mon.bounds.bottom - mon.bounds.top;
                    std::snprintf(label, sizeof(label), "Monitor %d (%s%dx%d) Screensaver:",
                        m + 1, mon.isPrimary ? "Primary - " : "", w, h);
                } else {
                    std::snprintf(label, sizeof(label), "Monitor %d Screensaver:", m + 1);
                }

                makeControl("STATIC", label, 0, 20, yCursor, 400, 18, 0);
                yCursor += 20;

                HWND combo = makeControl("COMBOBOX", "", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
                    20, yCursor, 410, 560, IDC_COMBO_MON_BASE + m);
                state->comboMonitor[m] = combo;

                if (m == 0) {
                    SendMessageA(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("[ Auto-Cycle All Effects ]"));
                } else {
                    SendMessageA(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("[ Auto-Cycle (Different from Monitor 1) ]"));
                    SendMessageA(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("[ Mirror Monitor 1 ]"));
                    SendMessageA(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("[ Auto-Cycle (Same Order) ]"));
                }

                for (size_t i = 0; i < state->effects.size(); ++i) {
                    char itemText[128];
                    std::snprintf(itemText, sizeof(itemText), "%zu. %s", i + 1, state->effects[i]->GetName());
                    SendMessageA(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(itemText));
                }

                int initSel = EffectIndexToComboSel(
                    m,
                    state->config->monitorEffectIndex[m],
                    static_cast<int>(state->effects.size()));
                SendMessageA(combo, CB_SETCURSEL, initSel, 0);

                yCursor += 28;
                state->lblMonitorDesc[m] = makeControl("STATIC", "", SS_LEFT, 24, yCursor, 406, 32, 0);
                UpdateMonitorDescription(state, m);
                yCursor += 38;
            }

            // Nighttime / Sleep Mode Selector
            makeControl("STATIC", "Nighttime / Sleep Mode (Softens glare, warms blue light & dims output):", 0, 20, yCursor, 410, 18, 0);
            yCursor += 20;
            state->comboNighttime = makeControl("COMBOBOX", "", CBS_DROPDOWNLIST | WS_TABSTOP, 20, yCursor, 410, 180, IDC_COMBO_NIGHTTIME);
            const char* nightLabels[] = {
                "Off (100% Full Vibrancy & Brightness)",
                "Nighttime Soft & Dim (40% Brightness, Warm Low-Blue Glow, Soft Focus)",
                "Ultra-Dark Sleep Mode (20% Brightness, Deep Amber Candlelight)",
                "Auto-Schedule Nighttime (40% Soft & Dim from 10:00 PM to 7:00 AM)"
            };
            for (int i = 0; i < 4; ++i) {
                SendMessageA(state->comboNighttime, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(nightLabels[i]));
            }
            int nightSel = std::clamp(state->config->nighttimeMode, 0, 3);
            SendMessageA(state->comboNighttime, CB_SETCURSEL, nightSel, 0);
            yCursor += 36;

            makeControl("STATIC", "Animation Speed:", 0, 20, yCursor, 180, 18, 0);
            makeControl("STATIC", "Auto-Cycle Interval:", 0, 235, yCursor, 180, 18, 0);
            yCursor += 20;

            state->comboSpeed = makeControl("COMBOBOX", "", CBS_DROPDOWNLIST | WS_TABSTOP, 20, yCursor, 190, 160, IDC_COMBO_SPEED);
            const char* speedLabels[] = { "50% (Relaxed)", "100% (Normal)", "150% (Fast)", "200% (Hyper)" };
            const int speedValues[] = { 50, 100, 150, 200 };
            int speedSel = 1;
            for (int i = 0; i < 4; ++i) {
                SendMessageA(state->comboSpeed, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(speedLabels[i]));
                if (state->config->speedPercent == speedValues[i]) speedSel = i;
            }
            SendMessageA(state->comboSpeed, CB_SETCURSEL, speedSel, 0);

            state->comboCycle = makeControl("COMBOBOX", "", CBS_DROPDOWNLIST | WS_TABSTOP, 235, yCursor, 195, 160, IDC_COMBO_CYCLE);
            const char* cycleLabels[] = { "15 Seconds", "30 Seconds", "60 Seconds", "120 Seconds" };
            const int cycleValues[] = { 15, 30, 60, 120 };
            int cycleSel = 1;
            for (int i = 0; i < 4; ++i) {
                SendMessageA(state->comboCycle, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(cycleLabels[i]));
                if (state->config->cycleIntervalSeconds == cycleValues[i]) cycleSel = i;
            }
            SendMessageA(state->comboCycle, CB_SETCURSEL, cycleSel, 0);

            yCursor += 36;
            state->checkVsync = makeControl("BUTTON", "Enable VSync (Smooth tear-free frame pacing across monitors)",
                BS_AUTOCHECKBOX | WS_TABSTOP, 20, yCursor, 410, 22, IDC_CHECK_VSYNC);
            SendMessageA(state->checkVsync, BM_SETCHECK, state->config->vsync ? BST_CHECKED : BST_UNCHECKED, 0);

            yCursor += 36;
            makeControl("BUTTON", "Save && Apply", BS_DEFPUSHBUTTON | WS_TABSTOP, 235, yCursor, 95, 28, IDC_BTN_OK);
            makeControl("BUTTON", "Cancel", BS_PUSHBUTTON | WS_TABSTOP, 340, yCursor, 90, 28, IDC_BTN_CANCEL);
            return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            int code = HIWORD(wParam);
            if (id >= IDC_COMBO_MON_BASE && id < IDC_COMBO_MON_BASE + kMaxMonitors && code == CBN_SELCHANGE) {
                UpdateMonitorDescription(state, id - IDC_COMBO_MON_BASE);
            } else if (id == IDC_BTN_OK && state) {
                for (int m = 0; m < state->numMonitorControls; ++m) {
                    int sel = static_cast<int>(SendMessageA(state->comboMonitor[m], CB_GETCURSEL, 0, 0));
                    state->config->monitorEffectIndex[m] = ComboSelToEffectIndex(m, sel);
                }

                int nightSel = static_cast<int>(SendMessageA(state->comboNighttime, CB_GETCURSEL, 0, 0));
                state->config->nighttimeMode = std::clamp(nightSel, 0, 3);

                const int speedValues[] = { 50, 100, 150, 200 };
                int spdSel = static_cast<int>(SendMessageA(state->comboSpeed, CB_GETCURSEL, 0, 0));
                if (spdSel >= 0 && spdSel < 4) state->config->speedPercent = speedValues[spdSel];

                const int cycleValues[] = { 15, 30, 60, 120 };
                int cycSel = static_cast<int>(SendMessageA(state->comboCycle, CB_GETCURSEL, 0, 0));
                if (cycSel >= 0 && cycSel < 4) state->config->cycleIntervalSeconds = cycleValues[cycSel];

                state->config->vsync = (SendMessageA(state->checkVsync, BM_GETCHECK, 0, 0) == BST_CHECKED);
                state->config->SaveToRegistry();
                state->accepted = true;
                DestroyWindow(hwnd);
                return 0;
            } else if (id == IDC_BTN_CANCEL) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if (state) state->closed = true;
            return 0;
        }
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
}

bool ShowConfigurationDialog(HINSTANCE hInstance, HWND parentHwnd, ScreensaverConfig& config) {
    static bool registered = false;
    const char* className = "ScreensaverStudioConfigClass";
    if (!registered) {
        WNDCLASSEXA wc = {};
        wc.cbSize = sizeof(WNDCLASSEXA);
        wc.lpfnWndProc = ConfigWndProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        wc.lpszClassName = className;
        RegisterClassExA(&wc);
        registered = true;
    }

    DialogState state;
    state.config = &config;
    state.effects = CreateAllEffects();
    state.monitors = EnumerateConnectedMonitors();
    state.numMonitorControls = (std::clamp)(static_cast<int>(state.monitors.size()), 2, kMaxMonitors);

    int width = 465;
    int height = 250 + state.numMonitorControls * 106;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - width) / 2;
    int y = (screenH - height) / 2;

    if (parentHwnd && IsWindow(parentHwnd)) {
        EnableWindow(parentHwnd, FALSE);
    }

    HWND dlg = CreateWindowExA(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        className,
        "Windows Screensaver Studio - Multi-Monitor & Nighttime Settings",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, width, height,
        parentHwnd,
        nullptr,
        hInstance,
        &state);

    MSG msg = {};
    while (!state.closed && GetMessageA(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessageA(dlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

    if (parentHwnd && IsWindow(parentHwnd)) {
        EnableWindow(parentHwnd, TRUE);
        SetForegroundWindow(parentHwnd);
    }

    return state.accepted;
}

} // namespace ScreensaverEngine
