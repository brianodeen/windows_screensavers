#pragma once

#include "../Engine/ShaderHelper.h"

namespace ScreensaverEngine {

// ============================================================================
// 21. High-Voltage Tesla Plasma Globe
// ============================================================================
class PlasmaGlobeTeslaEffect : public FullscreenShaderEffect {
public:
    const char* GetName() const override {
        return "High-Voltage Tesla Plasma Globe";
    }

    const char* GetDescription() const override {
        return "3D glass plasma sphere with branching volumetric electric filaments arcing from a central high-voltage electrode.";
    }

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override {
        static const char* kPS = R"HLSL(
cbuffer StandardConstants : register(b0) {
    float2 uResolution; float uTime; float uDeltaTime;
    float uAspect; float uSpeed; float uParam1; float uParam2;
};
struct PSIn { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };

float hash31(float3 p) {
    p = frac(p * float3(443.897, 441.423, 437.195));
    p += dot(p, p.yzx + 19.19);
    return frac((p.x + p.y) * p.z);
}

float noise3D(float3 p) {
    float3 i = floor(p), f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    return lerp(
        lerp(lerp(hash31(i), hash31(i + float3(1,0,0)), f.x),
             lerp(hash31(i + float3(0,1,0)), hash31(i + float3(1,1,0)), f.x), f.y),
        lerp(lerp(hash31(i + float3(0,0,1)), hash31(i + float3(1,0,1)), f.x),
             lerp(hash31(i + float3(0,1,1)), hash31(i + float3(1,1,1)), f.x), f.y), f.z);
}

float4 PSMain(PSIn input) : SV_TARGET {
    float2 uv = (input.uv * 2.0 - 1.0) * float2(uAspect, -1.0);
    float t = uTime * 1.25;

    float3 ro = float3(0.0, 0.0, -2.8);
    float3 rd = normalize(float3(uv, 1.55));

    // Ray-sphere intersection with outer glass globe (radius 1.25)
    float b = dot(ro, rd);
    float c = dot(ro, ro) - 1.25 * 1.25;
    float disc = b * b - c;

    float3 col = float3(0.01, 0.012, 0.028) * (1.2 - 0.35 * length(uv));

    if (disc > 0.0) {
        float tNear = -b - sqrt(disc);
        float tFar  = -b + sqrt(disc);

        // Glass rim Fresnel glow
        float3 hitNormal = normalize(ro + rd * tNear);
        float fresnel = pow(1.0 - max(dot(hitNormal, -rd), 0.0), 3.2);
        col += float3(0.18, 0.45, 0.95) * fresnel * 0.65;

        // Volumetric integration inside the plasma sphere
        const int steps = 28;
        float stepLen = (tFar - tNear) / float(steps);
        float3 accum = float3(0, 0, 0);

        for (int i = 0; i < steps; ++i) {
            float3 p = ro + rd * (tNear + (float(i) + 0.5) * stepLen);
            float r = length(p);

            // Central hot electrode sphere
            float core = exp(-r * 9.0);
            accum += float3(1.0, 0.45, 0.85) * core * 0.25;

            // 12 branching lightning tendrils rotating in 3D
            float3 dir = normalize(p + 1e-5);
            float3 warpedDir = dir + 0.28 * float3(
                noise3D(p * 3.5 + float3(t * 2.2, 0, 0)) - 0.5,
                noise3D(p * 3.5 + float3(0, t * 2.2, 0)) - 0.5,
                noise3D(p * 3.5 + float3(0, 0, t * 2.2)) - 0.5
            );
            warpedDir = normalize(warpedDir);

            // Angular filament pattern using spherical harmonics
            float fil = abs(sin(warpedDir.x * 7.0 + t) * cos(warpedDir.y * 7.0 - t * 0.8) + sin(warpedDir.z * 7.0 + t * 0.6));
            float bolt = exp(-fil * 18.0) * smoothstep(0.12, 0.35, r) * smoothstep(1.25, 0.95, r);

            // Pink/magenta attachment pads where bolts touch the inner glass wall
            float pad = exp(-fil * 8.0) * exp(-abs(r - 1.18) * 28.0);

            accum += float3(0.45, 0.65, 1.0) * bolt * 0.55;
            accum += float3(1.0, 0.25, 0.75) * pad * 1.15;
        }
        col += accum;
    }

    col = 1.0 - exp(-col * 1.3);
    return float4(saturate(col), 1.0);
}
)HLSL";
        return InitFullscreenShader(device, kPS);
    }
};

// ============================================================================
// 22. Neon Mystify Polygons (Classic Windows Screensaver Tribute)
// ============================================================================
class MystifyGeometricRibbonsEffect : public FullscreenShaderEffect {
public:
    const char* GetName() const override {
        return "Neon Mystify Polygons (Classic Tribute)";
    }

    const char* GetDescription() const override {
        return "Bouncing multi-vertex wireframe polygons with glowing neon echo trails, inspired by classic Windows Mystify.";
    }

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override {
        static const char* kPS = R"HLSL(
cbuffer StandardConstants : register(b0) {
    float2 uResolution; float uTime; float uDeltaTime;
    float uAspect; float uSpeed; float uParam1; float uParam2;
};
struct PSIn { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };

// Triangle wave bounce inside [-bounds, +bounds]
float2 bouncePos(float t, float2 vel, float2 phase, float2 bounds) {
    float2 x = frac((t * vel + phase) * 0.5);
    float2 tri = 1.0 - abs(x * 2.0 - 1.0);
    return (tri * 2.0 - 1.0) * bounds;
}

float distToSegment(float2 p, float2 a, float2 b) {
    float2 pa = p - a, ba = b - a;
    float h = saturate(dot(pa, ba) / max(dot(ba, ba), 1e-6));
    return length(pa - ba * h);
}

float4 PSMain(PSIn input) : SV_TARGET {
    float2 uv = (input.uv * 2.0 - 1.0) * float2(uAspect, -1.0);
    float2 bounds = float2(uAspect * 0.92, 0.92);

    float3 col = float3(0.008, 0.01, 0.022);

    // 3 independent bouncing 4-vertex polygons, each leaving 14 historical echo trails
    for (int poly = 0; poly < 3; ++poly) {
        float fp = float(poly);
        float3 baseCol = 0.55 + 0.45 * cos(float3(0.0, 2.1, 4.2) + uTime * 0.45 + fp * 2.1);

        [loop]
        for (int echo = 0; echo < 14; ++echo) {
            float fe = float(echo);
            float tHistory = uTime * 0.55 - fe * 0.032;
            float fade = pow(1.0 - fe / 14.0, 1.8);

            float2 v0 = bouncePos(tHistory, float2(0.41, 0.53) + fp * 0.07, float2(0.1, 0.4) + fp * 1.1, bounds);
            float2 v1 = bouncePos(tHistory, float2(0.57, 0.37) + fp * 0.05, float2(0.6, 0.2) + fp * 1.7, bounds);
            float2 v2 = bouncePos(tHistory, float2(0.33, 0.61) + fp * 0.09, float2(0.3, 0.8) + fp * 2.3, bounds);
            float2 v3 = bouncePos(tHistory, float2(0.49, 0.43) + fp * 0.06, float2(0.8, 0.5) + fp * 2.9, bounds);

            float d = min(
                min(distToSegment(uv, v0, v1), distToSegment(uv, v1, v2)),
                min(distToSegment(uv, v2, v3), distToSegment(uv, v3, v0))
            );

            float lineCore = smoothstep(0.006, 0.001, d);
            float lineGlow = 0.0012 / (d * d + 0.00025);

            col += baseCol * (lineCore * 0.65 + lineGlow * 0.08) * fade;
        }
    }

    col = 1.0 - exp(-col * 1.2);
    return float4(saturate(col), 1.0);
}
)HLSL";
        return InitFullscreenShader(device, kPS);
    }
};

// ============================================================================
// 23. Retro-Futuristic 3D Spectrum Analyzer Equalizer
// ============================================================================
class AudioSpectrumEqualizerEffect : public FullscreenShaderEffect {
public:
    const char* GetName() const override {
        return "3D Synth Spectrum Equalizer";
    }

    const char* GetDescription() const override {
        return "Led-segmented 3D spectrum analyzer equalizer bars with floating peak-hold caps and glossy floor reflections.";
    }

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override {
        static const char* kPS = R"HLSL(
cbuffer StandardConstants : register(b0) {
    float2 uResolution; float uTime; float uDeltaTime;
    float uAspect; float uSpeed; float uParam1; float uParam2;
};
struct PSIn { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };

float barEnvelope(float bandIndex, float t) {
    // Procedural multi-band harmonic synth beat + bass kick + treble shimmer
    float normBand = bandIndex / 32.0;
    float bassKick = pow(0.5 + 0.5 * sin(t * 6.28318 * 1.15), 4.0) * exp(-normBand * 3.5);
    float midWave  = (0.5 + 0.5 * sin(normBand * 12.0 - t * 4.2)) * (0.5 + 0.5 * cos(t * 2.7 + bandIndex * 0.4));
    float highArp  = (0.5 + 0.5 * sin(bandIndex * 1.9 + t * 9.5)) * smoothstep(0.2, 0.9, normBand);
    return clamp(0.08 + bassKick * 0.55 + midWave * 0.48 + highArp * 0.32, 0.06, 0.95);
}

float4 PSMain(PSIn input) : SV_TARGET {
    float2 uv = (input.uv * 2.0 - 1.0) * float2(uAspect, -1.0);
    float t = uTime * 1.0;

    float3 col = float3(0.01, 0.015, 0.03) * (1.2 - 0.3 * length(uv));

    // Mirror floor plane at y = -0.35
    float horizonY = -0.32;
    bool isReflection = (uv.y < horizonY);
    float2 eqUV = uv;
    if (isReflection) {
        eqUV.y = horizonY + (horizonY - uv.y) * 1.35;
        // Subtle water/floor ripple on reflection
        eqUV.x += sin(uv.y * 55.0 + t * 4.0) * 0.008;
    }

    // 32 frequency bands across [-1.45, +1.45]
    float spanX = 1.48;
    if (abs(eqUV.x) < spanX && eqUV.y >= horizonY && eqUV.y < 0.85) {
        float normX = (eqUV.x + spanX) / (2.0 * spanX);
        float bandFloat = normX * 32.0;
        float bandID = floor(bandFloat);
        float bandLocalX = frac(bandFloat) - 0.5;

        float barH = barEnvelope(bandID, t);
        // Peak hold floats slightly above the current bar
        float peakH = max(barH, barEnvelope(bandID, t - 0.14) + 0.06);

        float relY = (eqUV.y - horizonY) / 1.12; // [0..1]
        float segFloat = relY * 28.0;
        float segID = floor(segFloat);
        float segLocalY = frac(segFloat) - 0.5;

        // Individual LED brick mask
        float ledMask = smoothstep(0.42, 0.32, abs(bandLocalX)) * smoothstep(0.38, 0.25, abs(segLocalY));

        // Color gradient from bottom to top: Cyan -> Emerald -> Yellow -> Crimson
        float segNorm = segID / 28.0;
        float3 ledCol = lerp(float3(0.0, 0.9, 1.0), float3(0.1, 0.95, 0.35), smoothstep(0.0, 0.45, segNorm));
        ledCol = lerp(ledCol, float3(1.0, 0.85, 0.1), smoothstep(0.45, 0.75, segNorm));
        ledCol = lerp(ledCol, float3(1.0, 0.12, 0.35), smoothstep(0.75, 0.95, segNorm));

        float peakSeg = floor(peakH * 28.0);
        bool isLit = (segNorm <= barH);
        bool isPeak = (abs(segID - peakSeg) < 0.5);

        float3 brick = ledCol * 0.07; // Unlit dim LED socket
        if (isLit) {
            brick = ledCol * 1.25;
        } else if (isPeak) {
            brick = float3(0.95, 0.98, 1.0) * 1.35;
        }

        floatatten = isReflection ? (0.28 * exp(-(horizonY - uv.y) * 3.5)) : 1.0;
        col += brick * ledMask * atten;
    }

    // Glowing horizon divider line
    col += float3(0.2, 0.8, 1.0) * exp(-abs(uv.y - horizonY) * 90.0) * 0.65;
    return float4(saturate(col), 1.0);
}
)HLSL";
        return InitFullscreenShader(device, kPS);
    }
};

// ============================================================================
// 24. Amethyst Crystal Geode Cavern
// ============================================================================
class CrystalGeodeCavernEffect : public FullscreenShaderEffect {
public:
    const char* GetName() const override {
        return "Amethyst Crystal Geode Cavern";
    }

    const char* GetDescription() const override {
        return "Raymarched subterranean crystal cavern lined with faceted specular amethyst and quartz prisms.";
    }

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override {
        static const char* kPS = R"HLSL(
cbuffer StandardConstants : register(b0) {
    float2 uResolution; float uTime; float uDeltaTime;
    float uAspect; float uSpeed; float uParam1; float uParam2;
};
struct PSIn { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };

float2x2 rot2D(float a) {
    float c = cos(a), s = sin(a);
    return float2x2(c, -s, s, c);
}

// Hexagonal prism SDF for faceted quartz/amethyst crystals
float sdHexPrism(float3 p, float2 h) {
    const float3 k = float3(-0.8660254, 0.5, 0.57735);
    p = abs(p);
    p.xy -= 2.0 * min(dot(k.xy, p.xy), 0.0) * k.xy;
    float2 d = float2(
        length(p.xy - float2(clamp(p.x, -k.z * h.x, k.z * h.x), h.x)) * sign(p.y - h.x),
        p.z - h.y);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

float mapCavern(float3 p) {
    // Winding tunnel radius
    float2 tunnelCenter = float2(sin(p.z * 0.35) * 0.65, cos(p.z * 0.25) * 0.45);
    float2 radial = p.xy - tunnelCenter;
    float tunnelR = length(radial);

    // Repeat crystal clusters around tunnel wall
    float angle = atan2(radial.y, radial.x);
    float sector = 6.2831853 / 10.0;
    float aLocal = fmod(angle + 6.2831853, sector) - sector * 0.5;
    float zLocal = fmod(abs(p.z), 0.85) - 0.425;

    float3 q = float3(aLocal * 1.8, zLocal, 2.15 - tunnelR);
    q.xz = mul(rot2D(0.35), q.xz);

    // Pointed hexagonal crystal termination
    float prism = sdHexPrism(q.xzy, float2(0.24, 0.65));
    float wall = 2.25 - tunnelR;
    return min(wall, prism * 0.65);
}

float3 calcNormal(float3 p) {
    float2 e = float2(0.003, -0.003);
    return normalize(
        e.xyy * mapCavern(p + e.xyy) +
        e.yyx * mapCavern(p + e.yyx) +
        e.yxy * mapCavern(p + e.yxy) +
        e.xxx * mapCavern(p + e.xxx));
}

float4 PSMain(PSIn input) : SV_TARGET {
    float2 uv = (input.uv * 2.0 - 1.0) * float2(uAspect, -1.0);
    float t = uTime * 1.1;

    float z = t * 1.4;
    float3 ro = float3(sin(z * 0.35) * 0.65, cos(z * 0.25) * 0.45, z);
    float3 rd = normalize(float3(uv, 1.45));
    rd.xy = mul(rot2D(t * 0.15), rd.xy);

    float dist = 0.0;
    float glow = 0.0;
    bool hit = false;

    [loop]
    for (int i = 0; i < 68; ++i) {
        float3 p = ro + rd * dist;
        float d = mapCavern(p);
        glow += 0.012 / (0.02 + d * d * 15.0);
        if (d < 0.003) { hit = true; break; }
        dist += d;
        if (dist > 18.0) break;
    }

    float3 col = float3(0.04, 0.01, 0.08);
    if (hit) {
        float3 p = ro + rd * dist;
        float3 n = calcNormal(p);

        // Orbiting fairy light inside the geode tunnel
        float3 lightPos = ro + float3(sin(t * 1.5) * 0.6, cos(t * 1.2) * 0.6, 2.5);
        float3 lDir = normalize(lightPos - p);

        float diff = max(dot(n, lDir), 0.0);
        float spec = pow(max(dot(reflect(-lDir, n), -rd), 0.0), 32.0);
        float fresnel = pow(1.0 - max(dot(n, -rd), 0.0), 3.0);

        float3 amethyst = lerp(float3(0.45, 0.10, 0.85), float3(0.95, 0.35, 0.90), fresnel);
        col = amethyst * (0.22 + 0.85 * diff) + float3(0.9, 0.8, 1.0) * spec * 1.1;
        col = lerp(float3(0.05, 0.01, 0.10), col, exp(-dist * 0.14));
    }

    col += float3(0.75, 0.25, 1.0) * glow * 0.08;
    col = 1.0 - exp(-col * 1.25);
    return float4(saturate(col), 1.0);
}
)HLSL";
        return InitFullscreenShader(device, kPS);
    }
};

// ============================================================================
// 25. Solar Surface Granulation & Coronal Plasma Loops
// ============================================================================
class SolarFlareCoronalLoopEffect : public FullscreenShaderEffect {
public:
    const char* GetName() const override {
        return "Solar Corona & Magnetic Flare Loops";
    }

    const char* GetDescription() const override {
        return "Boiling solar photosphere granulation cells with erupting magnetic coronal plasma loops and solar wind.";
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
    for (int i = 0; i < 5; ++i) {
        v += a * noise(p);
        p = mul(float2x2(0.8, -0.6, 0.6, 0.8), p * 2.02);
        a *= 0.5;
    }
    return v;
}

float4 PSMain(PSIn input) : SV_TARGET {
    float2 uv = (input.uv * 2.0 - 1.0) * float2(uAspect, -1.0);
    float t = uTime * 0.65;

    // Position the limb of the star across the lower/left portion of the screen
    float2 sunCenter = float2(-0.45, -0.55);
    float2 dVec = uv - sunCenter;
    float r = length(dVec);
    float sunRadius = 1.18;

    float3 col = float3(0.01, 0.005, 0.015);

    if (r < sunRadius) {
        // 3D sphere normal on the solar photosphere
        float z = sqrt(max(sunRadius * sunRadius - r * r, 0.0));
        float3 n = normalize(float3(dVec, z));

        // Boiling convective plasma granulation
        float2 sphereUV = float2(atan2(n.x, n.z) * 4.0 - t * 0.25, asin(n.y) * 4.0);
        float warp = fbm(sphereUV * 2.5 + t * 0.4);
        float granules = fbm(sphereUV * 6.0 + warp * 2.2 - t * 0.6);

        // Limb darkening near the edge of the solar disk
        float limb = pow(saturate(z / sunRadius), 0.45);
        float temp = granules * limb * 1.45;

        float3 darkSpot = float3(0.45, 0.06, 0.01);
        float3 orange   = float3(1.00, 0.42, 0.04);
        float3 whiteHot = float3(1.00, 0.96, 0.75);

        col = lerp(darkSpot, orange, smoothstep(0.1, 0.55, temp));
        col = lerp(col, whiteHot, smoothstep(0.55, 1.05, temp));
        // Bright chromosphere rim
        col += float3(1.0, 0.35, 0.05) * pow(1.0 - limb, 3.0) * 0.85;
    } else {
        // Solar Corona & Erupting Magnetic Prominence Loops outside the disk
        float distFromLimb = r - sunRadius;
        float angle = atan2(dVec.y, dVec.x);

        // Radial coronal streamers
        float streamers = fbm(float2(angle * 7.0, distFromLimb * 2.0 - t * 0.8));
        float corona = exp(-distFromLimb * 3.2) * (0.55 + 0.65 * streamers);

        // Arching magnetic plasma loops along the solar limb
        float loopPhase = sin(angle * 9.0 + sin(distFromLimb * 8.0 - t * 2.2) * 1.5);
        float loopArch  = exp(-abs(distFromLimb - 0.16 * (0.5 + 0.5 * sin(angle * 5.0 + t))) * 18.0);
        float prominence = pow(saturate(loopPhase), 4.0) * loopArch;

        col += float3(1.0, 0.38, 0.06) * corona * 1.15;
        col += float3(1.0, 0.82, 0.35) * prominence * 1.45;
    }

    col = 1.0 - exp(-col * 1.2);
    return float4(saturate(col), 1.0);
}
)HLSL";
        return InitFullscreenShader(device, kPS);
    }
};

} // namespace ScreensaverEngine
