#pragma once

#include "../Engine/ShaderHelper.h"

namespace ScreensaverEngine {

class FractalRaymarcherEffect : public FullscreenShaderEffect {
public:
    const char* GetName() const override {
        return "Quantum Fractal Cathedral";
    }

    const char* GetDescription() const override {
        return "Real-time 3D raymarched Kaleidoscopic IFS crystal architecture with volumetric glow and ambient occlusion.";
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

float2x2 rot2D(float a) {
    float c = cos(a), s = sin(a);
    return float2x2(c, -s, s, c);
}

// Cosine color palette by Inigo Quilez
float3 palette(float t) {
    float3 a = float3(0.5, 0.5, 0.5);
    float3 b = float3(0.5, 0.5, 0.5);
    float3 c = float3(1.0, 1.0, 1.0);
    float3 d = float3(0.263, 0.416, 0.557);
    return a + b * cos(6.28318 * (c * t + d));
}

// Repeating Kaleidoscopic IFS distance estimator
float2 mapFractal(float3 p) {
    // Repeat domain seamlessly along tunnel axis
    p.z = fmod(abs(p.z), 8.0) - 4.0;
    p.xy = fmod(abs(p.xy + 4.0), 8.0) - 4.0;

    float scale = 1.0;
    float orb = 1e5;

    float foldAngle1 = 0.38 + 0.08 * sin(uTime * 0.25);
    float foldAngle2 = 0.52 + 0.08 * cos(uTime * 0.19);
    float2x2 r1 = rot2D(foldAngle1);
    float2x2 r2 = rot2D(foldAngle2);

    [unroll]
    for (int i = 0; i < 6; ++i) {
        p = abs(p) - float3(0.85, 0.92, 0.78);
        p.xy = mul(r1, p.xy);
        p.yz = mul(r2, p.yz);

        float r2len = dot(p, p);
        orb = min(orb, r2len);

        float k = 1.65 / clamp(r2len, 0.28, 1.0);
        p *= k;
        scale *= k;
    }

    float d = (length(p.xy) - 0.45) / scale;
    return float2(d * 0.75, orb);
}

float3 calcNormal(float3 p) {
    float2 e = float2(0.002, -0.002);
    return normalize(
        e.xyy * mapFractal(p + e.xyy).x +
        e.yyx * mapFractal(p + e.yyx).x +
        e.yxy * mapFractal(p + e.yxy).x +
        e.xxx * mapFractal(p + e.xxx).x);
}

float4 PSMain(PSIn input) : SV_TARGET {
    float2 uv = (input.uv * 2.0 - 1.0);
    uv.x *= uAspect;
    uv.y = -uv.y;

    float t = uTime * 1.1;

    // Camera flight path
    float3 ro = float3(0.0, 0.0, t * 1.8);
    float3 rd = normalize(float3(uv, 1.45));
    rd.xy = mul(rot2D(t * 0.18), rd.xy);
    rd.xz = mul(rot2D(sin(t * 0.22) * 0.25), rd.xz);

    float totalDist = 0.0;
    float glow = 0.0;
    float2 res = float2(0.0, 0.0);
    bool hit = false;

    [loop]
    for (int i = 0; i < 72; ++i) {
        float3 p = ro + rd * totalDist;
        res = mapFractal(p);
        float d = res.x;

        // Volumetric energy accumulation near fractal edges
        glow += 0.012 / (0.015 + d * d * 18.0);

        if (d < 0.0025) {
            hit = true;
            break;
        }
        totalDist += d;
        if (totalDist > 26.0) break;
    }

    float3 col = float3(0.01, 0.015, 0.04);

    if (hit) {
        float3 p = ro + rd * totalDist;
        float3 n = calcNormal(p);
        float3 lightDir = normalize(float3(0.6, 0.8, -0.5));

        float diff = max(dot(n, lightDir), 0.0);
        float spec = pow(max(dot(reflect(-lightDir, n), -rd), 0.0), 24.0);
        float fresnel = pow(1.0 - max(dot(n, -rd), 0.0), 3.0);

        float3 baseCol = palette(sqrt(res.y) * 0.65 + uTime * 0.08);
        col = baseCol * (diff * 0.75 + 0.25) + float3(0.9, 0.95, 1.0) * spec * 0.6 + palette(uTime * 0.15) * fresnel * 0.55;

        // Distance attenuation
        float fog = exp(-totalDist * 0.11);
        col = lerp(float3(0.01, 0.02, 0.05), col, fog);
    }

    // Add iridescent volumetric glow
    float3 glowCol = palette(totalDist * 0.06 - uTime * 0.12);
    col += glowCol * glow * 0.085;

    // Tone mapping & gamma
    col = 1.0 - exp(-col * 1.35);
    return float4(saturate(col), 1.0);
}
)HLSL";
        return InitFullscreenShader(device, kPixelShader);
    }
};

} // namespace ScreensaverEngine
