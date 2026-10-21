#pragma once

#include "../Engine/ShaderHelper.h"

namespace ScreensaverEngine {

class NeonCyberGridEffect : public FullscreenShaderEffect {
public:
    const char* GetName() const override {
        return "Neon Cyber Grid";
    }

    const char* GetDescription() const override {
        return "Procedural synthwave 3D canyon flight with glowing neon grid, twin retro sun, and starfield.";
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

float hash21(float2 p) {
    p = frac(p * float2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return frac(p.x * p.y);
}

float noise(float2 p) {
    float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash21(i);
    float b = hash21(i + float2(1.0, 0.0));
    float c = hash21(i + float2(0.0, 1.0));
    float d = hash21(i + float2(1.0, 1.0));
    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

float terrainHeight(float2 p) {
    // Central highway valley so the camera flies through a canyon
    float canyon = smoothstep(1.2, 6.5, abs(p.x));
    float h = noise(p * 0.22) * 2.8 + noise(p * 0.55) * 1.1;
    return h * canyon;
}

float4 PSMain(PSIn input) : SV_TARGET {
    float2 uv = (input.uv * 2.0 - 1.0);
    uv.x *= uAspect;
    uv.y = -uv.y; // Y up

    float t = uTime * 3.5;

    // Gentle camera banking
    float bank = sin(uTime * 0.45) * 0.08;
    float c = cos(bank), s = sin(bank);
    uv = float2(uv.x * c - uv.y * s, uv.x * s + uv.y * c);

    float3 ro = float3(sin(uTime * 0.4) * 0.6, 1.65, t);
    float3 rd = normalize(float3(uv.x, uv.y - 0.12, 1.6));

    // Sky & Retro Sun
    float3 skyTop = float3(0.03, 0.01, 0.10);
    float3 skyHorizon = float3(0.55, 0.06, 0.42);
    float skyGrad = saturate(rd.y * 2.2 + 0.15);
    float3 col = lerp(skyHorizon, skyTop, pow(skyGrad, 0.65));

    // Starfield in upper sky
    if (rd.y > 0.05) {
        float2 starUV = rd.xy / (rd.z + 0.5) * 45.0;
        float2 starID = floor(starUV);
        float2 starF = frac(starUV) - 0.5;
        float h = hash21(starID);
        if (h > 0.94) {
            float twinkle = 0.5 + 0.5 * sin(uTime * 4.0 + h * 60.0);
            float star = smoothstep(0.18, 0.0, length(starF)) * twinkle;
            col += star * float3(0.7, 0.9, 1.0) * smoothstep(0.05, 0.35, rd.y);
        }
    }

    // Synthwave Striped Sun
    float2 sunPos = float2(0.0, 0.22);
    float sunDist = length(uv - sunPos);
    if (sunDist < 0.42) {
        float stripes = sin((uv.y - sunPos.y) * 95.0 - uTime * 2.5);
        float stripeMask = smoothstep(-0.15, 0.05, stripes + (uv.y - sunPos.y) * 2.8);
        float sunMask = smoothstep(0.42, 0.405, sunDist) * stripeMask;
        float3 sunCol = lerp(float3(1.0, 0.1, 0.55), float3(1.0, 0.92, 0.2), saturate((uv.y - sunPos.y + 0.4) * 1.4));
        col = lerp(col, sunCol, sunMask);
    }
    // Sun halo
    col += float3(1.0, 0.2, 0.55) * exp(-sunDist * 4.2) * 0.55;

    // Raymarch the procedural neon grid terrain
    float dist = 0.0;
    bool hit = false;
    float3 p = ro;
    [loop]
    for (int i = 0; i < 68; ++i) {
        p = ro + rd * dist;
        float h = terrainHeight(p.xz);
        float d = p.y - h;
        if (d < 0.015) {
            hit = true;
            break;
        }
        dist += max(d * 0.55, 0.08);
        if (dist > 45.0) break;
    }

    if (hit) {
        float2 gridUV = p.xz * 1.25;
        float2 gridDist = abs(frac(gridUV - 0.5) - 0.5) / fwidth(gridUV);
        float lineFactor = min(gridDist.x, gridDist.y);
        float gridLine = 1.0 - smoothstep(0.0, 1.65, lineFactor);

        // Energy pulses racing down the grid lines
        float pulse = exp(-abs(frac(p.z * 0.08 - uTime * 0.6) - 0.5) * 12.0);
        float canyonFactor = saturate(abs(p.x) * 0.22);
        float3 neonColor = lerp(float3(0.0, 0.95, 1.0), float3(1.0, 0.08, 0.72), canyonFactor);
        neonColor += float3(0.4, 0.9, 1.0) * pulse * 0.8;

        float3 surfaceCol = float3(0.02, 0.01, 0.06);
        float3 terrainCol = lerp(surfaceCol, neonColor * 1.4, gridLine);

        // Distance fog towards the glowing horizon
        float fog = exp(-dist * 0.055);
        col = lerp(skyHorizon * 1.15, terrainCol, fog);
    }

    // Subtle scanline & vignette
    float scanline = 0.94 + 0.06 * sin(input.position.y * 1.8);
    float vig = 1.0 - dot(uv * 0.42, uv * 0.42);
    col *= scanline * saturate(vig);

    // ACES-inspired filmic tonemap
    col = (col * (2.51 * col + 0.03)) / (col * (2.43 * col + 0.59) + 0.14);
    return float4(saturate(col), 1.0);
}
)HLSL";
        return InitFullscreenShader(device, kPixelShader);
    }
};

} // namespace ScreensaverEngine
