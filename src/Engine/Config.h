#pragma once

#include <windows.h>
#include <vector>
#include <string>

namespace ScreensaverEngine {

constexpr int kMaxMonitors = 8;

// Special effect index values for per-monitor selection
constexpr int kEffectAutoCycle       = -1; // Cycles through all effects
constexpr int kEffectAutoCycleOffset = -2; // Cycles through all effects, offset from Monitor 1 so monitors differ
constexpr int kEffectMirrorPrimary   = -3; // Always matches Monitor 1's active effect

struct MonitorDesc {
    HMONITOR hMonitor = nullptr;
    RECT bounds = { 0, 0, 1920, 1080 };
    bool isPrimary = false;
    std::string displayName;
};

// Enumerates all active display monitors, sorted with the Primary monitor first (index 0)
std::vector<MonitorDesc> EnumerateConnectedMonitors();

struct ScreensaverConfig {
    // Per-monitor effect selection:
    // monitorEffectIndex[0] = Monitor 1 (default: kEffectAutoCycle)
    // monitorEffectIndex[1] = Monitor 2 (default: kEffectAutoCycleOffset so each monitor shows a different visual!)
    int monitorEffectIndex[kMaxMonitors];
    int cycleIntervalSeconds = 30;
    int speedPercent = 100; // 100 = 1.0x speed
    bool vsync = true;

    ScreensaverConfig();

    void LoadFromRegistry();
    void SaveToRegistry() const;
};

// Shows a native Win32 Configuration dialog for /c mode or interactive 'C' shortcut
bool ShowConfigurationDialog(HINSTANCE hInstance, HWND parentHwnd, ScreensaverConfig& config);

} // namespace ScreensaverEngine
