#pragma once

#include "../Engine/IScreensaver.h"
#include "NeonCyberGrid.h"
#include "FractalRaymarcher.h"
#include "CosmicNebulaAttractor.h"
#include "AuroraPlasmaWaves.h"
#include "EscherMetamorphosis.h"
#include "CyberGameOfLife.h"
#include "ReactionDiffusionCoral.h"
#include "CosmicAndRelativityPack.h"
#include "RetroAndCyberPack.h"
#include "ArtisticAndOrganicPack.h"
#include "ClassicAndElementalPack.h"
#include <memory>
#include <vector>

namespace ScreensaverEngine {

// Complete Library of all 25 GPU Screensaver Effects (5 Original + 20 Added, in strict chronological order)
inline std::vector<std::unique_ptr<IScreensaver>> CreateAllEffects() {
    std::vector<std::unique_ptr<IScreensaver>> effects;
    effects.reserve(25);

    // ------------------------------------------------------------------------
    // 1–5: Original 5 Screensavers (Preserved at #1–#5!)
    // ------------------------------------------------------------------------
    effects.push_back(std::make_unique<NeonCyberGridEffect>());            // 1
    effects.push_back(std::make_unique<FractalRaymarcherEffect>());        // 2
    effects.push_back(std::make_unique<CosmicNebulaAttractorEffect>());    // 3
    effects.push_back(std::make_unique<AuroraPlasmaWavesEffect>());        // 4
    effects.push_back(std::make_unique<EscherMetamorphosisEffect>());      // 5

    // ------------------------------------------------------------------------
    // 6–25: 20 Additional Screensavers (Appended!)
    // ------------------------------------------------------------------------
    effects.push_back(std::make_unique<CyberGameOfLifeEffect>());          // 6
    effects.push_back(std::make_unique<ReactionDiffusionCoralEffect>());   // 7
    effects.push_back(std::make_unique<GargantuaBlackHoleEffect>());       // 8
    effects.push_back(std::make_unique<LiquidFerrofluidEffect>());         // 9
    effects.push_back(std::make_unique<LavaLampMetaballsEffect>());        // 10
    effects.push_back(std::make_unique<WarpSpeedStarfieldEffect>());       // 11
    effects.push_back(std::make_unique<MatrixDigitalRainEffect>());        // 12
    effects.push_back(std::make_unique<Raytraced3DPipesEffect>());         // 13
    effects.push_back(std::make_unique<SynthOscilloscopeEffect>());        // 14
    effects.push_back(std::make_unique<LowPolySynthMountainEffect>());     // 15
    effects.push_back(std::make_unique<StainedGlassVoronoiEffect>());      // 16
    effects.push_back(std::make_unique<BioluminescentJellyfishEffect>());  // 17
    effects.push_back(std::make_unique<SteampunkClockworkEffect>());       // 18
    effects.push_back(std::make_unique<JapaneseInkWaveEffect>());          // 19
    effects.push_back(std::make_unique<DNAHelixLatticeEffect>());          // 20
    effects.push_back(std::make_unique<PlasmaGlobeTeslaEffect>());         // 21
    effects.push_back(std::make_unique<MystifyGeometricRibbonsEffect>());  // 22
    effects.push_back(std::make_unique<AudioSpectrumEqualizerEffect>());   // 23
    effects.push_back(std::make_unique<CrystalGeodeCavernEffect>());       // 24
    effects.push_back(std::make_unique<SolarFlareCoronalLoopEffect>());    // 25

    return effects;
}

} // namespace ScreensaverEngine
