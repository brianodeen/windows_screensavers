# Windows Screensaver Studio (C++17 & Direct3D 11)

A lightweight, zero-dependency, GPU-accelerated Windows Screensaver Engine & Effect Library built with native Win32, Direct3D 11, and HLSL Shader Model 5.0, featuring **first-class multi-monitor support**.

## Features
- **True Per-Monitor Rendering & Per-Monitor Effect Selection**:
  - Enumerates each physical monitor (`EnumDisplayMonitors`) and creates an **independent Direct3D 11 swap chain and viewport per monitor** so visuals are centered and aspect-correct on each screen (even if your monitors have different resolutions or orientations).
  - Synchronizes VSync on the primary monitor while presenting secondary monitors without double-blocking, maintaining full native refresh rates across all displays.
  - **Per-Monitor Settings (`/c` or press `C`)**: Choose a specific screensaver for **Monitor 1** and **Monitor 2** independently, set Monitor 2 to **Mirror Monitor 1**, or set it to **Auto-Cycle (Different from Monitor 1)** so both screens automatically rotate through your library without showing duplicates.
- **Nighttime / Sleep Mode (Universal Post-Processing Filter)**:
  - Applies a 13-tap soft-focus spatial filter, compresses bright highlight peaks, warms short-wavelength blue light toward circadian-friendly amber tones, and dims overall output so it never glares or wakes you up.
  - Configurable in Settings (`/c` or `C`) or toggled live with **`N`**:
    - **Off (100% Full Vibrancy & Brightness)**
    - **Nighttime Soft & Dim (40% Brightness, Warm Low-Blue Glow, Soft Focus)**
    - **Ultra-Dark Sleep Mode (20% Brightness, Deep Amber Candlelight)**
    - **Auto-Schedule Nighttime (Automatically enables 40% Soft & Dim between 10:00 PM and 7:00 AM)**
- **Dual Mode Execution**:
  - **Interactive Studio Mode (`ScreensaverStudio.exe`)**: Opens a resizable 1280x720 preview window on Monitor 1. Press **`M`** at any time to spawn a live fullscreen output on **Monitor 2** while keeping interactive keyboard controls!
  - **Native Windows `.scr` Support (`ScreensaverStudio.scr`)**:
    - `/s`: Fullscreen multi-monitor screensaver mode (spawns an independent fullscreen window on each connected monitor, hides cursor, exits on mouse movement or key press).
    - `/p <HWND>`: Renders live inside the miniature preview monitor in the Windows *Screen Saver Settings* control panel.
    - `/c`: Opens the Multi-Monitor Configuration Dialog (saved in `HKCU\Software\ScreensaverStudio`).
- **Included Library of 25 GPU Screensaver Effects (5 Original + 20 Added in Order)**:
  1. **Neon Cyber Grid** (`src/Effects/NeonCyberGrid.h`): Procedural synthwave 3D canyon flight with glowing neon grid, twin striped retro sun, and starfield.
  2. **Quantum Fractal Cathedral** (`src/Effects/FractalRaymarcher.h`): Real-time 3D raymarched Kaleidoscopic IFS crystal architecture with volumetric glow and ambient occlusion.
  3. **Cosmic Nebula Swarm (65K GPU Particles)** (`src/Effects/CosmicNebulaAttractor.h`): 65,536 additive-blended 3D plasma particles flowing through harmonic vortex attractors entirely on the GPU.
  4. **Bioluminescent Aurora Flux** (`src/Effects/AuroraPlasmaWaves.h`): Silky domain-warped aurora ribbons and harmonic plasma interference waves.
  5. **Escher Metamorphosis Tessellation** (`src/Effects/EscherMetamorphosis.h`): Interlocking figure-ground tessellations that progressively morph through impossible isometric cubes, organic Escher woodcut tiles, Poincaré hyperbolic disks, and Droste spirals.
  6. **Conway's Game of Life (Cyber Automata)** (`src/Effects/CyberGameOfLife.h`): True GPU ping-pong B3/S23 cellular automaton with cell-age color evolution, glider phosphor trails, and continuous cosmic seeding.
  7. **Turing Reaction-Diffusion Morphogenesis** (`src/Effects/ReactionDiffusionCoral.h`): Live GPU Gray-Scott reaction-diffusion chemical simulation growing organic coral labyrinths and mitotic spots with 3D specular emboss.
  8. **Gargantua Black Hole (Gravitational Lensing)** (`src/Effects/CosmicAndRelativityPack.h`): Relativistic gravitational ray-bending around an event horizon with Doppler-beamed accretion plasma and photon ring.
  9. **Magnetic Liquid Ferrofluid** (`src/Effects/CosmicAndRelativityPack.h`): Raymarched 3D liquid-metal ferrofluid sculpture erupting with harmonic magnetic spikes under neon studio lights.
  10. **Subsurface Lava Lamp** (`src/Effects/CosmicAndRelativityPack.h`): 3D raymarched buoyancy-driven wax metaballs rising, splitting, and coalescing with warm subsurface scattering.
  11. **Hyperspace Warp Starfield** (`src/Effects/CosmicAndRelativityPack.h`): Multi-layered 3D relativistic star streaks racing through glowing volumetric nebula clouds.
  12. **Cybernetic Digital Glyph Rain** (`src/Effects/RetroAndCyberPack.h`): Multi-layered 3D parallax cascading procedural glyphs with glowing leading droplets and phosphor trails.
  13. **Raymarched 3D Metallic Pipes** (`src/Effects/RetroAndCyberPack.h`): Next-gen raymarched tribute to the classic 3D Pipes screensaver with chrome, gold, and copper lattices and ball joints.
  14. **CRT Vector Lissajous Oscilloscope** (`src/Effects/RetroAndCyberPack.h`): Analog phosphor CRT oscilloscope drawing rotating 3D harmonic Lissajous knots with green/cyan electron beam bloom.
  15. **Crystalline Low-Poly World** (`src/Effects/RetroAndCyberPack.h`): Faceted flat-shaded triangulated crystal mountains with specular origami lighting and an evolving aurora sky.
  16. **Stained Glass Cathedral Rose** (`src/Effects/ArtisticAndOrganicPack.h`): Kaleidoscopic stained-glass rose window with Voronoi crystal shards, leaded solder borders, and cathedral sunbeams.
  17. **Abyssal Bioluminescent Jellyfish** (`src/Effects/ArtisticAndOrganicPack.h`): 3D raymarched translucent jellyfish pulsing with neon bioluminescence in deep ocean currents.
  18. **Celestial Brass Clockwork** (`src/Effects/ArtisticAndOrganicPack.h`): Interlocking counter-rotating brass, copper, and silver clockwork gears with anisotropic brushed metal and glowing runes.
  19. **Ukiyo-e Woodblock Ocean Waves** (`src/Effects/ArtisticAndOrganicPack.h`): Stylized Hokusai-inspired rolling woodblock ocean waves, curling foam crests, falling sakura petals, and a crimson moon.
  20. **Quantum DNA Double Helix** (`src/Effects/ArtisticAndOrganicPack.h`): 3D raymarched twisting DNA double-helix molecular strands with glowing base-pair rungs and depth-of-field bokeh.
  21. **High-Voltage Tesla Plasma Globe** (`src/Effects/ClassicAndElementalPack.h`): 3D glass plasma sphere with branching volumetric electric filaments arcing from a central high-voltage electrode.
  22. **Neon Mystify Polygons (Classic Tribute)** (`src/Effects/ClassicAndElementalPack.h`): Bouncing multi-vertex wireframe polygons with glowing neon echo trails, inspired by classic Windows Mystify.
  23. **3D Synth Spectrum Equalizer** (`src/Effects/ClassicAndElementalPack.h`): LED-segmented 3D spectrum analyzer equalizer bars with floating peak-hold caps and glossy floor reflections.
  24. **Amethyst Crystal Geode Cavern** (`src/Effects/ClassicAndElementalPack.h`): Raymarched subterranean crystal cavern lined with faceted specular amethyst and quartz prisms.
  25. **Solar Corona & Magnetic Flare Loops** (`src/Effects/ClassicAndElementalPack.h`): Boiling solar photosphere granulation cells with erupting magnetic coronal plasma loops and solar wind.

## Quick Start

### Build & Run from PowerShell
```powershell
# Build Release binaries (.exe and .scr) and launch Interactive Studio Mode:
.\build.ps1 -Run

# Open the Multi-Monitor Configuration Dialog (/c):
.\build.ps1 -ConfigDialog

# Test Fullscreen Multi-Monitor Screensaver Mode (/s):
.\build.ps1 -FullscreenScreensaver
```

### Interactive Studio Hotkeys (`ScreensaverStudio.exe`)
- **`Left` / `Right` Arrows** or **`1`–`4`**: Switch active effect on **Monitor 1**
- **`M`**: Toggle live fullscreen preview on **Monitor 2** (Dual-Monitor Studio Mode)
- **`[` / `]`** or **`Tab`** or **`Shift + 1`–`4`**: Switch active effect on **Monitor 2**
- **`Up` / `Down` Arrows**: Adjust animation speed (`0.1x` – `4.0x`)
- **`Space`**: Pause / Resume animation
- **`C`**: Open the **Multi-Monitor Settings Dialog**
- **`F11`**: Toggle fullscreen on Monitor 1 (`Esc` exits multi-monitor preview / fullscreen)

### Installing as Your Active Windows Screensaver
Right-click `build\Release\ScreensaverStudio.scr` in File Explorer and click **Install** (or **Configure** to pick which screensaver plays on Monitor 1 and Monitor 2).
