#pragma once

#include "../Engine/ShaderHelper.h"
#include <algorithm>
#include <cmath>

namespace ScreensaverEngine {

class ReactionDiffusionCoralEffect : public IScreensaver {
public:
    const char* GetName() const override {
        return "Turing Reaction-Diffusion Morphogenesis";
    }

    const char* GetDescription() const override {
        return "Live GPU Gray-Scott reaction-diffusion chemical simulation growing organic coral labyrinths and mitotic spots with 3D specular emboss.";
    }

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override {
        m_device = device;

        static const char* kSimPS = R"HLSL(
cbuffer RDConstants : register(b0) {
    float2 uGridSize;
    float  uTime;
    float  uResetFlag;
    float2 uDropPos;
    float  uDropActive;
    float  uPadding;
};

Texture2D<float4> uPrevTex : register(t0);
SamplerState uWrapSampler  : register(s0);

struct PSIn {
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

float hash21(float2 p) {
    p = frac(p * float2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return frac(p.x * p.y);
}

float4 PSMain(PSIn input) : SV_TARGET {
    float2 uv = input.uv;
    if (uResetFlag > 0.5) {
        // Initialize with U = 1.0, and several circular seeds of V = 1.0
        float2 p = uv * uGridSize;
        float v = 0.0;
        for (int i = 1; i <= 7; ++i) {
            float2 center = uGridSize * float2(
                0.2 + 0.6 * hash21(float2(i * 11.0, 3.0)),
                0.2 + 0.6 * hash21(float2(7.0, i * 19.0))
            );
            if (length(p - center) < 9.0) v = 0.85;
        }
        return float4(1.0 - v, v, 0.0, 1.0);
    }

    float2 texel = 1.0 / uGridSize;
    float2 c  = uPrevTex.SampleLevel(uWrapSampler, uv, 0).rg;

    // 9-point isotropic Laplacian stencil
    float2 n  = uPrevTex.SampleLevel(uWrapSampler, uv + float2( 0, -1) * texel, 0).rg;
    float2 s  = uPrevTex.SampleLevel(uWrapSampler, uv + float2( 0,  1) * texel, 0).rg;
    float2 e  = uPrevTex.SampleLevel(uWrapSampler, uv + float2( 1,  0) * texel, 0).rg;
    float2 w  = uPrevTex.SampleLevel(uWrapSampler, uv + float2(-1,  0) * texel, 0).rg;
    float2 ne = uPrevTex.SampleLevel(uWrapSampler, uv + float2( 1, -1) * texel, 0).rg;
    float2 nw = uPrevTex.SampleLevel(uWrapSampler, uv + float2(-1, -1) * texel, 0).rg;
    float2 se = uPrevTex.SampleLevel(uWrapSampler, uv + float2( 1,  1) * texel, 0).rg;
    float2 sw = uPrevTex.SampleLevel(uWrapSampler, uv + float2(-1,  1) * texel, 0).rg;

    float2 laplacian = (n + s + e + w) * 0.20 + (ne + nw + se + sw) * 0.05 - c;

    // Slowly vary feed (F) and kill (K) across space and time between mitosis spots & coral waves
    float feed = 0.037 + 0.018 * (0.5 + 0.5 * sin(uTime * 0.12 + uv.x * 3.14159));
    float kill = 0.060 + 0.004 * (0.5 + 0.5 * cos(uTime * 0.15 + uv.y * 3.14159));

    float u = c.r;
    float v = c.g;
    float uvv = u * v * v;

    float du = 1.0 * laplacian.x - uvv + feed * (1.0 - u);
    float dv = 0.5 * laplacian.y + uvv - (feed + kill) * v;

    u = saturate(u + du * 0.92);
    v = saturate(v + dv * 0.92);

    // Periodic chemical droplet injection so patterns continually evolve
    if (uDropActive > 0.5) {
        float dist = length((uv - uDropPos) * uGridSize);
        if (dist < 7.5) {
            v = 0.85;
            u = 0.15;
        }
    }

    return float4(u, v, 0.0, 1.0);
}
)HLSL";

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

Texture2D<float4> uRDTex    : register(t0);
SamplerState uLinearSampler : register(s0);

struct PSIn {
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

float3 palette(float t) {
    float3 a = float3(0.5, 0.5, 0.55);
    float3 b = float3(0.5, 0.45, 0.45);
    float3 c = float3(1.0, 1.0, 1.0);
    float3 d = float3(0.05, 0.38, 0.68);
    return a + b * cos(6.28318 * (c * t + d));
}

float4 PSMain(PSIn input) : SV_TARGET {
    float2 texel = 1.0 / float2(uGridW, uGridH);
    float v  = uRDTex.SampleLevel(uLinearSampler, input.uv, 0).g;
    float vx = uRDTex.SampleLevel(uLinearSampler, input.uv + float2(texel.x, 0.0), 0).g;
    float vy = uRDTex.SampleLevel(uLinearSampler, input.uv + float2(0.0, texel.y), 0).g;

    // Compute 3D surface normal from chemical concentration gradient
    float3 normal = normalize(float3((v - vx) * 18.0, (v - vy) * 18.0, 1.0));
    float3 lightDir = normalize(float3(cos(uTime * 0.4) * 0.7, sin(uTime * 0.4) * 0.7, 0.85));
    float3 viewDir  = float3(0.0, 0.0, 1.0);
    float3 halfVec  = normalize(lightDir + viewDir);

    float diff = saturate(dot(normal, lightDir));
    float spec = pow(saturate(dot(normal, halfVec)), 36.0);

    float ridge = smoothstep(0.12, 0.42, v);
    float3 deepCol  = float3(0.02, 0.04, 0.09);
    float3 coralCol = palette(v * 1.4 - uTime * 0.05);

    float3 col = lerp(deepCol, coralCol * (0.45 + 0.65 * diff), ridge);
    col += float3(0.95, 0.98, 1.0) * spec * ridge * 0.75;

    return float4(saturate(col), 1.0);
}
)HLSL";

        if (!ShaderHelper::CreateFullscreenQuadVS(device, m_vs)) return false;
        if (!ShaderHelper::CompilePixelShader(device, kSimPS, "PSMain", m_simPS)) return false;
        if (!ShaderHelper::CompilePixelShader(device, kDisplayPS, "PSMain", m_displayPS)) return false;
        if (!ShaderHelper::CreateConstantBuffer<RDConstants>(device, m_simCB)) return false;
        if (!ShaderHelper::CreateConstantBuffer<StandardConstants>(device, m_displayCB)) return false;

        D3D11_SAMPLER_DESC sampDesc = {};
        sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        if (FAILED(device->CreateSamplerState(&sampDesc, m_linearWrapSampler.GetAddressOf()))) return false;

        return CreatePingPongBuffers(320, 180);
    }

    void Update(const FrameContext& frame) override {
        m_dropTimer += frame.deltaTime;
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

        D3D11_VIEWPORT simVP = {};
        simVP.Width = static_cast<float>(m_gridW);
        simVP.Height = static_cast<float>(m_gridH);
        simVP.MaxDepth = 1.0f;
        ctx->RSSetViewports(1, &simVP);

        ctx->PSSetShader(m_simPS.Get(), nullptr, 0);
        ID3D11SamplerState* samplers[] = { m_linearWrapSampler.Get() };
        ctx->PSSetSamplers(0, 1, samplers);

        // Run 8 PDE substeps per frame for rapid organic coral growth
        int substeps = (frame.deltaTime > 0.0f) ? 8 : 0;
        if (m_needsReset) substeps = 1;

        for (int step = 0; step < substeps; ++step) {
            int src = m_currentIndex;
            int dst = 1 - m_currentIndex;

            ID3D11ShaderResourceView* nullSRV[] = { nullptr };
            ctx->PSSetShaderResources(0, 1, nullSRV);

            ID3D11RenderTargetView* simRTV[] = { m_rtv[dst].Get() };
            ctx->OMSetRenderTargets(1, simRTV, nullptr);

            RDConstants rd = {};
            rd.gridW = static_cast<float>(m_gridW);
            rd.gridH = static_cast<float>(m_gridH);
            rd.time = frame.totalTime;
            rd.resetFlag = m_needsReset ? 1.0f : 0.0f;

            if (step == 0 && m_dropTimer > 2.5f) {
                m_dropTimer = 0.0f;
                rd.dropActive = 1.0f;
                rd.dropX = 0.15f + 0.70f * std::abs(std::sin(frame.totalTime * 1.91f));
                rd.dropY = 0.15f + 0.70f * std::abs(std::cos(frame.totalTime * 2.47f));
            }

            ShaderHelper::UpdateConstantBuffer(ctx, m_simCB.Get(), rd);
            ID3D11Buffer* cbs[] = { m_simCB.Get() };
            ctx->PSSetConstantBuffers(0, 1, cbs);
            ID3D11ShaderResourceView* srvs[] = { m_srv[src].Get() };
            ctx->PSSetShaderResources(0, 1, srvs);

            ctx->Draw(3, 0);
            ctx->PSSetShaderResources(0, 1, nullSRV);

            m_currentIndex = dst;
            m_needsReset = false;
        }

        // Render final embossed coral surface to monitor
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
        m_linearWrapSampler.Reset();
        m_simCB.Reset();
        m_displayCB.Reset();
        m_simPS.Reset();
        m_displayPS.Reset();
        m_vs.Reset();
    }

private:
    struct alignas(16) RDConstants {
        float gridW;
        float gridH;
        float time;
        float resetFlag;
        float dropX;
        float dropY;
        float dropActive;
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
            td.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
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
    int m_gridW = 320;
    int m_gridH = 180;
    int m_currentIndex = 0;
    bool m_needsReset = true;
    float m_dropTimer = 0.0f;

    ComPtr<ID3D11VertexShader> m_vs;
    ComPtr<ID3D11PixelShader> m_simPS;
    ComPtr<ID3D11PixelShader> m_displayPS;
    ComPtr<ID3D11Buffer> m_simCB;
    ComPtr<ID3D11Buffer> m_displayCB;
    ComPtr<ID3D11SamplerState> m_linearWrapSampler;

    ComPtr<ID3D11Texture2D> m_tex[2];
    ComPtr<ID3D11RenderTargetView> m_rtv[2];
    ComPtr<ID3D11ShaderResourceView> m_srv[2];
    StandardConstants m_displayConstants = {};
};

} // namespace ScreensaverEngine
