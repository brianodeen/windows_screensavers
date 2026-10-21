#pragma once

#include "../Engine/ShaderHelper.h"

namespace ScreensaverEngine {

// ============================================================================
// 1. Cybernetic Matrix Digital Rain (3D Parallax)
// ============================================================================
class MatrixDigitalRainEffect : public FullscreenShaderEffect {
public:
    const char* GetName() const override {
        return "Cybernetic Digital Glyph Rain";
    }

    const char* GetDescription() const override {
        return "Multi-layered 3D parallax cascading procedural glyphs with glowing leading droplets and phosphor trails.";
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

// Procedural 5x7 sci-fi / katakana-inspired glyph generator
float proceduralGlyph(float2 cellUV, float seed) {
    // Margin inside each character cell
    if (any(abs(cellUV - 0.5) > 0.38)) return 0.0;

    float2 g = floor(cellUV * float2(5.0, 7.0));
    float bitIndex = g.y * 5.0 + g.x;
    float h = hash21(float2(seed, bitIndex));

    // Create structured vertical/horizontal strokes so glyphs look like authentic characters
    bool stroke = (h > 0.46) && (fmod(g.x + g.y, 2.0) < 1.5 || h > 0.72);
    return stroke ? 1.0 : 0.0;
}

float3 rainLayer(float2 uv, float scale, float speedMul, float t, float layerSeed) {
    float2 grid = uv * float2(scale * uAspect, scale * 0.65);
    float colID = floor(grid.x);
    float colHash = hash21(float2(colID, layerSeed));

    float fallSpeed = (2.5 + colHash * 3.5) * speedMul;
    float yOffset = t * fallSpeed + colHash * 100.0;

    float rowID = floor(grid.y + yOffset);
    float2 cellUV = frac(float2(grid.x, grid.y + yOffset));

    // Character mutates occasionally
    float charMutate = floor(t * (1.5 + frac(colHash * 13.0) * 4.0));
    float glyphSeed = hash21(float2(colID * 17.0 + rowID, charMutate * 0.1 + layerSeed));
    float glyph = proceduralGlyph(cellUV, floor(glyphSeed * 256.0));

    // Raindrop trail envelope along the column
    float dropPos = frac((grid.y + yOffset) * 0.065 + colHash * 7.0);
    float trail = pow(1.0 - dropPos, 3.2);
    float headGlow = smoothstep(0.06, 0.0, dropPos);

    // Occasional blank columns for depth contrast
    if (colHash < 0.18) return float3(0, 0, 0);

    float3 emerald = float3(0.05, 0.92, 0.32);
    float3 cyanTint = float3(0.10, 0.95, 0.75);
    float3 baseCol = lerp(emerald, cyanTint, frac(colHash * 7.3));

    float3 col = baseCol * glyph * trail * 1.15;
    // White-hot leading character at the tip of the raindrop
    col += float3(0.85, 1.0, 0.92) * glyph * headGlow * 1.8;
    // Subtle phosphor halo around the drop head
    col += baseCol * headGlow * 0.35;
    return col;
}

float4 PSMain(PSIn input) : SV_TARGET {
    float2 uv = input.uv;
    float t = uTime;

    // 3 parallax depth layers of digital rain (background, midground, foreground)
    float3 col = float3(0.005, 0.018, 0.010);
    col += rainLayer(uv + float2(0.03 * sin(t * 0.2), 0.0), 68.0, 0.55, t, 11.0) * 0.35;
    col += rainLayer(uv, 42.0, 0.85, t, 29.0) * 0.75;
    col += rainLayer(uv - float2(0.02 * sin(t * 0.2), 0.0), 26.0, 1.25, t, 53.0) * 1.10;

    // Expanding sonar data-pulse ring across the matrix
    float2 centered = (uv * 2.0 - 1.0) * float2(uAspect, 1.0);
    float pulseR = frac(t * 0.15) * 3.2;
    float ring = exp(-abs(length(centered) - pulseR) * 12.0) * (1.0 - frac(t * 0.15));
    col += float3(0.2, 1.0, 0.6) * ring * 0.35;

    // CRT scanline & vignette
    col *= 0.92 + 0.08 * sin(input.position.y * 2.0);
    col *= saturate(1.15 - 0.35 * dot(centered, centered));
    return float4(saturate(col), 1.0);
}
)HLSL";
        return InitFullscreenShader(device, kPS);
    }
};

// ============================================================================
// 2. Raymarched 3D Chrome Pipes (Modern Tribute to Classic Windows 3D Pipes)
// ============================================================================
class Raytraced3DPipesEffect : public FullscreenShaderEffect {
public:
    const char* GetName() const override {
        return "Raymarched 3D Metallic Pipes";
    }

    const char* GetDescription() const override {
        return "A next-gen raymarched tribute to the classic 3D Pipes screensaver with chrome, gold, and copper lattices and ball joints.";
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

float hash31(float3 p) {
    p = frac(p * float3(443.897, 441.423, 437.195));
    p += dot(p, p.yzx + 19.19);
    return frac((p.x + p.y) * p.z);
}

// Returns (signed distance, material ID) for infinite 3D pipe lattice with ball joints
float2 mapPipes(float3 p) {
    float cellSize = 2.4;
    float3 cellID = floor((p + cellSize * 0.5) / cellSize);
    float3 q = fmod(abs(p + cellSize * 0.5), cellSize) - cellSize * 0.5;

    float h = hash31(cellID);
    float pipeRadius = 0.21;
    float jointRadius = 0.34;

    // Central ball joint (classic Windows 3D Pipes elbow sphere!)
    float dBall = length(q) - jointRadius;

    // Connect pipes along 2 or 3 orthogonal axes depending on cell hash
    float dPipeX = length(q.yz) - pipeRadius;
    float dPipeY = length(q.xz) - pipeRadius;
    float dPipeZ = length(q.xy) - pipeRadius;

    float dPipes = dPipeZ;
    if (h < 0.33) {
        dPipes = min(dPipeX, dPipeY);
    } else if (h < 0.66) {
        dPipes = min(dPipeY, dPipeZ);
    } else {
        dPipes = min(dPipeX, dPipeZ);
    }

    // Flange rings along the pipes
    float flange = abs(frac(max(abs(q.x), max(abs(q.y), abs(q.z))) * 2.5) - 0.5) - 0.08;
    float dFlange = max(dPipes - 0.035, -flange);

    float d = min(dBall, min(dPipes, dFlange));
    return float2(d, h);
}

float3 calcNormal(float3 p) {
    float2 e = float2(0.002, -0.002);
    return normalize(
        e.xyy * mapPipes(p + e.xyy).x +
        e.yyx * mapPipes(p + e.yyx).x +
        e.yxy * mapPipes(p + e.yxy).x +
        e.xxx * mapPipes(p + e.xxx).x);
}

float4 PSMain(PSIn input) : SV_TARGET {
    float2 uv = (input.uv * 2.0 - 1.0) * float2(uAspect, -1.0);
    float t = uTime * 1.15;

    // Camera weaving smoothly between the 3D pipe lattice
    float3 ro = float3(1.2, 1.2, t * 2.2);
    float3 rd = normalize(float3(uv, 1.5));
    rd.xy = mul(rot2D(sin(t * 0.22) * 0.45), rd.xy);
    rd.xz = mul(rot2D(cos(t * 0.17) * 0.35), rd.xz);

    float dist = 0.0;
    float2 hitRes = float2(0, 0);
    bool hit = false;

    [loop]
    for (int i = 0; i < 80; ++i) {
        float3 p = ro + rd * dist;
        hitRes = mapPipes(p);
        if (hitRes.x < 0.002) { hit = true; break; }
        dist += hitRes.x * 0.85;
        if (dist > 32.0) break;
    }

    float3 col = float3(0.015, 0.02, 0.035);

    if (hit) {
        float3 p = ro + rd * dist;
        float3 n = calcNormal(p);
        float3 refl = reflect(rd, n);

        // Metallic material selection: Polished Chrome, 24K Gold, Copper, Cyber Teal, Crimson Anodized
        float matHash = frac(hitRes.y * 13.7);
        float3 albedo = float3(0.85, 0.88, 0.92); // Chrome
        if (matHash < 0.22)      albedo = float3(1.00, 0.78, 0.28); // Gold
        else if (matHash < 0.44) albedo = float3(0.95, 0.48, 0.30); // Copper
        else if (matHash < 0.66) albedo = float3(0.18, 0.82, 0.95); // Anodized Cyan
        else if (matHash < 0.82) albedo = float3(0.92, 0.20, 0.42); // Anodized Ruby

        // Procedural studio reflection environment
        float envBands = 0.5 + 0.5 * sin(refl.y * 8.0 + refl.x * 4.0 + t * 0.5);
        float3 light1 = normalize(float3(0.6, 0.8, -0.5));
        float3 light2 = normalize(float3(-0.7, -0.4, 0.6));

        float diff1 = max(dot(n, light1), 0.0);
        float diff2 = max(dot(n, light2), 0.0);
        float spec1 = pow(max(dot(refl, light1), 0.0), 32.0);
        float spec2 = pow(max(dot(refl, light2), 0.0), 24.0);

        // glowing pulse traveling along pipes
        float pulse = exp(-abs(frac((p.x + p.y + p.z) * 0.15 - t * 0.6) - 0.5) * 18.0);

        float3 metalCol = albedo * (0.18 + 0.55 * diff1 + 0.35 * diff2 + 0.45 * envBands);
        metalCol += albedo * (spec1 * 1.1 + spec2 * 0.7);
        metalCol += float3(0.4, 0.95, 1.0) * pulse * 0.55;

        float fog = exp(-dist * 0.075);
        col = lerp(float3(0.015, 0.02, 0.035), metalCol, fog);
    }

    col = 1.0 - exp(-col * 1.25);
    return float4(saturate(col), 1.0);
}
)HLSL";
        return InitFullscreenShader(device, kPS);
    }
};

// ============================================================================
// 3. Analog CRT Lissajous Vector Oscilloscope
// ============================================================================
class SynthOscilloscopeEffect : public FullscreenShaderEffect {
public:
    const char* GetName() const override {
        return "CRT Vector Lissajous Oscilloscope";
    }

    const char* GetDescription() const override {
        return "Analog phosphor CRT oscilloscope drawing rotating 3D harmonic Lissajous knots with green/cyan electron beam bloom.";
    }

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override {
        static const char* kPS = R"HLSL(
cbuffer StandardConstants : register(b0) {
    float2 uResolution; float uTime; float uDeltaTime;
    float uAspect; float uSpeed; float uParam1; float uParam2;
};
struct PSIn { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };

float distToSegment(float2 p, float2 a, float2 b) {
    float2 pa = p - a, ba = b - a;
    float h = saturate(dot(pa, ba) / max(dot(ba, ba), 1e-6));
    return length(pa - ba * h);
}

float2 evalLissajous(float s, float t) {
    // Slowly evolving harmonic frequency ratios + 3D rotation projection
    float fx = 3.0, fy = 4.0, fz = 5.0;
    float3 p3 = float3(
        sin(fx * s + t * 0.45),
        sin(fy * s - t * 0.31),
        cos(fz * s + t * 0.23)
    );
    // Modulate with secondary epicycle harmonic
    p3.xy += 0.22 * float2(cos(7.0 * s - t * 0.6), sin(7.0 * s + t * 0.6));

    // Rotate 3D knot around Y and X axes
    float ay = t * 0.28, ax = t * 0.19;
    float cy = cos(ay), sy = sin(ay);
    p3 = float3(p3.x * cy - p3.z * sy, p3.y, p3.x * sy + p3.z * cy);
    float cx = cos(ax), sx = sin(ax);
    p3 = float3(p3.x, p3.y * cx - p3.z * sx, p3.y * sx + p3.z * cx);

    return p3.xy * (0.88 / (1.45 + p3.z * 0.25));
}

float4 PSMain(PSIn input) : SV_TARGET {
    float2 uv = (input.uv * 2.0 - 1.0) * float2(uAspect, -1.0);
    // Subtle curved CRT glass barrel distortion
    uv *= 1.0 + 0.06 * dot(uv, uv);

    // Oscilloscope graticule grid
    float2 grid = abs(frac(uv * 4.0) - 0.5);
    float graticule = smoothstep(0.02, 0.0, min(grid.x, grid.y));
    float axisLine  = smoothstep(0.012, 0.0, min(abs(uv.x), abs(uv.y)));

    float3 col = float3(0.008, 0.028, 0.018);
    col += float3(0.02, 0.11, 0.06) * (graticule * 0.45 + axisLine * 0.8);

    // Trace the parametric electron beam across 96 segments
    float t = uTime * 1.1;
    const int N = 96;
    float dtParam = 6.2831853 / float(N);
    float2 prevPt = evalLissajous(0.0, t);

    float beamAccum = 0.0;
    float haloAccum = 0.0;

    [loop]
    for (int i = 1; i <= N; ++i) {
        float s = float(i) * dtParam;
        float2 currPt = evalLissajous(s, t);
        float d = distToSegment(uv, prevPt, currPt);

        // Slower electron beam velocity = brighter phosphor burn
        float segLen = length(currPt - prevPt) + 0.015;
        float intensity = 0.045 / segLen;

        beamAccum += exp(-d * 140.0) * intensity;
        haloAccum += (0.0008 / (d * d + 0.0006)) * intensity;
        prevPt = currPt;
    }

    // Phosphor P31 Green / Cyan shift
    float3 phosphorCore = float3(0.75, 1.0, 0.85);
    float3 phosphorGlow = lerp(float3(0.08, 0.95, 0.42), float3(0.12, 0.85, 0.95), 0.5 + 0.5 * sin(t * 0.2));

    col += phosphorGlow * haloAccum * 0.42;
    col += phosphorCore * beamAccum * 0.85;

    // CRT bezel vignette
    float vig = 1.0 - 0.22 * dot(uv, uv);
    col *= saturate(vig);
    return float4(saturate(col), 1.0);
}
)HLSL";
        return InitFullscreenShader(device, kPS);
    }
};

// ============================================================================
// 4. Crystalline Low-Poly Origami World
// ============================================================================
class LowPolySynthMountainEffect : public FullscreenShaderEffect {
public:
    const char* GetName() const override {
        return "Crystalline Low-Poly World";
    }

    const char* GetDescription() const override {
        return "Faceted flat-shaded triangulated crystal mountains with specular origami lighting and an evolving aurora sky.";
    }

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override {
        static const char* kPS = R"HLSL(
cbuffer StandardConstants : register(b0) {
    float2 uResolution; float uTime; float uDeltaTime;
    float uAspect; float uSpeed; float uParam1; float uParam2;
};
struct PSIn { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };

float hash21(float2 p) {
    p = frac(p * float2(127.1, 311.7));
    p += dot(p, p + 19.19);
    return frac(p.x * p.y);
}

float vertexHeight(float2 v) {
    float valley = smoothstep(0.6, 4.2, abs(v.x));
    float h = sin(v.x * 0.7) * cos(v.y * 0.7) * 1.4 + hash21(v) * 1.35;
    return h * valley;
}

// Evaluates exact barycentric flat-shaded triangle height and facet normal on a simplicial grid
float lowPolyTerrain(float2 p, out float3 facetNormal, out float2 triID) {
    // Skew to equilateral-like triangle lattice
    float2 grid = p * 0.95;
    float2 i = floor(grid);
    float2 f = frac(grid);

    float3 v0, v1, v2;
    if (f.x > f.y) {
        v0 = float3(i.x,       vertexHeight(i),                 i.y);
        v1 = float3(i.x + 1.0, vertexHeight(i + float2(1, 0)),  i.y);
        v2 = float3(i.x + 1.0, vertexHeight(i + float2(1, 1)),  i.y + 1.0);
        triID = i * 2.0;
    } else {
        v0 = float3(i.x,       vertexHeight(i),                 i.y);
        v1 = float3(i.x + 1.0, vertexHeight(i + float2(1, 1)),  i.y + 1.0);
        v2 = float3(i.x,       vertexHeight(i + float2(0, 1)),  i.y + 1.0);
        triID = i * 2.0 + 1.0;
    }

    facetNormal = normalize(cross(v2 - v0, v1 - v0));
    if (facetNormal.y < 0.0) facetNormal = -facetNormal;

    // Interpolate plane height at fractional position f
    float h = v0.y;
    if (f.x > f.y) {
        h = v0.y + f.x * (v1.y - v0.y) + f.y * (v2.y - v1.y);
    } else {
        h = v0.y + f.y * (v2.y - v0.y) + f.x * (v1.y - v2.y);
    }
    return h;
}

float4 PSMain(PSIn input) : SV_TARGET {
    float2 uv = (input.uv * 2.0 - 1.0) * float2(uAspect, -1.0);
    float t = uTime * 1.8;

    float3 ro = float3(0.0, 2.35, t);
    float3 rd = normalize(float3(uv.x, uv.y - 0.15, 1.55));

    // Dynamic twilight sky
    float3 skyTop = float3(0.04, 0.08, 0.22);
    float3 skyBot = float3(0.95, 0.42, 0.35);
    float3 col = lerp(skyBot, skyTop, saturate(rd.y * 2.0 + 0.1));

    // Low-poly faceted sun
    float sunDist = length(uv - float2(0.0, 0.25));
    if (sunDist < 0.32) {
        col = lerp(float3(1.0, 0.55, 0.25), float3(1.0, 0.95, 0.65), saturate((uv.y - 0.0) * 2.5));
    }

    float dist = 0.0;
    bool hit = false;
    float3 facetN = float3(0, 1, 0);
    float2 triID = float2(0, 0);

    [loop]
    for (int i = 0; i < 64; ++i) {
        float3 p = ro + rd * dist;
        float h = lowPolyTerrain(p.xz, facetN, triID);
        float d = p.y - h;
        if (d < 0.02) { hit = true; break; }
        dist += max(d * 0.5, 0.08);
        if (dist > 36.0) break;
    }

    if (hit) {
        float3 p = ro + rd * dist;
        float3 lightDir = normalize(float3(0.35, 0.65, 0.7));
        float diff = saturate(dot(facetN, lightDir));
        float spec = pow(saturate(dot(reflect(-lightDir, facetN), -rd)), 18.0);

        float facetRand = hash21(triID);
        float3 baseCol = lerp(float3(0.15, 0.32, 0.62), float3(0.75, 0.28, 0.55), saturate(p.y * 0.35 + facetRand * 0.25));
        // Snow-capped crystal peaks
        if (p.y > 1.65) baseCol = lerp(baseCol, float3(0.92, 0.96, 1.0), 0.75);

        float3 litFacet = baseCol * (0.28 + 0.78 * diff) + float3(1.0, 0.85, 0.7) * spec * 0.55;
        float fog = exp(-dist * 0.065);
        col = lerp(skyBot * 0.9, litFacet, fog);
    }

    return float4(saturate(col), 1.0);
}
)HLSL";
        return InitFullscreenShader(device, kPS);
    }
};

} // namespace ScreensaverEngine
