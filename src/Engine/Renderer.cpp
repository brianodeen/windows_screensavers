#include "Renderer.h"
#include "ShaderHelper.h"
#include "../Effects/EffectRegistry.h"
#include <algorithm>

namespace ScreensaverEngine {

namespace {
    struct alignas(16) PostProcessConstants {
        float invResX;
        float invResY;
        float brightness;   // 1.0 = normal, 0.15 = Soft & Dim, 0.05 = Ultra-Dark
        float softness;     // 0.0 = sharp, 1.0+ = soft focus blur radius
        float warmth;       // 0.0 = normal, 0.65 = warm low-blue-light shift, 0.85 = deep amber
        float highlightComp;// Highlight compression strength
        float pad1;
        float pad2;
    };

    int ResolveEffectiveNighttimeMode(int configuredMode) {
        if (configuredMode == kNighttimeAutoSchedule) {
            SYSTEMTIME st = {};
            GetLocalTime(&st);
            // Active between 10:00 PM (22:00) and 7:00 AM (07:00)
            if (st.wHour >= 22 || st.wHour < 7) {
                return kNighttimeSoftDim;
            }
            return kNighttimeOff;
        }
        return configuredMode;
    }
}

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

    if (!InitPostProcessPipeline()) {
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

bool Renderer::InitPostProcessPipeline() {
    static const char* kPostPS = R"HLSL(
cbuffer PostProcessConstants : register(b0) {
    float2 uInvRes;
    float  uBrightness;
    float  uSoftness;
    float  uWarmth;
    float  uHighlightComp;
    float2 uPad;
};

Texture2D<float4> uSceneTex : register(t0);
SamplerState uLinearSampler : register(s0);

struct PSIn {
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

float4 PSMain(PSIn input) : SV_TARGET {
    float2 uv = input.uv;

    // Fast path when Nighttime Mode is Off
    if (uSoftness <= 0.001 && uWarmth <= 0.001 && uBrightness >= 0.999) {
        return uSceneTex.SampleLevel(uLinearSampler, uv, 0);
    }

    // 13-tap dual-ring spatial Gaussian/disc filter for velvety soft focus without harsh edges
    float2 r1 = uInvRes * (1.8 * uSoftness);
    float2 r2 = uInvRes * (4.2 * uSoftness);

    float3 col = uSceneTex.SampleLevel(uLinearSampler, uv, 0).rgb * 0.22;

    // Inner ring
    col += uSceneTex.SampleLevel(uLinearSampler, uv + float2( r1.x,  0.0), 0).rgb * 0.11;
    col += uSceneTex.SampleLevel(uLinearSampler, uv + float2(-r1.x,  0.0), 0).rgb * 0.11;
    col += uSceneTex.SampleLevel(uLinearSampler, uv + float2( 0.0,  r1.y), 0).rgb * 0.11;
    col += uSceneTex.SampleLevel(uLinearSampler, uv + float2( 0.0, -r1.y), 0).rgb * 0.11;

    // Outer diagonal & cardinal soft halo ring
    col += uSceneTex.SampleLevel(uLinearSampler, uv + float2( r2.x,  r2.y), 0).rgb * 0.055;
    col += uSceneTex.SampleLevel(uLinearSampler, uv + float2(-r2.x,  r2.y), 0).rgb * 0.055;
    col += uSceneTex.SampleLevel(uLinearSampler, uv + float2( r2.x, -r2.y), 0).rgb * 0.055;
    col += uSceneTex.SampleLevel(uLinearSampler, uv + float2(-r2.x, -r2.y), 0).rgb * 0.055;
    col += uSceneTex.SampleLevel(uLinearSampler, uv + float2( r2.x * 1.3, 0.0), 0).rgb * 0.03;
    col += uSceneTex.SampleLevel(uLinearSampler, uv + float2(-r2.x * 1.3, 0.0), 0).rgb * 0.03;
    col += uSceneTex.SampleLevel(uLinearSampler, uv + float2(0.0,  r2.y * 1.3), 0).rgb * 0.03;
    col += uSceneTex.SampleLevel(uLinearSampler, uv + float2(0.0, -r2.y * 1.3), 0).rgb * 0.03;

    // Gentle soft-knee highlight roll-off on bright spikes without darkening midtones
    col = (col * (1.0 + col * 0.25)) / (1.0 + col * uHighlightComp);

    // Circadian Warm Low-Blue-Light Shift:
    // Gently reduces harsh blue/cyan channels and warms toward soothing amber candlelight
    float luma = dot(col, float3(0.299, 0.587, 0.114));
    float3 warmTint = float3(1.12, 0.84, 0.45) * luma;
    float3 filtered = float3(
        lerp(col.r, warmTint.r, uWarmth * 0.45),
        lerp(col.g, warmTint.g, uWarmth * 0.40),
        lerp(col.b, warmTint.b * 0.60, uWarmth * 0.70)
    );

    // Gentle edge vignette in Nighttime Mode so monitor bezels fade softly
    float2 centered = uv * 2.0 - 1.0;
    float nightVignette = 1.0 - 0.12 * dot(centered, centered) * saturate(uSoftness);

    filtered *= uBrightness * saturate(nightVignette);
    return float4(saturate(filtered), 1.0);
}
)HLSL";

    if (!ShaderHelper::CreateFullscreenQuadVS(m_device.Get(), m_postVS)) return false;
    if (!ShaderHelper::CompilePixelShader(m_device.Get(), kPostPS, "PSMain", m_postPS)) return false;
    if (!ShaderHelper::CreateConstantBuffer<PostProcessConstants>(m_device.Get(), m_postCB)) return false;

    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    return SUCCEEDED(m_device->CreateSamplerState(&sampDesc, m_linearClampSampler.GetAddressOf()));
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
    m_sceneRTV.Reset();
    m_sceneSRV.Reset();
    m_sceneTex.Reset();

    ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
    if (FAILED(hr)) {
        return false;
    }
    hr = m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, m_rtv.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    // Create offscreen scene texture for post-processing (Nighttime soft blur & dimming)
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = m_width;
    td.Height = m_height;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    if (FAILED(m_device->CreateTexture2D(&td, nullptr, m_sceneTex.GetAddressOf()))) return false;
    if (FAILED(m_device->CreateRenderTargetView(m_sceneTex.Get(), nullptr, m_sceneRTV.GetAddressOf()))) return false;
    if (FAILED(m_device->CreateShaderResourceView(m_sceneTex.Get(), nullptr, m_sceneSRV.GetAddressOf()))) return false;

    return true;
}

void Renderer::Resize(int width, int height) {
    if (!m_swapChain || width <= 0 || height <= 0) return;
    if (width == m_width && height == m_height) return;

    m_width = width;
    m_height = height;

    m_context->OMSetRenderTargets(0, nullptr, nullptr);
    m_rtv.Reset();
    m_sceneRTV.Reset();
    m_sceneSRV.Reset();
    m_sceneTex.Reset();

    if (SUCCEEDED(m_swapChain->ResizeBuffers(0, m_width, m_height, DXGI_FORMAT_UNKNOWN, 0))) {
        CreateRenderTarget();
    }

    for (auto& fx : m_effects) {
        fx->Resize(m_width, m_height);
    }
}

void Renderer::UpdateAndRender(float deltaTime, bool allowVSyncWait) {
    if (m_effects.empty() || !m_rtv || !m_sceneRTV) return;

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

    int effectiveNightMode = ResolveEffectiveNighttimeMode(m_config.nighttimeMode);
    bool useNightFilter = (effectiveNightMode != kNighttimeOff);

    D3D11_VIEWPORT vp = {};
    vp.Width = static_cast<float>(m_width);
    vp.Height = static_cast<float>(m_height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;

    m_context->RSSetViewports(1, &vp);
    m_context->RSSetState(m_rasterizerState.Get());

    // Render active effect either to offscreen scene texture (when Nighttime filter is active)
    // or directly to the SwapChain backbuffer (when Nighttime filter is Off)
    ID3D11RenderTargetView* targetRTV = useNightFilter ? m_sceneRTV.Get() : m_rtv.Get();
    ID3D11RenderTargetView* rtvs[] = { targetRTV };
    m_context->OMSetRenderTargets(1, rtvs, nullptr);

    FrameContext frame = {};
    frame.device = m_device.Get();
    frame.context = m_context.Get();
    frame.rtv = targetRTV;
    frame.dsv = nullptr;
    frame.width = m_width;
    frame.height = m_height;
    frame.deltaTime = scaledDt;
    frame.totalTime = m_totalTime;
    frame.speedMultiplier = m_speedMultiplier;

    auto& activeEffect = m_effects[m_activeIndex];
    activeEffect->Update(frame);
    activeEffect->Render(frame);

    // If Nighttime Mode is active, run the soft-focus + low-blue-light + dimming post-process pass to m_rtv
    if (useNightFilter) {
        m_context->RSSetViewports(1, &vp);
        ID3D11RenderTargetView* finalRTVs[] = { m_rtv.Get() };
        m_context->OMSetRenderTargets(1, finalRTVs, nullptr);

        PostProcessConstants post = {};
        post.invResX = 1.0f / static_cast<float>(m_width);
        post.invResY = 1.0f / static_cast<float>(m_height);

        if (effectiveNightMode == kNighttimeUltraDark) {
            // Ultra-Dark Sleep Mode (20% deep amber candlelight, extra soft focus)
            post.brightness = 0.20f;
            post.softness = 1.85f;
            post.warmth = 0.72f;
            post.highlightComp = 0.40f;
        } else {
            // Nighttime Soft & Dim (40% warm low-blue glow, gentle soft focus)
            post.brightness = 0.40f;
            post.softness = 1.25f;
            post.warmth = 0.48f;
            post.highlightComp = 0.30f;
        }

        ShaderHelper::UpdateConstantBuffer(m_context.Get(), m_postCB.Get(), post);

        m_context->IASetInputLayout(nullptr);
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_context->VSSetShader(m_postVS.Get(), nullptr, 0);
        m_context->PSSetShader(m_postPS.Get(), nullptr, 0);

        ID3D11Buffer* cbs[] = { m_postCB.Get() };
        m_context->PSSetConstantBuffers(0, 1, cbs);
        ID3D11ShaderResourceView* srvs[] = { m_sceneSRV.Get() };
        m_context->PSSetShaderResources(0, 1, srvs);
        ID3D11SamplerState* samplers[] = { m_linearClampSampler.Get() };
        m_context->PSSetSamplers(0, 1, samplers);

        m_context->Draw(3, 0);

        ID3D11ShaderResourceView* nullSRV[] = { nullptr };
        m_context->PSSetShaderResources(0, 1, nullSRV);
    }

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

void Renderer::SetNighttimeMode(int mode) {
    m_config.nighttimeMode = std::clamp(mode, 0, 3);
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
    m_linearClampSampler.Reset();
    m_postCB.Reset();
    m_postPS.Reset();
    m_postVS.Reset();
    m_sceneSRV.Reset();
    m_sceneRTV.Reset();
    m_sceneTex.Reset();
    m_rasterizerState.Reset();
    m_rtv.Reset();
    m_swapChain.Reset();
    m_context.Reset();
    m_device.Reset();
}

} // namespace ScreensaverEngine
