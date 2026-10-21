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

// Nighttime / Sleep Mode options
enum NighttimeMode : int {
    kNighttimeOff          = 0, // 100% normal brightness & colors
    kNighttimeSoftDim      = 1, // Soft blur + warm low-blue-light + 15% dim brightness
    kNighttimeUltraDark    = 2, // Extra soft + deep amber shift + 5% ultra-dim bedroom mode
    kNighttimeAutoSchedule = 3  // Automatically enables Soft & Dim between 10:00 PM and 7:00 AM
};

struct MonitorDesc {
    HMONITOR hMonitor = nullptr;
    RECT bounds = { 0, 0, 1920, 1080 };
    bool isPrimary = false;
    std::string displayName;
};

// Enumerates all active display monitors, sorted with the Primary monitor first (index 0)
std::vector<MonitorDesc> EnumerateConnectedMonitors();

struct ScreensaverConfig {
    int monitorEffectIndex[kMaxMonitors];
    int cycleIntervalSeconds = 30;
    int speedPercent = 100;       // 100 = 1.0x speed
    int nighttimeMode = kNighttimeOff;
    bool vsync = true;

    ScreensaverConfig();

    void LoadFromRegistry();
    void SaveToRegistry() const;
};

// Returns human-readable label for current nighttime mode
const char* GetNighttimeModeLabel(int mode);

// Shows a native Win32 Configuration dialog for /c mode or interactive 'C' shortcut
bool ShowConfigurationDialog(HINSTANCE hInstance, HWND parentHwnd, ScreensaverConfig& config);

} // namespace ScreensaverEngine
