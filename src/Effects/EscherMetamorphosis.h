#pragma once

#include "../Engine/ShaderHelper.h"

namespace ScreensaverEngine {

class EscherMetamorphosisEffect : public FullscreenShaderEffect {
public:
    const char* GetName() const override {
        return "Escher Metamorphosis Tessellation";
    }

    const char* GetDescription() const override {
        return "Interlocking figure-ground tessellations that progressively morph through impossible isometric cubes, organic Escher tiles, and Droste spirals.";
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

static const float PI = 3.14159265359;
static const float TAU = 6.28318530718;

float2x2 rot2D(float a) {
    float c = cos(a), s = sin(a);
    return float2x2(c, -s, s, c);
}

// Smoothly warps space between Euclidean plane, Droste log-polar spiral, and Poincaré hyperbolic disk
float2 escherSpaceWarp(float2 uv, float t) {
    // Slow breathing cycle for the global spatial conformal mapping
    float spiralPhase = smoothstep(-0.2, 0.85, sin(t * 0.17));
    float hyperPhase  = smoothstep(0.25, 0.95, cos(t * 0.13));

    float2 p = uv;

    // 1. Subtle Poincaré hyperbolic disk compression near edges (Escher's "Circle Limit")
    float r2 = dot(p, p);
    float2 hyperP = p / max(1.0 - min(r2 * 0.22, 0.78), 0.22);
    p = lerp(p, hyperP, hyperPhase * 0.65);

    // 2. Conformal Logarithmic Droste Spiral ("Print Gallery" / "Whirlpools")
    float r = max(length(p), 1e-4);
    float theta = atan2(p.y, p.x);
    // Log-polar coordinates where scale repeats periodically with angle
    float2 logPolar = float2(log(r) * 2.5 - t * 0.35, theta * 3.0 / PI);
    // Twist log-polar so grid lines form a seamless logarithmic spiral
    float2 spiralP = float2(
        logPolar.x + logPolar.y * 0.5,
        logPolar.y - logPolar.x * 0.5
    ) * 1.35;

    // Panning Euclidean lattice
    float2 panP = mul(rot2D(sin(t * 0.09) * 0.35), p * 3.8) + float2(t * 0.45, sin(t * 0.23) * 1.2);

    return lerp(panP, spiralP, spiralPhase * 0.72);
}

// Exact plane-filling interlocking Escher boundary deformation:
// By deforming horizontal and vertical cell boundaries with C2-continuous periodic curves
// whose symmetry alternates on even/odd edges, the light and dark figures remain 100% interlocking!
float escherInterlockingTile(float2 p, float morphAmount, float lobeComplexity, out float2 localCoord, out float edgeDist) {
    // Base square/rhombic lattice coordinates
    float2 i = floor(p);
    float2 f = frac(p) - 0.5; // [-0.5, 0.5]

    // Morphing wave functions along the 4 edges of each tile
    // Because sin(PI * x * 2) is periodic on [-0.5, 0.5], adjacent cells share identical deformed boundaries!
    float waveX = sin(f.y * TAU) * 0.28 * morphAmount
                + sin(f.y * TAU * 2.0 + lobeComplexity) * 0.11 * morphAmount;
    float waveY = sin(f.x * TAU) * 0.28 * morphAmount
                + cos(f.x * TAU * 2.0 - lobeComplexity) * 0.11 * morphAmount;

    // Rotational pinwheel Escher symmetry ("Swans" / "Fish & Birds")
    float2 warpedF = f + float2(waveX, -waveY);

    // Distance to the interlocking boundary between dark and light figures
    float2 dEdge = 0.5 - abs(warpedF);
    edgeDist = min(dEdge.x, dEdge.y);

    // Determine which figure (0 = Ivory Figure, 1 = Obsidian/Indigo Figure) owns this pixel
    // If warpedF spills across [-0.5, 0.5], it crosses smoothly into the neighboring interlocking figure!
    float2 shiftedP = p + float2(
        sin(p.y * TAU) * 0.26 * morphAmount + sin(p.y * TAU * 2.0 + lobeComplexity) * 0.10 * morphAmount,
        -sin(p.x * TAU) * 0.26 * morphAmount - cos(p.x * TAU * 2.0 - lobeComplexity) * 0.10 * morphAmount
    );

    float2 cellID = floor(shiftedP);
    localCoord = frac(shiftedP) - 0.5;

    // Smooth anti-aliased distance to the exact interlocking contour
    float2 gridDist = abs(localCoord) - 0.5;
    edgeDist = -max(gridDist.x, gridDist.y);

    // Checkerboard parity gives the figure-ground reversal (Figure A vs Figure B)
    float parity = fmod(abs(cellID.x + cellID.y), 2.0);
    return parity;
}

// Isometric impossible 3D cube shading (Escher's "Relativity" & "Metamorphosis" cube stage)
float3 isometricCubeShading(float2 p, float flipPhase) {
    // Hexagonal / Rhombic 3-axis coordinates
    float2 hexP = float2(p.x * 1.1547, p.y + p.x * 0.57735);
    float2 f = frac(hexP) - 0.5;

    // 3 rhombic faces of an isometric cube
    float a1 = step(0.0, f.x);
    float a2 = step(0.0, f.y);
    float a3 = step(0.0, f.x - f.y);

    // Smoothly invert perceived concavity/convexity (Necker cube optical illusion!)
    float light1 = lerp(0.92, 0.28, flipPhase);
    float light2 = 0.58;
    float light3 = lerp(0.25, 0.88, flipPhase);

    float shade = light2;
    if (f.x > 0.0 && f.y > 0.0) {
        shade = (f.x > f.y) ? light1 : light3;
    } else if (f.x < 0.0 && f.y < 0.0) {
        shade = (f.x < f.y) ? light3 : light1;
    } else if (f.x > 0.0 && f.y <= 0.0) {
        shade = light1;
    } else {
        shade = light3;
    }

    return float3(shade, shade, shade);
}

float4 PSMain(PSIn input) : SV_TARGET {
    float2 uv = (input.uv * 2.0 - 1.0);
    uv.x *= uAspect;
    uv.y = -uv.y;

    float t = uTime * 0.85;

    // Progressive metamorphosis varies smoothly across both time AND screen space
    // (just like Escher's long woodcut scroll "Metamorphosis I/II/III")
    float spatialWave = dot(uv, float2(cos(t * 0.11), sin(t * 0.11))) * 0.55;
    float cycle = t * 0.32 + spatialWave;

    // morphAmount: 0.0 = pure geometric lattice/cubes -> 1.0 = full organic interlocking Escher figures
    float morphAmount = smoothstep(-0.6, 0.7, sin(cycle));
    float lobePhase   = t * 0.9 + length(uv) * 1.2;
    float cubeBlend   = smoothstep(0.45, -0.55, sin(cycle)); // High when morphAmount is low

    // Apply conformal spatial warp (Euclidean <-> Hyperbolic Circle Limit <-> Droste Spiral)
    float2 warpedP = escherSpaceWarp(uv, t);

    float2 localCoord;
    float edgeDist;
    float parity = escherInterlockingTile(warpedP, morphAmount, lobePhase, localCoord, edgeDist);

    // Compute screen-space derivative for crisp, artifact-free anti-aliased woodcut lines
    float fw = max(fwidth(edgeDist), 0.002);
    float borderMask = smoothstep(0.0, fw * 1.8, edgeDist);

    // Internal Escher woodcut engraving details (eye/wing motifs & cross-hatching)
    // Figure A (parity == 0) rotates one way; Figure B (parity == 1) rotates the dual way!
    float2 motifUV = mul(rot2D((parity > 0.5 ? 1.0 : -1.0) * (PI * 0.25 + 0.35 * morphAmount)), localCoord);

    // Spiraling wing/scale arcs inside each creature tile
    float motifRadius = length(motifUV - float2(0.08 * morphAmount, 0.0));
    float motifAngle  = atan2(motifUV.y, motifUV.x);
    float featherRibs = sin(motifRadius * 28.0 - motifAngle * 3.0 + (parity > 0.5 ? PI : 0.0));
    float engraving   = smoothstep(-0.2, 0.3, featherRibs) * morphAmount;

    // Woodcut line-art cross-hatching texture
    float hatchCoord = (localCoord.x + (parity > 0.5 ? localCoord.y : -localCoord.y)) * 42.0;
    float hatch = 0.88 + 0.12 * sin(hatchCoord);

    // Subtle 3D relief bevel near tile edges
    float bevel = smoothstep(0.0, 0.22, edgeDist);

    // Color Palettes that progressively shift over time:
    // Warm Escher Lithograph Ivory/Gold vs Deep Woodcut Obsidian/Prussian Blue/Crimson
    float palShift = 0.5 + 0.5 * sin(t * 0.14);
    float3 lightTileCol = lerp(float3(0.95, 0.91, 0.82), float3(0.96, 0.84, 0.58), palShift);
    float3 darkTileCol  = lerp(float3(0.09, 0.14, 0.24), float3(0.22, 0.09, 0.16), palShift);
    float3 accentCol    = lerp(float3(0.82, 0.44, 0.18), float3(0.18, 0.68, 0.72), palShift);

    // Apply internal engraving and 3D bevel to both interlocking figures
    float3 figLight = lightTileCol * (0.78 + 0.22 * bevel) * hatch;
    figLight = lerp(figLight, accentCol * 1.1, (1.0 - engraving) * 0.28);

    float3 figDark = darkTileCol * (0.65 + 0.35 * bevel) * hatch;
    figDark += accentCol * engraving * 0.25;

    // Smooth anti-aliased figure-ground transition across the interlocking contour
    float3 tileColor = (parity < 0.5) ? figLight : figDark;

    // Blend in isometric impossible 3D cubes during the geometric stage of Metamorphosis
    if (cubeBlend > 0.01) {
        float flipIllusion = 0.5 + 0.5 * sin(t * 0.55 + spatialWave * 2.0);
        float3 cubeShade = isometricCubeShading(warpedP, flipIllusion);
        float3 isoCol = lerp(darkTileCol * 0.8, lightTileCol * 1.05, cubeShade.x) * hatch;
        tileColor = lerp(tileColor, isoCol, cubeBlend * 0.75);
    }

    // Crisp inked woodcut contour line separating the interlocking figures
    float3 inkColor = float3(0.04, 0.04, 0.06);
    float3 col = lerp(inkColor, tileColor, borderMask);

    // Subtle gallery spotlight vignette
    float vig = 1.0 - 0.22 * dot(uv, uv);
    col *= saturate(vig);

    return float4(saturate(col), 1.0);
}
)HLSL";
        return InitFullscreenShader(device, kPixelShader);
    }
};

} // namespace ScreensaverEngine
