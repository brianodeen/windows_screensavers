#pragma once

#include "../Engine/ShaderHelper.h"

namespace ScreensaverEngine {

class AuroraPlasmaWavesEffect : public FullscreenShaderEffect {
public:
    const char* GetName() const override {
        return "Bioluminescent Aurora Flux";
    }

    const char* GetDescription() const override {
        return "Silky domain-warped aurora ribbons and harmonic plasma interference waves.";
    }

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override {
        static const char* kPixelShader = R"HLSL(
cbuffer StandardConstants : register(b0) {
    float2 uResolution;
    float  uTime;
    float  uDeltaTime;
    float  uAspect;
    float  uSpeed;
    float  uParam1;
    float  uParam2;
};

struct PSIn {
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

float2x2 rot(float a) {
    float c = cos(a), s = sin(a);
    return float2x2(c, -s, s, c);
}

float4 PSMain(PSIn input) : SV_TARGET {
    float2 uv = (input.uv * 2.0 - 1.0);
    uv.x *= uAspect;

    float t = uTime * 0.45;
    float2 p = uv * 1.4;

    float3 col = float3(0.01, 0.025, 0.06);

    // Iterative domain warping & harmonic ribbon accumulation
    float2 q = p;
    for (float i = 1.0; i <= 7.0; i += 1.0) {
        q = mul(rot(0.45 + 0.03 * i), q);
        q += 0.42 / i * float2(
            sin(i * 1.8 * q.y + t * 1.4 + 0.3 * i),
            cos(i * 1.8 * q.x + t * 1.1 + 0.5 * i)
        );

        float wave = abs(sin(q.y * 2.5 + q.x * 0.8 - t));
        float intensity = 0.022 / (0.012 + wave * wave * 1.8);

        float3 ribbonCol = 0.5 + 0.5 * cos(float3(0.0, 2.1, 4.2) + i * 0.65 + uTime * 0.25 + length(uv) * 0.8);
        col += ribbonCol * intensity;
    }

    // Subtle deep-sea/aurora shimmer highlights
    float caustic = sin(q.x * 6.0 + t) * cos(q.y * 6.0 - t);
    col += float3(0.05, 0.35, 0.45) * pow(saturate(caustic), 4.0) * 0.35;

    // Vignette & smooth tone curve
    float vig = 1.0 - 0.25 * dot(uv, uv);
    col *= saturate(vig);
    col = 1.0 - exp(-col * 1.15);

    return float4(saturate(col), 1.0);
}
)HLSL";
        return InitFullscreenShader(device, kPixelShader);
    }
};

} // namespace ScreensaverEngine
