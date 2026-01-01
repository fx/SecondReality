# Second Reality Cross-Platform Refactor

True-to-original recreation of Second Reality (Future Crew, 1993) using Sokol + Emscripten for cross-platform native and browser support.

## Architecture Overview

```
/workspace/
├── build/                 # Build outputs (native + wasm)
├── src/                   # New cross-platform source
│   ├── core/              # Sokol app, DIS replacement, utilities
│   ├── audio/             # libopenmpt integration for S3M playback
│   └── parts/             # Ported scene code (mirrors original dirs)
├── [ORIGINAL DIRS]/       # Preserved original source (ALKU, BEG, etc.)
└── docs/PROJECT.md        # This file
```

**Tech Stack:**
- **Graphics:** Sokol (sokol_app.h, sokol_gfx.h, sokol_gl.h)
- **Audio:** libopenmpt (S3M playback)
- **Sync:** GNU Rocket or custom sync system
- **Build:** CMake + Emscripten for WASM

## Tasks

- [ ] Phase 0: Infrastructure Setup
  - [ ] Initialize CMake build system with Sokol
    - [ ] Add sokol as submodule/vendor
    - [ ] Configure native builds (Linux, macOS, Windows)
    - [ ] Configure Emscripten/WASM build
  - [x] Create core framework (replaces DIS) (PR #2)
    - [x] Create `src/core/dis.h` - demo interrupt server API
    - [x] Implement `dis_waitb()` - frame sync using sokol_app
    - [x] Implement `dis_exit()` - clean exit handling
    - [x] Implement `dis_muscode()` - music sync interface
    - [x] Implement `dis_setcopper()` - palette/raster effects
  - [x] Create video subsystem (PR #3)
    - [x] Implement 320x200 framebuffer with scaling
    - [x] Implement 320x400 mode support
    - [x] Implement palette management (256 colors)
    - [x] Implement page flipping and horizontal scrolling
  - [ ] Integrate libopenmpt for S3M audio
    - [ ] Add libopenmpt dependency
    - [ ] Create `src/audio/music.h` API
    - [ ] Load MUSIC0.S3M and MUSIC1.S3M
    - [ ] Implement playback position tracking for sync
  - [ ] Create part loader framework
    - [ ] Define part interface (init, update, render, cleanup)
    - [ ] Implement part sequencing from SCRIPT
    - [ ] Create timing/sync system

- [ ] Phase 1: Opening Sequence (Parts 1-5)
  - [ ] Port ALKU (Opening Credits)
    - [ ] Port MAIN.C to src/parts/alku/
    - [ ] Convert COPPER.ASM palette effects to shaders
    - [ ] Load FONA font data (FONA.INC → runtime load)
    - [ ] Load horizon images (HOI.IN0, HOI.IN1)
    - [ ] Implement text fade-in with scrolling background
    - [ ] Test: "A FUTURE CREW production" text displays correctly
  - [ ] Port BEG (Title Screen)
    - [ ] Port BEG.C to src/parts/beg/
    - [ ] Load SRTITLE.LBM title image
    - [ ] Implement fade transitions
    - [ ] Test: Second Reality logo displays with fade
  - [ ] Port 3DS/Vector Ship Flyby
    - [ ] Extract ship geometry from 3DS project files
    - [ ] Port vector rendering from VISU library
    - [ ] Implement ship approach animation
    - [ ] Implement shrink-to-point effect
    - [ ] Test: Ship flies overhead toward horizon
  - [ ] Port PANIC (Praxis Explosion)
    - [ ] Port KOE.C rasterizer to src/parts/panic/
    - [ ] Convert KOEA.ASM/KOEB.ASM polygon filling
    - [ ] Load TROLL.UP precalc data
    - [ ] Implement expanding ring effect
    - [ ] Test: Explosion rings expand toward viewer
  - [ ] Port FCP (Logo Rotation)
    - [ ] Port logo display code
    - [ ] Implement precalculated rotation playback
    - [ ] Test: FC logo with rotation effect

- [ ] Phase 2: Glenz & Tunnel Effects (Parts 6-8)
  - [ ] Port GLENZ (Jello Blob)
    - [ ] Port MAIN.C controller to src/parts/glenz/
    - [ ] Convert VEC.ASM vector math to C/shaders
    - [ ] Convert VIDPOLY.ASM polygon rasterizer
    - [ ] Load ZOOMLOOP.INC transformation data
    - [ ] Implement checkerboard plate flip animation
    - [ ] Implement jello blob drop and bounce
    - [ ] Implement dual rotating blob effect
    - [ ] Test: Full Glenz sequence plays correctly
  - [ ] Port DOTS (Dot Tunnel)
    - [ ] Port MAIN.C to src/parts/dots/
    - [ ] Convert ASM.ASM dot rendering
    - [ ] Implement tunnel formation around sphere
    - [ ] Implement fade to black
    - [ ] Test: Dot tunnel surrounds and fades
  - [ ] Port GRID (Interference Pattern)
    - [ ] Port MAIN.C to src/parts/grid/
    - [ ] Convert LINEBLIT.ASM line drawing
    - [ ] Load lookup tables (LINEBLIT.INC)
    - [ ] Implement moiré interference effect
    - [ ] Test: Grid pattern animates correctly

- [ ] Phase 3: Techno & 3D Effects (Parts 9-11)
  - [ ] Port TECHNO (Rotating Bars)
    - [ ] Port KOE.C to src/parts/techno/
    - [ ] Convert polygon rasterizer for bars
    - [ ] Load SIN4096.INC sine tables
    - [ ] Implement bar rotation synchronized to music
    - [ ] Implement acceleration and pause
    - [ ] Test: Techno bars rotate to beat
  - [ ] Port HARD (3D Vectors)
    - [ ] Port B.C 3D engine to src/parts/hard/
    - [ ] Convert VEC.ASM and VMATH.ASM
    - [ ] Implement 3D transformation pipeline
    - [ ] Test: Vector objects render and rotate
  - [ ] Port COMAN (Voxel Terrain)
    - [ ] Port MAIN.C to src/parts/coman/
    - [ ] Convert THELOOP.ASM heightfield renderer
    - [ ] Load wave data (W1DTA.BIN, W2DTA.BIN)
    - [ ] Implement sinusoidal terrain deformation
    - [ ] Implement camera movement and fade
    - [ ] Test: Canyon terrain undulates correctly

- [ ] Phase 4: Visual Effects (Parts 12-18)
  - [ ] Port WATER (Ray-traced Reflections)
    - [ ] Load precalculated frames (WAT1-4.DAT)
    - [ ] Implement frame sequencing
    - [ ] Test: Water reflection animation plays
  - [ ] Port FOREST (Parallax Terrain)
    - [ ] Convert Pascal units (AOS1-3.TPU) to C
    - [ ] Load background images
    - [ ] Implement parallax scrolling
    - [ ] Test: Forest scrolls with parallax
  - [ ] Port TUNNELI (Ball Tunnel)
    - [ ] Convert Pascal source (TUN9.PAS, TUN10.PAS) to C
    - [ ] Load tunnel shape data
    - [ ] Implement sinusoidal ball movement
    - [ ] Test: Ball tunnel recedes correctly
  - [ ] Port TWIST (Rotation Effect)
    - [ ] Port MAIN.C to src/parts/twist/
    - [ ] Convert TWIST.ASM transformation
    - [ ] Load TWSTLOOP.INC lookup table
    - [ ] Test: Twist effect animates
  - [ ] Port PAM (Flic Animation)
    - [ ] Implement FLIC decoder (from GRAB/FLIC/)
    - [ ] Load PRAX4.FLI animation
    - [ ] Implement frame playback
    - [ ] Test: Explosion animation plays
  - [ ] Port JPLOGO (Logo Zoom)
    - [ ] Port JP.C zoom engine
    - [ ] Load ZOOM.INC transformation data
    - [ ] Implement bounce/pump effect
    - [ ] Test: Logo zooms with bounce
  - [ ] Port LENS (Lens Distortion)
    - [ ] Port MAIN.C to src/parts/lens/
    - [ ] Load LENS.DAT distortion coefficients
    - [ ] Implement lens bounce effect
    - [ ] Implement 3-color shading
    - [ ] Test: Lens distorts and bounces correctly

- [ ] Phase 5: Stars & Plasma (Parts 19-21)
  - [ ] Port DDSTARS (Desert Dream Stars)
    - [ ] Convert STARS.ASM star field renderer
    - [ ] Convert POLYEGA.ASM for EGA mode
    - [ ] Implement 3D cube approach
    - [ ] Test: Star field with cube approach
  - [ ] Port PLZPART (Plasma Effect)
    - [ ] Port PLZ.C plasma engine
    - [ ] Convert PLZA.ASM plasma generator
    - [ ] Load precalc tables (PSINI.INC, TILE.INC)
    - [ ] Implement plasma with polygon distortion
    - [ ] Test: Plasma animates correctly
  - [ ] Implement Plasma Cube variant
    - [ ] Reuse plasma renderer with 3D mapping
    - [ ] Implement cube rotation and scaling
    - [ ] Test: Plasma-textured cube rotates

- [ ] Phase 6: Ending Sequence (Parts 22-30)
  - [ ] Port ENDPIC (End Picture)
    - [ ] Port BEG.C image loader
    - [ ] Load final image assets
    - [ ] Test: End picture displays
  - [ ] Port ENDSCRL (Credits Scroll)
    - [ ] Port MAIN.C scroller
    - [ ] Load font (FONA.INC)
    - [ ] Parse ENDSCROL.TXT
    - [ ] Implement smooth vertical scrolling
    - [ ] Test: Credits scroll smoothly
  - [ ] Port CREDITS (Detailed Credits)
    - [ ] Port MAIN.C controller
    - [ ] Load crew member images
    - [ ] Implement credit sequence
    - [ ] Test: Full credits with pictures
  - [ ] Port START (Setup Menu)
    - [ ] Port menu system
    - [ ] Implement configuration options
    - [ ] Test: Menu navigates correctly
  - [ ] Port END (Exit Screen)
    - [ ] Port END.C
    - [ ] Implement loop/exit option
    - [ ] Test: Demo ends or loops correctly

- [ ] Phase 7: Integration & Polish
  - [ ] Full demo sequencing
    - [ ] Implement all part transitions
    - [ ] Sync all parts to music timing
    - [ ] Test complete playthrough
  - [ ] Audio sync verification
    - [ ] Verify all music sync points
    - [ ] Adjust timing as needed
  - [ ] Browser testing
    - [ ] Test WASM build in Chrome
    - [ ] Test WASM build in Firefox
    - [ ] Test WASM build in Safari
    - [ ] Optimize for web performance
  - [ ] Native platform testing
    - [ ] Test on Linux
    - [ ] Test on macOS
    - [ ] Test on Windows

## Completed

(None yet)

## References

- [Original README](../README.md)
- [SCRIPT](../SCRIPT) - Demo sequence and timing
- [DESIGN](../DESIGN) - Creative design document
- [CODE](../CODE) - Original coding guidelines
- [Sokol](https://github.com/floooh/sokol) - Cross-platform graphics
- [libopenmpt](https://lib.openmpt.org/libopenmpt/) - S3M/MOD playback
