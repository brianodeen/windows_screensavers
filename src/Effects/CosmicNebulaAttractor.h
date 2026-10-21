#pragma once

#include "../Engine/ShaderHelper.h"

namespace ScreensaverEngine {

class CosmicNebulaAttractorEffect : public IScreensaver {
public:
    static constexpr UINT kParticleCount = 65536;

    const char* GetName() const override {
        return "Cosmic Nebula Swarm (65K GPU Particles)";
    }

    const char* GetDescription() const override {
        return "65,536 additive-blended 3D plasma particles flowing through harmonic vortex attractors on the GPU.";
    }

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override {
        static const char* kParticleVS = R"HLSL(
cbuffer StandardConstants : register(b0) {
    float2 uResolution;
    float  uTime;
    float  uDeltaTime;
    float  uAspect;
    float  uSpeed;
    float  uParam1;
    float  uParam2;
};

struct VSOut {
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
};

float hash11(uint n) {
    n = (n << 13U) ^ n;
    n = n * (n * n * 15731U + 789221U) + 1376312589U;
    return float(n & 0x7fffffffU) / float(0x7fffffff);
}

float3 palette(float t) {
    float3 a = float3(0.55, 0.45, 0.65);
    float3 b = float3(0.45, 0.45, 0.35);
    float3 c = float3(1.0, 1.0, 1.0);
    float3 d = float3(0.05, 0.35, 0.68);
    return a + b * cos(6.28318 * (c * t + d));
}

VSOut VSMain(uint vertexID : SV_VertexID) {
    uint particleID = vertexID / 6;
    uint cornerID   = vertexID % 6;

    // 2 triangles forming a quad billboard
    float2 quadCorners[6] = {
        float2(-1.0, -1.0), float2(-1.0,  1.0), float2( 1.0, -1.0),
        float2( 1.0, -1.0), float2(-1.0,  1.0), float2( 1.0,  1.0)
    };
    float2 corner = quadCorners[cornerID];

    float seed1 = hash11(particleID * 3U + 1U);
    float seed2 = hash11(particleID * 3U + 2U);
    float seed3 = hash11(particleID * 3U + 3U);

    // Parametric 3D toroidal strange attractor & spiral galaxy ribbons
    float t = uTime * 0.42 + seed1 * 62.8318;
    float ribbon = floor(seed2 * 5.0);
    float ribbonAngle = ribbon * 1.256637 + t * 0.55;

    float radius = 1.2 + 2.8 * pow(seed3, 0.75) + 0.55 * sin(t * 1.7 + seed1 * 12.0);
    float twist  = radius * 1.45 - uTime * 0.65 + ribbonAngle;

    float3 pos;
    pos.x = cos(twist) * radius + 0.45 * sin(t * 2.3 + seed2 * 15.0);
    pos.z = sin(twist) * radius + 0.45 * cos(t * 2.1 + seed3 * 15.0);
    pos.y = sin(radius * 2.2 - t * 1.5) * 0.85 + (seed2 - 0.5) * 0.9 * exp(-radius * 0.25);

    // Rotate the entire 3D nebula slowly in 3D space
    float camPitch = 0.52 + 0.25 * sin(uTime * 0.23);
    float camYaw   = uTime * 0.18;

    float cy = cos(camYaw), sy = sin(camYaw);
    float3 pYaw = float3(pos.x * cy - pos.z * sy, pos.y, pos.x * sy + pos.z * cy);

    float cp = cos(camPitch), sp = sin(camPitch);
    float3 viewPos = float3(pYaw.x, pYaw.y * cp - pYaw.z * sp, pYaw.y * sp + pYaw.z * cp + 5.8);

    VSOut output;
    if (viewPos.z <= 0.2) {
        output.position = float4(0, 0, -2, 1);
        output.uv = float2(0, 0);
        output.color = float4(0, 0, 0, 0);
        return output;
    }

    // Bokeh size scales gently with depth and particle energy
    float size = (0.018 + 0.032 * pow(seed1, 3.0)) * (4.5 / viewPos.z);
    float2 proj = viewPos.xy / (viewPos.z * 0.75);
    proj.x /= uAspect;

    output.position = float4(proj.x + corner.x * (size / uAspect), proj.y + corner.y * size, 0.5, 1.0);
    output.uv = corner;

    float3 col = palette(radius * 0.18 + seed2 * 0.25 - uTime * 0.08);
    float alpha = saturate(1.8 / viewPos.z) * (0.35 + 0.65 * seed3);
    output.color = float4(col, alpha);
    return output;
}
)HLSL";

        static const char* kParticlePS = R"HLSL(
struct PSIn {
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
};

float4 PSMain(PSIn input) : SV_TARGET {
    float r2 = dot(input.uv, input.uv);
    if (r2 > 1.0) discard;

    // Soft radial Gaussian core + bright hot center
    float glow = exp(-r2 * 4.5);
    float core = exp(-r2 * 18.0);
    float3 finalCol = input.color.rgb * glow + float3(1.0, 0.98, 0.95) * core * 0.65;
    return float4(finalCol * input.color.a * 0.55, 1.0);
}
)HLSL";

        if (!ShaderHelper::CompileVertexShader(device, kParticleVS, "VSMain", m_vs)) {
            return false;
        }
        if (!ShaderHelper::CompilePixelShader(device, kParticlePS, "PSMain", m_ps)) {
            return false;
        }
        if (!ShaderHelper::CreateConstantBuffer<StandardConstants>(device, m_constantBuffer)) {
            return false;
        }

        // Create additive blend state for glowing nebula particles
        D3D11_BLEND_DESC blendDesc = {};
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        if (FAILED(device->CreateBlendState(&blendDesc, m_additiveBlend.GetAddressOf()))) {
            return false;
        }

        return true;
    }

    void Update(const FrameContext& frame) override {
        m_constants.resolutionX = static_cast<float>(frame.width);
        m_constants.resolutionY = static_cast<float>(frame.height);
        m_constants.time = frame.totalTime;
        m_constants.deltaTime = frame.deltaTime;
        m_constants.aspect = (frame.height > 0) ? (static_cast<float>(frame.width) / static_cast<float>(frame.height)) : 1.777f;
        m_constants.speedMultiplier = frame.speedMultiplier;
    }

    void Render(const FrameContext& frame) override {
        auto* ctx = frame.context;

        // Deep space dark indigo background clear
        const float clearColor[4] = { 0.008f, 0.010f, 0.028f, 1.0f };
        ctx->ClearRenderTargetView(frame.rtv, clearColor);

        ShaderHelper::UpdateConstantBuffer(ctx, m_constantBuffer.Get(), m_constants);

        ctx->IASetInputLayout(nullptr);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        ctx->VSSetShader(m_vs.Get(), nullptr, 0);
        ctx->PSSetShader(m_ps.Get(), nullptr, 0);

        ID3D11Buffer* cbs[] = { m_constantBuffer.Get() };
        ctx->VSSetConstantBuffers(0, 1, cbs);
        ctx->PSSetConstantBuffers(0, 1, cbs);

        float blendFactor[4] = { 0, 0, 0, 0 };
        ctx->OMSetBlendState(m_additiveBlend.Get(), blendFactor, 0xFFFFFFFF);

        // Draw 65,536 particles (6 vertices per quad)
        ctx->Draw(kParticleCount * 6, 0);

        // Restore default blend state
        ctx->OMSetBlendState(nullptr, blendFactor, 0xFFFFFFFF);
    }

    void Shutdown() override {
        m_additiveBlend.Reset();
        m_constantBuffer.Reset();
        m_ps.Reset();
        m_vs.Reset();
    }

private:
    ComPtr<ID3D11VertexShader> m_vs;
    ComPtr<ID3D11PixelShader> m_ps;
    ComPtr<ID3D11Buffer> m_constantBuffer;
    ComPtr<ID3D11BlendState> m_additiveBlend;
    StandardConstants m_constants = {};
};

} // namespace ScreensaverEngine
