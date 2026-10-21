#pragma once

#include "../Engine/ShaderHelper.h"
#include <algorithm>
#include <cmath>

namespace ScreensaverEngine {

class CyberGameOfLifeEffect : public IScreensaver {
public:
    const char* GetName() const override {
        return "Conway's Game of Life (Cyber Automata)";
    }

    const char* GetDescription() const override {
        return "True GPU B3/S23 Conway's Game of Life cellular automaton with cell-age color evolution, glider phosphor trails, and continuous cosmic seeding.";
    }

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override {
        m_device = device;

        // 1. Simulation Pixel Shader (runs on the toroidal cellular grid)
        // R = Alive (0 or 1)
        // G = Cell survival age (0..1)
        // B = Phosphor trail heat (0..1)
        // A = Local activity metric
        static const char* kSimPS = R"HLSL(
cbuffer SimConstants : register(b0) {
    float2 uGridSize;
    float  uTime;
    float  uSeedTrigger;
    float2 uSeedCenter;
    float  uResetFlag;
    float  uPadding;
};

Texture2D<float4> uPrevState : register(t0);
SamplerState uWrapSampler    : register(s0);

struct PSIn {
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

float hash21(float2 p) {
    p = frac(p * float2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return frac(p.x * p.y);
}

float4 PSMain(PSIn input) : SV_TARGET {
    float2 texel = 1.0 / uGridSize;
    float2 uv = input.uv;
    float2 cellPos = floor(uv * uGridSize);

    // Initial primordial soup on startup
    if (uResetFlag > 0.5) {
        // Symmetric primordial clusters + random soup
        float2 symCell = abs(cellPos - uGridSize * 0.5);
        float h = hash21(symCell + float2(17.0, 31.0));
        float alive = (h > 0.62) ? 1.0 : 0.0;
        return float4(alive, alive * 0.05, alive, 1.0);
    }

    float4 self = uPrevState.SampleLevel(uWrapSampler, uv, 0);
    float wasAlive = step(0.5, self.r);

    // Count 8 Moore neighbors on the wrapping torus
    float neighbors = 0.0;
    float neighborTrail = 0.0;
    [unroll]
    for (int dy = -1; dy <= 1; ++dy) {
        [unroll]
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            float4 n = uPrevState.SampleLevel(uWrapSampler, uv + float2(dx, dy) * texel, 0);
            neighbors += step(0.5, n.r);
            neighborTrail = max(neighborTrail, n.b);
        }
    }

    // Classic Conway's Game of Life B3/S23 rules:
    // Birth on 3 neighbors; Survival on 2 or 3 neighbors
    float isAlive = 0.0;
    if (wasAlive > 0.5) {
        isAlive = (neighbors > 1.5 && neighbors < 3.5) ? 1.0 : 0.0;
    } else {
        isAlive = (neighbors > 2.5 && neighbors < 3.5) ? 1.0 : 0.0;
    }

    // Periodic cosmic seed burst: injects symmetric kaleidoscopic patterns / gliders
    // so the ecosystem stays vibrant and never stagnates
    if (uSeedTrigger > 0.5) {
        float2 delta = cellPos - uSeedCenter;
        // 4-fold mirror symmetry around seed center creates methuselahs, pulsars, and glider guns!
        float2 sym = abs(delta);
        if (max(sym.x, sym.y) < 11.0) {
            float seedHash = hash21(sym + floor(uTime * 13.0));
            isAlive = (seedHash > 0.48) ? 1.0 : 0.0;
        }
    }

    // Age increases while alive, resets on death
    float age = (isAlive > 0.5) ? saturate(self.g + 0.012) : 0.0;

    // Phosphor heat trail decays smoothly and picks up slight neighbor glow
    float trail = (isAlive > 0.5) ? 1.0 : max(self.b * 0.935, neighborTrail * 0.18);

    // Activity metric tracks whether state changed
    float changed = abs(isAlive - wasAlive);
    float activity = lerp(self.a, changed, 0.15);

    return float4(isAlive, age, trail, activity);
}
)HLSL";

        // 2. Display Pixel Shader (renders crisp LED/OLED cells + phosphor bloom trails)
        static const char* kDisplayPS = R"HLSL(
cbuffer StandardConstants : register(b0) {
    float2 uResolution;
    float  uTime;
    float  uDeltaTime;
    float  uAspect;
    float  uSpeed;
    float  uGridW;
    float  uGridH;
};

Texture2D<float4> uStateTex : register(t0);
SamplerState uPointSampler  : register(s0);
SamplerState uLinearSampler : register(s1);

struct PSIn {
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

float3 agePalette(float age, float t) {
    // Newborn cells: electric cyan/white -> Mature gliders: neon emerald -> Ancient oscillators: gold/magenta
    float3 newborn = float3(0.35, 0.98, 1.00);
    float3 mature  = float3(0.10, 0.92, 0.55);
    float3 ancient = 0.55 + 0.45 * cos(6.28318 * (age * 0.8 + t * 0.04 + float3(0.0, 0.33, 0.67)));
    float3 c = lerp(newborn, mature, smoothstep(0.0, 0.18, age));
    return lerp(c, ancient, smoothstep(0.18, 0.75, age));
}

float4 PSMain(PSIn input) : SV_TARGET {
    float2 gridSize = float2(uGridW, uGridH);
    float2 cellCoord = input.uv * gridSize;
    float2 cellID = floor(cellCoord);
    float2 cellUV = frac(cellCoord) - 0.5; // [-0.5, 0.5] inside cell

    float2 sampleUV = (cellID + 0.5) / gridSize;
    float4 state = uStateTex.SampleLevel(uPointSampler, sampleUV, 0);

    // Sample blurred neighborhood for soft neon bloom
    float2 texel = 1.0 / gridSize;
    float4 bloomSample = (
        uStateTex.SampleLevel(uLinearSampler, input.uv + float2(-texel.x, -texel.y), 0) +
        uStateTex.SampleLevel(uLinearSampler, input.uv + float2( texel.x, -texel.y), 0) +
        uStateTex.SampleLevel(uLinearSampler, input.uv + float2(-texel.x,  texel.y), 0) +
        uStateTex.SampleLevel(uLinearSampler, input.uv + float2( texel.x,  texel.y), 0)
    ) * 0.25;

    float alive = state.r;
    float age   = state.g;
    float trail = state.b;

    // Rounded sub-pixel tile shape
    float2 d = abs(cellUV) - 0.34;
    float roundedBox = length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
    float cellMask = 1.0 - smoothstep(0.0, 0.09, roundedBox);
    float coreHighlight = exp(-dot(cellUV, cellUV) * 9.0);

    // Deep cyber-grid background
    float3 bgCol = float3(0.012, 0.018, 0.035);
    float gridLine = 1.0 - smoothstep(0.44, 0.50, max(abs(cellUV.x), abs(cellUV.y)));
    bgCol *= (0.75 + 0.25 * gridLine);

    // Phosphor afterglow trail color (deep indigo -> electric teal -> cyan)
    float3 trailCol = lerp(float3(0.05, 0.12, 0.38), float3(0.0, 0.68, 0.85), pow(trail, 1.5));
    float3 col = bgCol + trailCol * pow(trail, 1.7) * (0.45 + 0.55 * cellMask);

    // Active living cell
    if (alive > 0.5) {
        float3 cellCol = agePalette(age, uTime);
        col = lerp(col, cellCol * (0.85 + 0.55 * coreHighlight), cellMask);
    }

    // Soft bloom halo around active clusters & gliders
    col += float3(0.08, 0.45, 0.65) * bloomSample.b * 0.32;
    col += float3(0.25, 0.85, 0.95) * bloomSample.r * 0.28;

    return float4(saturate(col), 1.0);
}
)HLSL";

        if (!ShaderHelper::CreateFullscreenQuadVS(device, m_vs)) return false;
        if (!ShaderHelper::CompilePixelShader(device, kSimPS, "PSMain", m_simPS)) return false;
        if (!ShaderHelper::CompilePixelShader(device, kDisplayPS, "PSMain", m_displayPS)) return false;
        if (!ShaderHelper::CreateConstantBuffer<SimConstants>(device, m_simCB)) return false;
        if (!ShaderHelper::CreateConstantBuffer<StandardConstants>(device, m_displayCB)) return false;

        // Create wrapping point & linear samplers
        D3D11_SAMPLER_DESC sampDesc = {};
        sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        if (FAILED(device->CreateSamplerState(&sampDesc, m_pointWrapSampler.GetAddressOf()))) return false;

        sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        if (FAILED(device->CreateSamplerState(&sampDesc, m_linearWrapSampler.GetAddressOf()))) return false;

        return CreatePingPongBuffers(256, 144);
    }

    void Resize(int width, int height) override {
        if (width <= 0 || height <= 0 || !m_device) return;
        // Scale cellular grid so each cell is ~6-8 pixels wide, keeping crisp aesthetics on any monitor
        int targetW = std::clamp(width / 7, 160, 384);
        int targetH = std::clamp(height / 7, 90, 216);
        if (targetW != m_gridW || targetH != m_gridH) {
            CreatePingPongBuffers(targetW, targetH);
        }
    }

    void Update(const FrameContext& frame) override {
        m_stepAccumulator += frame.deltaTime * 18.0f; // ~18 generations per second at 1.0x speed
        m_seedAccumulator += frame.deltaTime;

        m_displayConstants.resolutionX = static_cast<float>(frame.width);
        m_displayConstants.resolutionY = static_cast<float>(frame.height);
        m_displayConstants.time = frame.totalTime;
        m_displayConstants.deltaTime = frame.deltaTime;
        m_displayConstants.aspect = (frame.height > 0) ? (static_cast<float>(frame.width) / static_cast<float>(frame.height)) : 1.777f;
        m_displayConstants.speedMultiplier = frame.speedMultiplier;
        m_displayConstants.param1 = static_cast<float>(m_gridW);
        m_displayConstants.param2 = static_cast<float>(m_gridH);
    }

    void Render(const FrameContext& frame) override {
        auto* ctx = frame.context;

        ctx->IASetInputLayout(nullptr);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->VSSetShader(m_vs.Get(), nullptr, 0);

        // Step the Game of Life simulation when accumulator reaches 1 generation
        int steps = 0;
        while ((m_needsReset || m_stepAccumulator >= 1.0f) && steps < 3) {
            if (!m_needsReset) m_stepAccumulator -= 1.0f;
            ++steps;

            int src = m_currentIndex;
            int dst = 1 - m_currentIndex;

            D3D11_VIEWPORT simVP = {};
            simVP.Width = static_cast<float>(m_gridW);
            simVP.Height = static_cast<float>(m_gridH);
            simVP.MaxDepth = 1.0f;
            ctx->RSSetViewports(1, &simVP);

            ID3D11ShaderResourceView* nullSRV[] = { nullptr, nullptr };
            ctx->PSSetShaderResources(0, 2, nullSRV);

            ID3D11RenderTargetView* simRTV[] = { m_rtv[dst].Get() };
            ctx->OMSetRenderTargets(1, simRTV, nullptr);

            SimConstants simConsts = {};
            simConsts.gridW = static_cast<float>(m_gridW);
            simConsts.gridH = static_cast<float>(m_gridH);
            simConsts.time = frame.totalTime;
            simConsts.resetFlag = m_needsReset ? 1.0f : 0.0f;

            // Every ~2.2 seconds, spawn a new symmetric pattern burst at a pseudo-random location
            if (m_seedAccumulator >= 2.2f) {
                m_seedAccumulator = 0.0f;
                simConsts.seedTrigger = 1.0f;
                float angle = frame.totalTime * 2.39996f;
                simConsts.seedCenterX = (0.2f + 0.6f * std::abs(std::sin(angle * 1.3f))) * m_gridW;
                simConsts.seedCenterY = (0.2f + 0.6f * std::abs(std::cos(angle * 1.7f))) * m_gridH;
            }

            ShaderHelper::UpdateConstantBuffer(ctx, m_simCB.Get(), simConsts);

            ctx->PSSetShader(m_simPS.Get(), nullptr, 0);
            ID3D11Buffer* cbs[] = { m_simCB.Get() };
            ctx->PSSetConstantBuffers(0, 1, cbs);
            ID3D11ShaderResourceView* srvs[] = { m_srv[src].Get() };
            ctx->PSSetShaderResources(0, 1, srvs);
            ID3D11SamplerState* samplers[] = { m_pointWrapSampler.Get() };
            ctx->PSSetSamplers(0, 1, samplers);

            ctx->Draw(3, 0);

            ctx->PSSetShaderResources(0, 1, nullSRV);
            m_currentIndex = dst;
            m_needsReset = false;
        }

        // Restore main monitor viewport & render target
        D3D11_VIEWPORT mainVP = {};
        mainVP.Width = static_cast<float>(frame.width);
        mainVP.Height = static_cast<float>(frame.height);
        mainVP.MaxDepth = 1.0f;
        ctx->RSSetViewports(1, &mainVP);

        ID3D11RenderTargetView* mainRTV[] = { frame.rtv };
        ctx->OMSetRenderTargets(1, mainRTV, nullptr);

        ShaderHelper::UpdateConstantBuffer(ctx, m_displayCB.Get(), m_displayConstants);

        ctx->PSSetShader(m_displayPS.Get(), nullptr, 0);
        ID3D11Buffer* dispCBs[] = { m_displayCB.Get() };
        ctx->PSSetConstantBuffers(0, 1, dispCBs);
        ID3D11ShaderResourceView* dispSRVs[] = { m_srv[m_currentIndex].Get() };
        ctx->PSSetShaderResources(0, 1, dispSRVs);
        ID3D11SamplerState* dispSamplers[] = { m_pointWrapSampler.Get(), m_linearWrapSampler.Get() };
        ctx->PSSetSamplers(0, 2, dispSamplers);

        ctx->Draw(3, 0);

        ID3D11ShaderResourceView* nullSRV[] = { nullptr };
        ctx->PSSetShaderResources(0, 1, nullSRV);
    }

    void Shutdown() override {
        for (int i = 0; i < 2; ++i) {
            m_rtv[i].Reset();
            m_srv[i].Reset();
            m_tex[i].Reset();
        }
        m_pointWrapSampler.Reset();
        m_linearWrapSampler.Reset();
        m_simCB.Reset();
        m_displayCB.Reset();
        m_simPS.Reset();
        m_displayPS.Reset();
        m_vs.Reset();
    }

private:
    struct alignas(16) SimConstants {
        float gridW;
        float gridH;
        float time;
        float seedTrigger;
        float seedCenterX;
        float seedCenterY;
        float resetFlag;
        float padding;
    };

    bool CreatePingPongBuffers(int w, int h) {
        m_gridW = w;
        m_gridH = h;
        for (int i = 0; i < 2; ++i) {
            m_rtv[i].Reset();
            m_srv[i].Reset();
            m_tex[i].Reset();

            D3D11_TEXTURE2D_DESC td = {};
            td.Width = w;
            td.Height = h;
            td.MipLevels = 1;
            td.ArraySize = 1;
            td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            td.SampleDesc.Count = 1;
            td.Usage = D3D11_USAGE_DEFAULT;
            td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

            if (FAILED(m_device->CreateTexture2D(&td, nullptr, m_tex[i].GetAddressOf()))) return false;
            if (FAILED(m_device->CreateRenderTargetView(m_tex[i].Get(), nullptr, m_rtv[i].GetAddressOf()))) return false;
            if (FAILED(m_device->CreateShaderResourceView(m_tex[i].Get(), nullptr, m_srv[i].GetAddressOf()))) return false;
        }
        m_needsReset = true;
        m_currentIndex = 0;
        return true;
    }

    ID3D11Device* m_device = nullptr;
    int m_gridW = 256;
    int m_gridH = 144;
    int m_currentIndex = 0;
    bool m_needsReset = true;
    float m_stepAccumulator = 0.0f;
    float m_seedAccumulator = 0.0f;

    ComPtr<ID3D11VertexShader> m_vs;
    ComPtr<ID3D11PixelShader> m_simPS;
    ComPtr<ID3D11PixelShader> m_displayPS;
    ComPtr<ID3D11Buffer> m_simCB;
    ComPtr<ID3D11Buffer> m_displayCB;
    ComPtr<ID3D11SamplerState> m_pointWrapSampler;
    ComPtr<ID3D11SamplerState> m_linearWrapSampler;

    ComPtr<ID3D11Texture2D> m_tex[2];
    ComPtr<ID3D11RenderTargetView> m_rtv[2];
    ComPtr<ID3D11ShaderResourceView> m_srv[2];
    StandardConstants m_displayConstants = {};
};

} // namespace ScreensaverEngine
