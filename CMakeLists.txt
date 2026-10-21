cmake_minimum_required(VERSION 3.20)
project(ScreensaverStudio LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    src/main.cpp
    src/Engine/ShaderHelper.cpp
    src/Engine/Renderer.cpp
    src/Engine/Config.cpp
)

set(HEADERS
    src/Engine/IScreensaver.h
    src/Engine/ShaderHelper.h
    src/Engine/Renderer.h
    src/Engine/Config.h
    src/Effects/EffectRegistry.h
    src/Effects/CyberGameOfLife.h
    src/Effects/EscherMetamorphosis.h
    src/Effects/ReactionDiffusionCoral.h
    src/Effects/CosmicAndRelativityPack.h
    src/Effects/RetroAndCyberPack.h
    src/Effects/ArtisticAndOrganicPack.h
    src/Effects/ClassicAndElementalPack.h
    src/Effects/NeonCyberGrid.h
    src/Effects/FractalRaymarcher.h
    src/Effects/CosmicNebulaAttractor.h
    src/Effects/AuroraPlasmaWaves.h
)

add_executable(ScreensaverStudio WIN32 ${SOURCES} ${HEADERS})

target_compile_definitions(ScreensaverStudio PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(ScreensaverStudio PRIVATE d3d11 dxgi d3dcompiler)

# Automatically copy the compiled .exe to a .scr file in the same output directory
add_custom_command(TARGET ScreensaverStudio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:ScreensaverStudio>"
        "$<TARGET_FILE_DIR:ScreensaverStudio>/ScreensaverStudio.scr"
    COMMENT "Creating Windows Screensaver binary ScreensaverStudio.scr"
)
