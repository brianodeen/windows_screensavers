#pragma once

#include "../Engine/ShaderHelper.h"

namespace ScreensaverEngine {

// ============================================================================
// 1. Gargantua Relativistic Black Hole & Accretion Disk
// ============================================================================
class GargantuaBlackHoleEffect : public FullscreenShaderEffect {
public:
    const char* GetName() const override {
        return "Gargantua Black Hole (Gravitational Lensing)";
    }

    const char* GetDescription() const override {
        return "Relativistic gravitational ray-bending around an event horizon with Doppler-beamed accretion plasma and photon ring.";
    }

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override {
        static const char* kPS = R"HLSL(
cbuffer StandardConstants : register(b0) {
    float2 uResolution; float uTime; float uDeltaTime;
    float uAspect; float uSpeed; float uParam1; float uParam2;
};
struct PSIn { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };

float hash21(float2 p) {
    p = frac(p * float2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return frac(p.x * p.y);
}

float noise(float2 p) {
    float2 i = floor(p), f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(hash21(i), hash21(i + float2(1, 0)), f.x),
                lerp(hash21(i + float2(0, 1)), hash21(i + float2(1, 1)), f.x), f.y);
}

float fbm(float2 p) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 4; ++i) {
        v += a * noise(p);
        p = mul(float2x2(0.8, -0.6, 0.6, 0.8), p * 2.03);
        a *= 0.5;
    }
    return v;
}

float4 PSMain(PSIn input) : SV_TARGET {
    float2 uv = (input.uv * 2.0 - 1.0) * float2(uAspect, -1.0);
    float t = uTime * 0.65;

    // Camera tilts slowly around the accretion disk plane
    float tilt = 0.22 + 0.14 * sin(t * 0.35);
    float roll = 0.18 * cos(t * 0.23);
    float cr = cos(roll), sr = sin(roll);
    uv = float2(uv.x * cr - uv.y * sr, uv.x * sr + uv.y * cr);

    float3 ro = float3(0.0, sin(tilt) * 7.2, -cos(tilt) * 7.2);
    float3 forward = normalize(-ro);
    float3 right = normalize(cross(float3(0, 1, 0), forward));
    float3 up = cross(forward, right);
    float3 rd = normalize(forward * 1.65 + right * uv.x + up * uv.y);

    float3 p = ro;
    float3 v = rd;
    float3 accumGlow = float3(0, 0, 0);
    bool swallowed = false;

    // Geodesic ray integration through curved spacetime
    [loop]
    for (int i = 0; i < 90; ++i) {
        float r2 = dot(p, p);
        float r = sqrt(r2);

        if (r < 1.0) {
            swallowed = true;
            break;
        }
        if (r > 18.0) break;

        // Gravitational deflection towards origin: a ~ -1.5 * R_s * L^2 / r^5
        float stepSize = max(0.035, (r - 0.92) * 0.085);
        float3 prevP = p;
        float3 accel = -p * (1.85 / (r2 * r + 0.05));
        v = normalize(v + accel * stepSize);
        p += v * stepSize;

        // Check if geodesic crossed the equatorial accretion disk plane (y == 0)
        if (prevP.y * p.y <= 0.0 || abs(p.y) < 0.14) {
            float diskR = length(p.xz);
            if (diskR > 1.45 && diskR < 6.8) {
                float angle = atan2(p.z, p.x);
                float orbitalSpeed = 4.5 / pow(diskR, 0.75);
                float2 spiralCoord = float2(diskR * 2.8, angle * 2.0 + t * orbitalSpeed);
                float plasma = fbm(spiralCoord) * fbm(spiralCoord * 2.3 - float2(0, t * 2.0));

                float radialFade = smoothstep(1.45, 1.95, diskR) * smoothstep(6.8, 2.8, diskR);
                float verticalFade = exp(-abs(p.y) * 16.0);

                // Relativistic Doppler beaming: approaching side of disk is hotter/brighter blue-white,
                // receding side is red-shifted orange-gold
                float doppler = 1.0 - 0.48 * (p.x / diskR);
                float3 hotCol  = float3(0.75, 0.92, 1.00);
                float3 warmCol = float3(1.00, 0.52, 0.12);
                float3 deepCol = float3(0.75, 0.12, 0.02);

                float temp = saturate((6.2 - diskR) / 4.5 * doppler);
                float3 diskCol = lerp(deepCol, warmCol, smoothstep(0.0, 0.55, temp));
                diskCol = lerp(diskCol, hotCol, smoothstep(0.55, 1.0, temp));

                accumGlow += diskCol * (0.35 + 1.8 * plasma) * radialFade * verticalFade * pow(doppler, 2.2) * 0.42;
            }
        }

        // Photon sphere halo at r ~ 1.5
        float photonRing = exp(-abs(r - 1.38) * 14.0);
        accumGlow += float3(1.0, 0.78, 0.45) * photonRing * 0.014;
    }

    float3 col = float3(0, 0, 0);
    if (!swallowed) {
        // Lensed background starfield along the deflected exit ray v
        float2 starUV = v.xy / (abs(v.z) + 0.6) * 38.0;
        float2 id = floor(starUV);
        float2 f = frac(starUV) - 0.5;
        float h = hash21(id);
        if (h > 0.93) {
            float star = smoothstep(0.22, 0.0, length(f)) * (0.6 + 0.4 * sin(uTime * 3.0 + h * 50.0));
            col += star * lerp(float3(1.0, 0.8, 0.6), float3(0.6, 0.85, 1.0), frac(h * 17.0));
        }
        // Subtle lensed cosmic nebula backdrop
        col += float3(0.04, 0.07, 0.16) * fbm(v.xy * 3.5);
    }

    col += accumGlow;
    col = 1.0 - exp(-col * 1.25);
    return float4(saturate(col), 1.0);
}
)HLSL";
        return InitFullscreenShader(device, kPS);
    }
};

// ============================================================================
// 2. Relativistic Hyperspace Starfield & Nebula Tunnel
// ============================================================================
class WarpSpeedStarfieldEffect : public FullscreenShaderEffect {
public:
    const char* GetName() const override {
        return "Hyperspace Warp Starfield";
    }

    const char* GetDescription() const override {
        return "Multi-layered 3D relativistic star streaks racing through glowing volumetric nebula clouds.";
    }

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override {
        static const char* kPS = R"HLSL(
cbuffer StandardConstants : register(b0) {
    float2 uResolution; float uTime; float uDeltaTime;
    float uAspect; float uSpeed; float uParam1; float uParam2;
};
struct PSIn { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };

float hash21(float2 p) {
    p = frac(p * float2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return frac(p.x * p.y);
}

float4 PSMain(PSIn input) : SV_TARGET {
    float2 uv = (input.uv * 2.0 - 1.0) * float2(uAspect, -1.0);
    float t = uTime * 1.4;

    // Gentle drift of the warp vanishing point
    float2 center = float2(sin(t * 0.35) * 0.22, cos(t * 0.27) * 0.15);
    float2 p = uv - center;

    float r = length(p);
    float angle = atan2(p.y, p.x);

    // Swirling volumetric nebula tunnel
    float3 col = float3(0.01, 0.015, 0.04);
    for (float layer = 1.0; layer <= 3.0; layer += 1.0) {
        float twist = angle + 1.0 / (r + 0.25) * 0.45 - t * 0.3 * layer;
        float neb = sin(twist * 4.0 + layer) * cos(log(r + 0.05) * 6.0 - t * 1.8);
        float3 nebCol = 0.5 + 0.5 * cos(float3(0.0, 2.1, 4.2) + layer * 1.4 + t * 0.2);
        col += nebCol * pow(saturate(neb), 3.0) * smoothstep(0.02, 0.6, r) * 0.22;
    }

    // 18 depth planes of stars streaking past the camera
    const float numLayers = 18.0;
    for (float i = 0.0; i < numLayers; i += 1.0) {
        float depth = frac(i / numLayers - t * 0.32);
        float scale = lerp(18.0, 0.35, 1.0 - depth);
        float fade = smoothstep(0.0, 0.15, depth) * smoothstep(1.0, 0.72, depth);

        float2 gridUV = p * scale + float2(i * 17.3, i * 31.7);
        float2 id = floor(gridUV);
        float2 local = frac(gridUV) - 0.5;

        float h = hash21(id);
        if (h > 0.45) {
            float2 offset = (float2(hash21(id + 1.0), hash21(id + 2.0)) - 0.5) * 0.65;
            float2 d = local - offset;

            // Radial motion blur elongation for warp-speed streaks
            float2 radDir = normalize(p + 1e-4);
            float2 perpDir = float2(-radDir.y, radDir.x);
            float along = dot(d, radDir) * 0.35;
            float across = dot(d, perpDir);
            float streakDist = length(float2(along, across));

            float star = 0.0045 / (streakDist * streakDist + 0.0004) * fade;
            float3 starCol = 0.65 + 0.35 * cos(float3(0.0, 2.0, 4.0) + h * 15.0);
            col += starCol * star * 0.14;
        }
    }

    // Bright hyperspace core flash
    col += float3(0.45, 0.75, 1.0) * exp(-r * 5.5) * 0.45;
    col = 1.0 - exp(-col * 1.25);
    return float4(saturate(col), 1.0);
}
)HLSL";
        return InitFullscreenShader(device, kPS);
    }
};

// ============================================================================
// 3. Magnetic Liquid Ferrofluid Spikes
// ============================================================================
class LiquidFerrofluidEffect : public FullscreenShaderEffect {
public:
    const char* GetName() const override {
        return "Magnetic Liquid Ferrofluid";
    }

    const char* GetDescription() const override {
        return "Raymarched 3D liquid-metal ferrofluid sculpture erupting with harmonic magnetic spikes under neon studio lights.";
    }

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override {
        static const char* kPS = R"HLSL(
cbuffer StandardConstants : register(b0) {
    float2 uResolution; float uTime; float uDeltaTime;
    float uAspect; float uSpeed; float uParam1; float uParam2;
};
struct PSIn { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };

float smin(float a, float b, float k) {
    float h = saturate(0.5 + 0.5 * (b - a) / k);
    return lerp(b, a, h) - k * h * (1.0 - h);
}

float mapFerrofluid(float3 p, float t) {
    // Base morphing metallic core + 3 orbiting magnetic droplets
    float d = length(p) - 1.35;
    for (int i = 0; i < 3; ++i) {
        float fi = float(i) * 2.094395;
        float3 orb = float3(cos(t * 0.7 + fi), sin(t * 0.9 + fi * 1.3), sin(t * 0.5 - fi)) * 1.15;
        d = smin(d, length(p - orb) - 0.65, 0.55);
    }

    // Normal-aligned magnetic Rosensweig instability spikes
    float3 n = normalize(p + 1e-4);
    float freq = 8.0 + 3.0 * sin(t * 0.4);
    float spikes = sin(n.x * freq + t * 1.8) * sin(n.y * freq - t * 1.4) * sin(n.z * freq + t * 1.2);
    spikes = pow(abs(spikes), 1.6) * sign(spikes);

    return d - spikes * 0.24;
}

float3 calcNormal(float3 p, float t) {
    float2 e = float2(0.0025, -0.0025);
    return normalize(
        e.xyy * mapFerrofluid(p + e.xyy, t) +
        e.yyx * mapFerrofluid(p + e.yyx, t) +
        e.yxy * mapFerrofluid(p + e.yxy, t) +
        e.xxx * mapFerrofluid(p + e.xxx, t));
}

float4 PSMain(PSIn input) : SV_TARGET {
    float2 uv = (input.uv * 2.0 - 1.0) * float2(uAspect, -1.0);
    float t = uTime * 1.1;

    float3 ro = float3(0.0, 0.0, -4.2);
    float3 rd = normalize(float3(uv, 1.55));

    float dist = 0.0;
    bool hit = false;
    [loop]
    for (int i = 0; i < 72; ++i) {
        float3 p = ro + rd * dist;
        float d = mapFerrofluid(p, t) * 0.55;
        if (d < 0.002) { hit = true; break; }
        dist += d;
        if (dist > 10.0) break;
    }

    // Dark studio backdrop with radial glow
    float3 col = float3(0.015, 0.02, 0.04) * (1.2 - 0.4 * length(uv));

    if (hit) {
        float3 p = ro + rd * dist;
        float3 n = calcNormal(p, t);
        float3 refl = reflect(rd, n);

        // 3 orbiting neon studio area lights reflecting off the glossy black ferrofluid
        float3 l1 = normalize(float3(cos(t * 0.8), sin(t * 0.6), -0.8));
        float3 l2 = normalize(float3(-cos(t * 0.5), 0.8, sin(t * 0.7)));
        float3 l3 = normalize(float3(0.0, -1.0, -0.5));

        float s1 = pow(max(dot(refl, l1), 0.0), 28.0);
        float s2 = pow(max(dot(refl, l2), 0.0), 28.0);
        float s3 = pow(max(dot(refl, l3), 0.0), 16.0);
        float fresnel = pow(1.0 - max(dot(n, -rd), 0.0), 3.5);

        // Iridescent thin-film oil sheen on the ferrofluid
        float3 oilSheen = 0.5 + 0.5 * cos(float3(0.0, 2.1, 4.2) + dot(n, float3(1, 2, 3)) * 2.5 + t * 0.5);

        col = float3(0.02, 0.025, 0.035);
        col += float3(0.0, 0.92, 1.0) * s1 * 1.35;
        col += float3(1.0, 0.12, 0.65) * s2 * 1.35;
        col += float3(1.0, 0.75, 0.15) * s3 * 0.85;
        col += oilSheen * fresnel * 0.95;
    }

    col = 1.0 - exp(-col * 1.3);
    return float4(saturate(col), 1.0);
}
)HLSL";
        return InitFullscreenShader(device, kPS);
    }
};

// ============================================================================
// 4. Retro Subsurface Lava Lamp Metaballs
// ============================================================================
class LavaLampMetaballsEffect : public FullscreenShaderEffect {
public:
    const char* GetName() const override {
        return "Subsurface Lava Lamp";
    }

    const char* GetDescription() const override {
        return "3D raymarched buoyancy-driven wax metaballs rising, splitting, and coalescing with warm subsurface scattering.";
    }

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override {
        static const char* kPS = R"HLSL(
cbuffer StandardConstants : register(b0) {
    float2 uResolution; float uTime; float uDeltaTime;
    float uAspect; float uSpeed; float uParam1; float uParam2;
};
struct PSIn { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };

float smin(float a, float b, float k) {
    float h = saturate(0.5 + 0.5 * (b - a) / k);
    return lerp(b, a, h) - k * h * (1.0 - h);
}

float mapWax(float3 p, float t) {
    // Bottom heating pool & top cooling pool
    float d = p.y + 2.35 + 0.12 * sin(p.x * 1.5 + t);
    d = smin(d, 2.55 - p.y + 0.12 * cos(p.z * 1.5 - t), 0.65);

    // 7 buoyant wax blobs rising and sinking at different harmonic frequencies
    [unroll]
    for (int i = 0; i < 7; ++i) {
        float fi = float(i);
        float y = sin(t * (0.32 + fi * 0.07) + fi * 1.7) * 1.85;
        float x = sin(t * (0.21 + fi * 0.05) + fi * 3.1) * 1.35;
        float z = cos(t * (0.25 + fi * 0.04) + fi * 2.3) * 0.85;
        float radius = 0.48 + 0.18 * sin(fi * 2.5 + t * 0.4);
        d = smin(d, length(p - float3(x, y, z)) - radius, 0.72);
    }
    return d;
}

float3 calcNormal(float3 p, float t) {
    float2 e = float2(0.004, -0.004);
    return normalize(
        e.xyy * mapWax(p + e.xyy, t) +
        e.yyx * mapWax(p + e.yyx, t) +
        e.yxy * mapWax(p + e.yxy, t) +
        e.xxx * mapWax(p + e.xxx, t));
}

float4 PSMain(PSIn input) : SV_TARGET {
    float2 uv = (input.uv * 2.0 - 1.0) * float2(uAspect, -1.0);
    float t = uTime * 1.0;

    float3 ro = float3(0.0, 0.0, -4.6);
    float3 rd = normalize(float3(uv, 1.5));

    // Slowly shifting color theme (Magenta/Orange -> Violet/Cyan -> Gold/Crimson)
    float hueShift = t * 0.06;
    float3 liquidTop = 0.25 + 0.2 * cos(float3(0.0, 2.0, 4.0) + hueShift + 2.0);
    float3 liquidBot = 0.45 + 0.35 * cos(float3(0.0, 1.8, 3.8) + hueShift);
    float3 bg = lerp(liquidBot, liquidTop, saturate(input.uv.y)) * (1.1 - 0.35 * length(uv));

    float dist = 0.0;
    bool hit = false;
    float glow = 0.0;

    [loop]
    for (int i = 0; i < 64; ++i) {
        float3 p = ro + rd * dist;
        float d = mapWax(p, t);
        glow += 0.015 / (0.08 + max(d, 0.0) * max(d, 0.0) * 6.0);
        if (d < 0.003) { hit = true; break; }
        dist += d * 0.75;
        if (dist > 9.0) break;
    }

    float3 col = bg;
    if (hit) {
        float3 p = ro + rd * dist;
        float3 n = calcNormal(p, t);

        // Subsurface scattering approximation: thickness along view + bottom heat lamp
        float thickness = saturate(mapWax(p + rd * 0.45, t) * 1.5 + 0.5);
        float bottomHeat = saturate((-p.y + 2.2) / 4.2);

        float3 waxCore = 0.65 + 0.35 * cos(float3(0.0, 1.2, 2.8) + hueShift);
        float3 waxHot  = float3(1.0, 0.92, 0.45);

        float fresnel = pow(1.0 - max(dot(n, -rd), 0.0), 2.5);
        float spec = pow(max(dot(reflect(rd, n), normalize(float3(0.4, -0.8, -0.5))), 0.0), 20.0);

        float3 waxCol = lerp(waxCore, waxHot, bottomHeat * 0.7 + thickness * 0.4);
        waxCol += waxHot * fresnel * 0.55 + float3(1, 1, 1) * spec * 0.45;
        col = waxCol;
    }

    // Add warm internal lamp halo
    col += (0.6 + 0.4 * cos(float3(0.0, 1.5, 3.2) + hueShift)) * glow * 0.09;
    col = 1.0 - exp(-col * 1.2);
    return float4(saturate(col), 1.0);
}
)HLSL";
        return InitFullscreenShader(device, kPS);
    }
};

} // namespace ScreensaverEngine
