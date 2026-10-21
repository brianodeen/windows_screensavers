#pragma once

#include "IScreensaver.h"
#include "Config.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <memory>
#include <vector>

namespace ScreensaverEngine {

using Microsoft::WRL::ComPtr;

class Renderer {
public:
    bool Initialize(HWND hwnd, int width, int height, int monitorIndex, const ScreensaverConfig& config);
    void Resize(int width, int height);
    void UpdateAndRender(float deltaTime, bool allowVSyncWait = true);
    void Shutdown();

    void NextEffect();
    void PrevEffect();
    void SelectEffect(int index);
    void ApplyConfig(const ScreensaverConfig& config);
    void SyncWithPrimary(const Renderer& primaryRenderer);

    HWND GetHwnd() const { return m_hwnd; }
    int GetMonitorIndex() const { return m_monitorIndex; }
    int GetConfiguredEffectMode() const;
    int GetActiveEffectIndex() const { return m_activeIndex; }
    int GetEffectCount() const { return static_cast<int>(m_effects.size()); }
    const char* GetActiveEffectName() const;
    const char* GetActiveEffectDescription() const;
    float GetSpeedMultiplier() const { return m_speedMultiplier; }
    void AdjustSpeed(float delta);

private:
    bool CreateRenderTarget();
    void ResolveInitialEffectIndex();

    HWND m_hwnd = nullptr;
    int m_width = 1;
    int m_height = 1;
    int m_monitorIndex = 0;

    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<IDXGISwapChain> m_swapChain;
    ComPtr<ID3D11RenderTargetView> m_rtv;
    ComPtr<ID3D11RasterizerState> m_rasterizerState;

    std::vector<std::unique_ptr<IScreensaver>> m_effects;
    int m_activeIndex = 0;
    float m_totalTime = 0.0f;
    float m_cycleTimer = 0.0f;
    float m_speedMultiplier = 1.0f;
    ScreensaverConfig m_config;
};

} // namespace ScreensaverEngine
