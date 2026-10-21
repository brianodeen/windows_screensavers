#include "Renderer.h"
#include "../Effects/EffectRegistry.h"
#include <algorithm>

namespace ScreensaverEngine {

bool Renderer::Initialize(HWND hwnd, int width, int height, int monitorIndex, const ScreensaverConfig& config) {
    m_hwnd = hwnd;
    m_width = (std::max)(width, 1);
    m_height = (std::max)(height, 1);
    m_monitorIndex = (std::clamp)(monitorIndex, 0, kMaxMonitors - 1);
    m_config = config;
    m_speedMultiplier = static_cast<float>(config.speedPercent) / 100.0f;

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = m_width;
    sd.BufferDesc.Height = m_height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };
    D3D_FEATURE_LEVEL selectedLevel;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &sd,
        m_swapChain.GetAddressOf(),
        m_device.GetAddressOf(),
        &selectedLevel,
        m_context.GetAddressOf());

    if (FAILED(hr)) {
        return false;
    }

    // Disable Alt+Enter DXGI fullscreen hijack so our Win32 window manager handles fullscreen cleanly
    ComPtr<IDXGIDevice> dxgiDevice;
    if (SUCCEEDED(m_device.As(&dxgiDevice))) {
        ComPtr<IDXGIAdapter> adapter;
        if (SUCCEEDED(dxgiDevice->GetAdapter(adapter.GetAddressOf()))) {
            ComPtr<IDXGIFactory> factory;
            if (SUCCEEDED(adapter->GetParent(IID_PPV_ARGS(factory.GetAddressOf())))) {
                factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
            }
        }
    }

    if (!CreateRenderTarget()) {
        return false;
    }

    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    m_device->CreateRasterizerState(&rd, m_rasterizerState.GetAddressOf());

    auto rawEffects = CreateAllEffects();
    for (auto& fx : rawEffects) {
        if (fx->Initialize(m_device.Get(), m_context.Get())) {
            fx->Resize(m_width, m_height);
            m_effects.push_back(std::move(fx));
        }
    }

    if (m_effects.empty()) {
        return false;
    }

    ResolveInitialEffectIndex();
    return true;
}

int Renderer::GetConfiguredEffectMode() const {
    return m_config.monitorEffectIndex[m_monitorIndex];
}

void Renderer::ResolveInitialEffectIndex() {
    if (m_effects.empty()) return;
    int count = static_cast<int>(m_effects.size());
    int mode = GetConfiguredEffectMode();

    if (mode >= 0 && mode < count) {
        m_activeIndex = mode;
    } else if (mode == kEffectAutoCycleOffset) {
        // Offset starting effect by monitor index so Monitor 1 & Monitor 2 show different visuals
        m_activeIndex = m_monitorIndex % count;
    } else {
        m_activeIndex = 0;
    }
}

void Renderer::SyncWithPrimary(const Renderer& primaryRenderer) {
    if (m_monitorIndex > 0 && GetConfiguredEffectMode() == kEffectMirrorPrimary) {
        int target = primaryRenderer.GetActiveEffectIndex();
        if (target >= 0 && target < static_cast<int>(m_effects.size())) {
            m_activeIndex = target;
        }
    }
}

bool Renderer::CreateRenderTarget() {
    m_rtv.Reset();
    ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
    if (FAILED(hr)) {
        return false;
    }
    hr = m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, m_rtv.GetAddressOf());
    return SUCCEEDED(hr);
}

void Renderer::Resize(int width, int height) {
    if (!m_swapChain || width <= 0 || height <= 0) return;
    if (width == m_width && height == m_height) return;

    m_width = width;
    m_height = height;

    m_context->OMSetRenderTargets(0, nullptr, nullptr);
    m_rtv.Reset();

    if (SUCCEEDED(m_swapChain->ResizeBuffers(0, m_width, m_height, DXGI_FORMAT_UNKNOWN, 0))) {
        CreateRenderTarget();
    }

    for (auto& fx : m_effects) {
        fx->Resize(m_width, m_height);
    }
}

void Renderer::UpdateAndRender(float deltaTime, bool allowVSyncWait) {
    if (m_effects.empty() || !m_rtv) return;

    float scaledDt = deltaTime * m_speedMultiplier;
    m_totalTime += scaledDt;

    int mode = GetConfiguredEffectMode();
    if ((mode == kEffectAutoCycle || mode == kEffectAutoCycleOffset) && m_effects.size() > 1) {
        m_cycleTimer += deltaTime;
        float interval = static_cast<float>((std::max)(m_config.cycleIntervalSeconds, 5));
        if (m_cycleTimer >= interval) {
            m_cycleTimer = 0.0f;
            NextEffect();
        }
    }

    D3D11_VIEWPORT vp = {};
    vp.Width = static_cast<float>(m_width);
    vp.Height = static_cast<float>(m_height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;

    m_context->RSSetViewports(1, &vp);
    m_context->RSSetState(m_rasterizerState.Get());

    ID3D11RenderTargetView* rtvs[] = { m_rtv.Get() };
    m_context->OMSetRenderTargets(1, rtvs, nullptr);

    FrameContext frame = {};
    frame.device = m_device.Get();
    frame.context = m_context.Get();
    frame.rtv = m_rtv.Get();
    frame.dsv = nullptr;
    frame.width = m_width;
    frame.height = m_height;
    frame.deltaTime = scaledDt;
    frame.totalTime = m_totalTime;
    frame.speedMultiplier = m_speedMultiplier;

    auto& activeEffect = m_effects[m_activeIndex];
    activeEffect->Update(frame);
    activeEffect->Render(frame);

    UINT syncInterval = (m_config.vsync && allowVSyncWait) ? 1 : 0;
    m_swapChain->Present(syncInterval, 0);
}

void Renderer::NextEffect() {
    if (m_effects.empty()) return;
    m_activeIndex = (m_activeIndex + 1) % static_cast<int>(m_effects.size());
    m_cycleTimer = 0.0f;
}

void Renderer::PrevEffect() {
    if (m_effects.empty()) return;
    int n = static_cast<int>(m_effects.size());
    m_activeIndex = (m_activeIndex - 1 + n) % n;
    m_cycleTimer = 0.0f;
}

void Renderer::SelectEffect(int index) {
    if (index >= 0 && index < static_cast<int>(m_effects.size())) {
        m_activeIndex = index;
        // Lock this monitor to manual selection until config changes
        m_config.monitorEffectIndex[m_monitorIndex] = index;
        m_cycleTimer = 0.0f;
    }
}

void Renderer::ApplyConfig(const ScreensaverConfig& config) {
    m_config = config;
    m_speedMultiplier = static_cast<float>(config.speedPercent) / 100.0f;
    ResolveInitialEffectIndex();
    m_cycleTimer = 0.0f;
}

void Renderer::AdjustSpeed(float delta) {
    m_speedMultiplier = std::clamp(m_speedMultiplier + delta, 0.1f, 4.0f);
}

const char* Renderer::GetActiveEffectName() const {
    if (m_effects.empty()) return "None";
    return m_effects[m_activeIndex]->GetName();
}

const char* Renderer::GetActiveEffectDescription() const {
    if (m_effects.empty()) return "";
    return m_effects[m_activeIndex]->GetDescription();
}

void Renderer::Shutdown() {
    for (auto& fx : m_effects) {
        fx->Shutdown();
    }
    m_effects.clear();
    m_rasterizerState.Reset();
    m_rtv.Reset();
    m_swapChain.Reset();
    m_context.Reset();
    m_device.Reset();
}

} // namespace ScreensaverEngine
