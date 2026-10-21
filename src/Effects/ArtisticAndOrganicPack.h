#pragma once

#include "../Engine/ShaderHelper.h"

namespace ScreensaverEngine {

// ============================================================================
// 1. Kaleidoscopic Stained Glass Rose Window
// ============================================================================
class StainedGlassVoronoiEffect : public FullscreenShaderEffect {
public:
    const char* GetName() const override {
        return "Stained Glass Cathedral Rose";
    }

    const char* GetDescription() const override {
        return "Kaleidoscopic stained-glass rose window with Voronoi crystal shards, leaded solder borders, and cathedral sunbeams.";
    }

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override {
        static const char* kPS = R"HLSL(
cbuffer StandardConstants : register(b0) {
    float2 uResolution; float uTime; float uDeltaTime;
    float uAspect; float uSpeed; float uParam1; float uParam2;
};
struct PSIn { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };

float2 hash22(float2 p) {
    float3 p3 = frac(float3(p.xyx) * float3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.xx + p3.yz) * p3.zy);
}

// Returns (minDist, edgeDist, cellHash)
float3 voronoiGlass(float2 x, float t) {
    float2 n = floor(x);
    float2 f = frac(x);

    float2 mg = float2(0, 0), mr = float2(0, 0);
    float md = 8.0;
    float2 bestID = float2(0, 0);

    [unroll]
    for (int j = -1; j <= 1; ++j) {
        [unroll]
        for (int i = -1; i <= 1; ++i) {
            float2 g = float2(float(i), float(j));
            float2 o = hash22(n + g);
            o = 0.5 + 0.42 * sin(t * 0.65 + 6.2831 * o);
            float2 r = g + o - f;
            float d = dot(r, r);
            if (d < md) {
                md = d;
                mr = r;
                mg = g;
                bestID = n + g;
            }
        }
    }

    float edgeDist = 8.0;
    [unroll]
    for (int j2 = -1; j2 <= 1; ++j2) {
        [unroll]
        for (int i2 = -1; i2 <= 1; ++i2) {
            float2 g = float2(float(i2), float(j2));
            if (dot(mg - g, mg - g) > 0.001) {
                float2 o = hash22(n + g);
                o = 0.5 + 0.42 * sin(t * 0.65 + 6.2831 * o);
                float2 r = g + o - f;
                edgeDist = min(edgeDist, dot(0.5 * (mr + r), normalize(r - mr)));
            }
        }
    }

    float cellSeed = frac(bestID.x * 0.173 + bestID.y * 0.319);
    return float3(sqrt(md), edgeDist, cellSeed);
}

float4 PSMain(PSIn input) : SV_TARGET {
    float2 uv = (input.uv * 2.0 - 1.0) * float2(uAspect, -1.0);
    float t = uTime * 0.85;

    // 12-fold Gothic Rose Window Kaleidoscope symmetry
    float r = length(uv);
    float angle = atan2(uv.y, uv.x) + t * 0.08;
    float sector = 6.2831853 / 12.0;
    angle = abs(fmod(angle + 6.2831853, sector) - sector * 0.5);

    float2 symUV = float2(cos(angle), sin(angle)) * r * 4.5;
    float3 v = voronoiGlass(symUV, t);

    // Rich stained glass jewel tones (Ruby, Sapphire, Emerald, Amber, Amethyst)
    float hue = v.z * 6.28318 + r * 1.4 - t * 0.25;
    float3 glassCol = 0.55 + 0.45 * cos(hue + float3(0.0, 2.09, 4.18));
    glassCol = pow(glassCol, 1.35) * 1.4;

    // Hammered cathedral glass internal ripple texture
    float ripple = 0.82 + 0.18 * sin(v.x * 24.0 + symUV.x * 8.0);
    float bevel  = smoothstep(0.0, 0.18, v.y);

    // Concentric Gothic iron tracery rings
    float ringDist = min(abs(r - 0.45), min(abs(r - 0.85), abs(r - 1.25)));
    float leadMask = smoothstep(0.032, 0.055, v.y) * smoothstep(0.012, 0.024, ringDist);

    // Backlit cathedral sunlight streaming through the stained glass
    float2 sunPos = float2(sin(t * 0.4) * 0.35, cos(t * 0.3) * 0.25);
    float sunGlow = exp(-length(uv - sunPos) * 1.4) * 1.35 + 0.45;

    float3 leadCol = float3(0.05, 0.05, 0.07) + float3(0.18, 0.15, 0.12) * (1.0 - bevel);
    float3 col = lerp(leadCol, glassCol * ripple * bevel * sunGlow, leadMask);

    return float4(saturate(col), 1.0);
}
)HLSL";
        return InitFullscreenShader(device, kPS);
    }
};

// ============================================================================
// 2. Abyssal Bioluminescent Jellyfish
// ============================================================================
class BioluminescentJellyfishEffect : public FullscreenShaderEffect {
public:
    const char* GetName() const override {
        return "Abyssal Bioluminescent Jellyfish";
    }

    const char* GetDescription() const override {
        return "3D raymarched translucent jellyfish pulsing with neon bioluminescence in deep ocean currents.";
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

// Returns (distance, partID: 0 = bell, 1 = tentacles/core)
float2 mapJellyfish(float3 p, float t) {
    // Gentle swimming propulsion pulse
    float swimCycle = t * 2.4;
    float pulse = sin(swimCycle - p.y * 1.2) * 0.18;

    // Gentle body sway
    p.x += sin(t * 0.7 + p.y * 0.5) * 0.25;
    p.y -= sin(swimCycle - 0.8) * 0.18 - 0.35;

    // Umbrella bell (hollowed hemisphere with scalloped rim)
    float angle = atan2(p.z, p.x);
    float bellRadius = 1.05 + pulse * smoothstep(0.6, -0.5, p.y) + 0.04 * sin(angle * 16.0);
    float3 bellP = float3(p.x / bellRadius, (p.y - 0.45) * 1.15, p.z / bellRadius);

    float outerSphere = length(bellP) - 1.0;
    float innerHollow = length(bellP - float3(0, -0.22, 0)) - 0.92;
    float cutBottom   = -p.y - 0.08 + 0.03 * cos(angle * 16.0);
    float dBell = max(max(outerSphere, -innerHollow), cutBottom);

    // 8 flowing sinusoidal tentacles trailing beneath the bell
    float dTentacles = 10.0;
    if (p.y < 0.45 && length(p.xz) < 1.35) {
        [unroll]
        for (int i = 0; i < 8; ++i) {
            float a = float(i) * 0.785398;
            float2 baseOffset = float2(cos(a), sin(a)) * 0.42;
            float2 wave = float2(
                sin(p.y * 3.2 - t * 3.0 + float(i)),
                cos(p.y * 2.7 - t * 2.6 + float(i) * 1.7)
            ) * 0.16 * saturate(0.4 - p.y);

            float2 tentPos = baseOffset + wave;
            float thickness = 0.035 * saturate((p.y + 2.4) / 2.8);
            float dt = max(length(p.xz - tentPos) - thickness, max(p.y - 0.35, -2.35 - p.y));
            dTentacles = min(dTentacles, dt);
        }
    }

    if (dBell < dTentacles) return float2(dBell, 0.0);
    return float2(dTentacles, 1.0);
}

float4 PSMain(PSIn input) : SV_TARGET {
    float2 uv = (input.uv * 2.0 - 1.0) * float2(uAspect, -1.0);
    float t = uTime * 0.95;

    float3 ro = float3(0.0, -0.35, -3.9);
    float3 rd = normalize(float3(uv, 1.55));

    // Abyssal ocean backdrop
    float3 col = lerp(float3(0.004, 0.012, 0.035), float3(0.01, 0.045, 0.11), saturate(input.uv.y));

    // Volumetric raymarching (accumulates translucent bioluminescence along the ray!)
    float dist = 1.6;
    [loop]
    for (int i = 0; i < 64; ++i) {
        float3 p = ro + rd * dist;
        float2 res = mapJellyfish(p, t);
        float d = res.x;

        // Accumulate translucent glowing tissue when near or inside the surface
        float density = exp(-max(d, 0.0) * 26.0);
        if (density > 0.005) {
            float angle = atan2(p.z, p.x);
            float ribs = 0.5 + 0.5 * cos(angle * 16.0);
            float bioPulse = 0.5 + 0.5 * sin(p.y * 5.0 - t * 4.5);

            float3 bellCol = lerp(float3(0.05, 0.75, 1.0), float3(0.95, 0.15, 0.82), ribs * 0.65 + bioPulse * 0.35);
            float3 tentCol = lerp(float3(1.0, 0.35, 0.75), float3(0.2, 1.0, 0.85), saturate(-p.y * 0.45));

            float3 emit = (res.y < 0.5) ? bellCol : tentCol;
            col += emit * density * 0.055;
        }

        dist += max(abs(d) * 0.48, 0.032);
        if (dist > 6.2) break;
    }

    // Floating bioluminescent marine snow plankton
    for (float k = 1.0; k <= 3.0; k += 1.0) {
        float2 snowUV = uv * (4.0 + k * 2.5) + float2(sin(t * 0.2 * k), -t * 0.35 * k);
        float2 id = floor(snowUV);
        float2 f = frac(snowUV) - 0.5;
        float h = frac(sin(dot(id, float2(12.9898, 78.233))) * 43758.5453);
        if (h > 0.88) {
            float spark = smoothstep(0.16, 0.0, length(f)) * (0.5 + 0.5 * sin(t * 3.0 + h * 40.0));
            col += float3(0.2, 0.9, 1.0) * spark * 0.35;
        }
    }

    col = 1.0 - exp(-col * 1.25);
    return float4(saturate(col), 1.0);
}
)HLSL";
        return InitFullscreenShader(device, kPS);
    }
};

// ============================================================================
// 3. Celestial Brass Clockwork & Astrolabe
// ============================================================================
class SteampunkClockworkEffect : public FullscreenShaderEffect {
public:
    const char* GetName() const override {
        return "Celestial Brass Clockwork";
    }

    const char* GetDescription() const override {
        return "Interlocking counter-rotating brass, copper, and silver clockwork gears with anisotropic brushed metal and glowing runes.";
    }

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override {
        static const char* kPS = R"HLSL(
cbuffer StandardConstants : register(b0) {
    float2 uResolution; float uTime; float uDeltaTime;
    float uAspect; float uSpeed; float uParam1; float uParam2;
};
struct PSIn { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };

// Evaluates a procedural toothed clockwork gear with spokes and inner rim
float gearMask(float2 p, float radius, float teethCount, float toothDepth, float angleOffset, out float bevel) {
    float r = length(p);
    float a = atan2(p.y, p.x) + angleOffset;

    // Gear teeth profile
    float teeth = smoothstep(-0.35, 0.35, cos(a * teethCount)) * toothDepth;
    float outerR = radius + teeth;

    // Spokes & hollow cutouts inside the gear
    float spokeMask = smoothstep(-0.2, 0.2, abs(sin(a * 3.0)) - 0.25);
    bool inHollow = (r > radius * 0.28 && r < radius * 0.76) && (spokeMask > 0.5);

    float dOuter = outerR - r;
    float dAxle  = r - radius * 0.11;
    float d = min(dOuter, dAxle);
    if (inHollow) {
        float dSpoke = min(r - radius * 0.28, radius * 0.76 - r);
        d = -dSpoke;
    }

    bevel = saturate(d * 25.0);
    return smoothstep(0.0, 0.008, d);
}

float4 PSMain(PSIn input) : SV_TARGET {
    float2 uv = (input.uv * 2.0 - 1.0) * float2(uAspect, -1.0);
    float t = uTime * 0.75;

    // Dark Victorian watchmaker velvet & faint engraved star chart background
    float3 col = float3(0.03, 0.035, 0.055) * (1.2 - 0.35 * length(uv));

    // Astrolabe celestial degree rings
    float rCenter = length(uv);
    float ringTick = smoothstep(0.008, 0.0, abs(rCenter - 0.88)) + smoothstep(0.005, 0.0, abs(rCenter - 0.92));
    col += float3(0.45, 0.35, 0.18) * ringTick * 0.55;

    // 7 interlocking gears with synchronized tooth ratios and alternating rotation directions
    struct GearDef {
        float2 pos;
        float radius;
        float teeth;
        float dir;
        float3 metalColor;
    };

    GearDef gears[6] = {
        { float2( 0.00,  0.00), 0.48, 24.0,  1.0, float3(0.92, 0.72, 0.32) }, // Master Brass Gear
        { float2( 0.77,  0.00), 0.26, 13.0, -1.0, float3(0.90, 0.52, 0.34) }, // Meshed Copper Gear
        { float2(-0.62,  0.52), 0.30, 15.0, -1.0, float3(0.82, 0.85, 0.90) }, // Meshed Silver Gear
        { float2(-0.55, -0.58), 0.28, 14.0, -1.0, float3(0.88, 0.68, 0.28) }, // Lower Gold Gear
        { float2( 0.52,  0.66), 0.32, 16.0,  1.0, float3(0.85, 0.62, 0.30) }, // Upper Right Bronze
        { float2( 0.55, -0.64), 0.24, 12.0,  1.0, float3(0.78, 0.82, 0.88) }  // Lower Right Steel
    };

    for (int i = 5; i >= 0; --i) {
        float2 gp = uv - gears[i].pos;
        float angularSpeed = (24.0 / gears[i].teeth) * gears[i].dir * t;
        float bevel = 0.0;
        float mask = gearMask(gp, gears[i].radius, gears[i].teeth, 0.045, angularSpeed, bevel);

        if (mask > 0.001) {
            float a = atan2(gp.y, gp.x);
            float r = length(gp);
            // Anisotropic brushed metal radial sheen + concentric lathe grooves
            float aniso = 0.65 + 0.35 * sin(a * 2.0 - t * 0.4 + float(i));
            float grooves = 0.92 + 0.08 * sin(r * 160.0);
            float3 gearCol = gears[i].metalColor * aniso * grooves * (0.55 + 0.55 * bevel);

            // Glowing arcane tick marks along the master gear
            if (i == 0 && abs(r - 0.39) < 0.015) {
                float runeGlow = smoothstep(0.4, 0.9, sin(a * 12.0 + angularSpeed * 12.0));
                gearCol += float3(0.2, 0.9, 1.0) * runeGlow * 0.85;
            }

            // Drop shadow onto lower layers
            col *= (1.0 - mask * 0.45);
            col = lerp(col, gearCol, mask);
        }
    }

    return float4(saturate(col), 1.0);
}
)HLSL";
        return InitFullscreenShader(device, kPS);
    }
};

// ============================================================================
// 4. Ukiyo-e Woodblock Ocean Waves & Crimson Moon
// ============================================================================
class JapaneseInkWaveEffect : public FullscreenShaderEffect {
public:
    const char* GetName() const override {
        return "Ukiyo-e Woodblock Ocean Waves";
    }

    const char* GetDescription() const override {
        return "Stylized Hokusai-inspired rolling woodblock ocean waves, curling foam crests, falling sakura petals, and a crimson moon.";
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

float4 PSMain(PSIn input) : SV_TARGET {
    float2 uv = (input.uv * 2.0 - 1.0) * float2(uAspect, -1.0);
    float t = uTime * 1.1;

    // Warm Washi parchment paper background with subtle woodblock grain
    float paperGrain = 0.96 + 0.04 * sin(uv.x * 120.0) * cos(uv.y * 90.0);
    float3 parchment = lerp(float3(0.95, 0.89, 0.76), float3(0.86, 0.76, 0.62), saturate(input.uv.y)) * paperGrain;

    // Giant Woodblock Crimson Sun / Moon
    float2 sunCenter = float2(0.38, 0.26);
    float sunDist = length(uv - sunCenter);
    float sunMask = smoothstep(0.46, 0.445, sunDist);
    float3 crimson = float3(0.82, 0.16, 0.14) * paperGrain;
    float3 col = lerp(parchment, crimson, sunMask);

    // Distant Mount Fuji silhouette on the horizon
    float fujiX = uv.x + 0.55;
    float fujiSlope = 0.08 - pow(abs(fujiX), 1.35) * 0.48;
    float fujiCap = max(fujiSlope, -0.32);
    if (uv.y < fujiSlope && abs(fujiX) < 0.85 && uv.y > -0.35) {
        bool snowLine = (uv.y > 0.01 + 0.02 * sin(fujiX * 25.0));
        col = snowLine ? float3(0.96, 0.95, 0.92) : float3(0.14, 0.22, 0.34);
    }

    // 6 parallax layers of rolling Prussian-blue Ukiyo-e waves from horizon to foreground
    for (int i = 0; i < 6; ++i) {
        float fi = float(i);
        float depth = fi / 5.0; // 0 = back, 1 = front
        float baseY = -0.12 - depth * 0.72;
        float speed = (0.8 + depth * 1.4) * (fmod(fi, 2.0) < 0.5 ? 1.0 : -0.85);
        float freq  = 2.6 - depth * 0.8;
        float amp   = 0.09 + depth * 0.09;

        // Trochoidal curling wave profile
        float xPhase = uv.x * freq + t * speed + fi * 1.7;
        float trochoid = sin(xPhase + 0.45 * cos(xPhase)) * amp
                       + sin(xPhase * 2.3 - t * 1.2) * (amp * 0.28);
        float waveY = baseY + trochoid;

        if (uv.y < waveY) {
            float distFromCrest = waveY - uv.y;

            // Prussian Blue woodblock gradient + white foam crest + dark ink contour
            float3 deepPrussian = float3(0.05, 0.14, 0.28);
            float3 midIndigo    = float3(0.14, 0.34, 0.52);
            float3 foamCream    = float3(0.96, 0.94, 0.88);
            float3 inkBlack     = float3(0.04, 0.06, 0.10);

            float3 bodyCol = lerp(midIndigo, deepPrussian, saturate(distFromCrest * 4.5));
            // Woodcut curved water stripes inside the wave body
            float stripe = smoothstep(0.4, 0.6, sin(distFromCrest * 75.0 + sin(uv.x * 8.0) * 2.0));
            bodyCol = lerp(bodyCol, midIndigo * 1.25, stripe * 0.35);

            // Curling white sea-foam Band at the top of each wave
            float foamMask = smoothstep(0.045, 0.0, distFromCrest - 0.012 * sin(uv.x * 38.0 + t * 3.0));
            float3 waveCol = lerp(bodyCol, foamCream, foamMask);

            // Crisp Sumi-e ink outline along the wave crest
            float inkEdge = smoothstep(0.0, 0.011, distFromCrest);
            col = lerp(inkBlack, waveCol * paperGrain, inkEdge);
        }
    }

    // Drifting sakura blossoms / sea spray in the wind
    float2 petalGrid = (uv + float2(t * 0.35, t * 0.18)) * 7.0;
    float2 pid = floor(petalGrid);
    float2 pf = frac(petalGrid) - 0.5;
    float ph = hash21(pid);
    if (ph > 0.85) {
        float2 offset = (float2(hash21(pid + 3.0), hash21(pid + 7.0)) - 0.5) * 0.5;
        float petal = smoothstep(0.09, 0.02, length(pf - offset));
        col = lerp(col, float3(0.98, 0.82, 0.84), petal * 0.85);
    }

    return float4(saturate(col), 1.0);
}
)HLSL";
        return InitFullscreenShader(device, kPS);
    }
};

// ============================================================================
// 5. Quantum Double Helix Molecular Lattice
// ============================================================================
class DNAHelixLatticeEffect : public FullscreenShaderEffect {
public:
    const char* GetName() const override {
        return "Quantum DNA Double Helix";
    }

    const char* GetDescription() const override {
        return "3D raymarched twisting DNA double-helix molecular strands with glowing base-pair rungs and depth-of-field bokeh.";
    }

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override {
        static const char* kPS = R"HLSL(
cbuffer StandardConstants : register(b0) {
    float2 uResolution; float uTime; float uDeltaTime;
    float uAspect; float uSpeed; float uParam1; float uParam2;
};
struct PSIn { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };

// Returns (distance, partID: 0 = phosphate backbone, 1 = nucleotide base-pair rung)
float2 mapDNA(float3 p, float t) {
    // Rotate & gently bend the helix axis
    p.x += sin(p.y * 0.35 + t * 0.6) * 0.25;
    float twist = p.y * 1.45 - t * 1.25;
    float c = cos(twist), s = sin(twist);
    float2 qxz = float2(p.x * c - p.z * s, p.x * s + p.z * c);

    float helixRadius = 0.78;
    // Two antiparallel sugar-phosphate backbone strands
    float dStrand1 = length(qxz - float2( helixRadius, 0.0)) - 0.14;
    float dStrand2 = length(qxz - float2(-helixRadius, 0.0)) - 0.14;

    // Nucleotide spheres along the strands
    float rungSpacing = 0.42;
    float localY = fmod(abs(p.y) + rungSpacing * 0.5, rungSpacing) - rungSpacing * 0.5;
    float dNode1 = length(float3(qxz - float2( helixRadius, 0.0), localY)) - 0.21;
    float dNode2 = length(float3(qxz - float2(-helixRadius, 0.0), localY)) - 0.21;
    float dBackbone = min(min(dStrand1, dStrand2), min(dNode1, dNode2));

    // Horizontal base-pair rungs connecting the two strands
    float rungX = clamp(qxz.x, -helixRadius, helixRadius);
    float dRung = length(float3(qxz.x - rungX, localY, qxz.y)) - 0.065;

    if (dBackbone < dRung) return float2(dBackbone * 0.7, 0.0);
    return float2(dRung * 0.7, (qxz.x > 0.0) ? 1.0 : 2.0);
}

float3 calcNormal(float3 p, float t) {
    float2 e = float2(0.0025, -0.0025);
    return normalize(
        e.xyy * mapDNA(p + e.xyy, t).x +
        e.yyx * mapDNA(p + e.yyx, t).x +
        e.yxy * mapDNA(p + e.yxy, t).x +
        e.xxx * mapDNA(p + e.xxx, t).x);
}

float4 PSMain(PSIn input) : SV_TARGET {
    float2 uv = (input.uv * 2.0 - 1.0) * float2(uAspect, -1.0);
    float t = uTime * 1.0;

    // Tilt the helix diagonally across the screen for cinematic composition
    float tilt = 0.38;
    float ct = cos(tilt), st = sin(tilt);
    float2 ruv = float2(uv.x * ct - uv.y * st, uv.x * st + uv.y * ct);

    float3 ro = float3(0.0, t * 0.65, -3.4);
    float3 rd = normalize(float3(ruv, 1.55));

    float dist = 0.0;
    float glow = 0.0;
    float2 hitRes = float2(0, 0);
    bool hit = false;

    [loop]
    for (int i = 0; i < 70; ++i) {
        float3 p = ro + rd * dist;
        hitRes = mapDNA(p, t);
        glow += 0.012 / (0.02 + hitRes.x * hitRes.x * 16.0);
        if (hitRes.x < 0.0025) { hit = true; break; }
        dist += hitRes.x;
        if (dist > 12.0) break;
    }

    float3 col = float3(0.01, 0.025, 0.06) * (1.2 - 0.35 * length(uv));

    if (hit) {
        float3 p = ro + rd * dist;
        float3 n = calcNormal(p, t);
        float3 lightDir = normalize(float3(0.6, 0.8, -0.7));

        float diff = max(dot(n, lightDir), 0.0);
        float spec = pow(max(dot(reflect(-lightDir, n), -rd), 0.0), 28.0);
        float fresnel = pow(1.0 - max(dot(n, -rd), 0.0), 2.5);

        // Color coding: Backbone = Electric Cyan/Blue, Adenine/Thymine/Guanine/Cytosine rungs = Magenta & Gold
        float3 baseCol = float3(0.10, 0.78, 1.0);
        if (hitRes.y > 1.5)      baseCol = float3(1.0, 0.22, 0.65);
        else if (hitRes.y > 0.5) baseCol = float3(0.25, 0.98, 0.55);

        col = baseCol * (0.28 + 0.72 * diff) + float3(1, 1, 1) * spec * 0.65 + baseCol * fresnel * 0.6;
    }

    col += float3(0.15, 0.65, 1.0) * glow * 0.085;
    col = 1.0 - exp(-col * 1.25);
    return float4(saturate(col), 1.0);
}
)HLSL";
        return InitFullscreenShader(device, kPS);
    }
};

} // namespace ScreensaverEngine
