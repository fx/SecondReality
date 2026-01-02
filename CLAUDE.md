# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Second Reality is a legendary PC demo by Future Crew, released at Assembly '93. This repository contains the original source code and data files, released to the public domain in 2013.

**Current Project Goal:** Cross-platform refactor using Sokol + Emscripten to run natively and in browsers while preserving the original code/directory structure. See `docs/PROJECT.md` for the detailed implementation plan.

## Original Architecture

### Demo Interrupt Server (DIS)

The demo uses a central interrupt server (`DIS/`) that all parts communicate through:

```c
dis_partstart();           // Initialize part (call first)
dis_waitb();               // Wait for vertical blank (frame sync)
dis_exit();                // Returns 1 if user pressed key to exit
dis_indemo();              // Returns 1 if running inside demo, 0 if standalone
dis_muscode(code);         // Music synchronization
dis_setcopper(num, func);  // Set raster interrupt routine (1=top, 2=bottom, 3=retrace)
dis_msgarea(0-3);          // Inter-part communication (64 bytes each)
```

### Part Structure

Each demo part is in its own directory (ALKU, BEG, GLENZ, etc.) and follows this pattern:
1. Call `dis_partstart()` or `dis_version()` on init
2. Main loop: render frame, call `dis_waitb()`, check `dis_exit()`
3. Parts compile to standalone EXE files that work with DIS loaded in background

### Key Directories

| Directory | Purpose |
|-----------|---------|
| `MAIN/` | Loader, packer, music files (MUSIC0.S3M, MUSIC1.S3M) |
| `DIS/` | Demo Interrupt Server - shared timing/sync API |
| `VISU/` | 3D visualization library (100KB), vector math, polygon rendering |
| `UTIL/` | Tools: LBM2P (image converter), DOOBJ (data→object converter) |
| `GRAB/` | Graphics utilities, FLIC decoder, font system |

### Languages

- **x86 Assembly** (TASM) - ~70% of performance-critical code
- **Turbo C** - Application logic
- **Turbo Pascal** - FOREST, TUNNELI parts

### Graphics Modes

- 320x200 VGA Mode 13h (most parts)
- 320x400 MCGA tweaked mode
- EGA modes for specific effects

### Data Files

- `.LBM` - Deluxe Paint images
- `.S3M` - ScreamTracker 3 music modules
- `.INC` - Precalculated lookup tables (can be 100KB-300KB)
- `.FLI` - Autodesk Flic animations

## Cross-Platform Refactor (In Progress)

### New Structure

```
src/
├── core/      # Sokol app, DIS replacement, video subsystem
├── audio/     # libopenmpt for S3M playback
└── parts/     # Ported scene code (mirrors original dirs)
```

### Tech Stack

- **Sokol** - Cross-platform graphics (sokol_app.h, sokol_gfx.h)
- **libopenmpt** - S3M/MOD music playback
- **Emscripten** - WebAssembly compilation
- **CMake** - Build system

### Memory Constraint

Original parts were limited to ~450KB. The refactor maintains similar frugality for web builds.

## Demo Sequence

The demo flow is defined in `SCRIPT`. Parts execute in order:
ALKU → BEG → 3DS → PANIC → FCP → GLENZ → DOTS → GRID → TECHNO → HARD → COMAN → WATER → FOREST → TUNNELI → TWIST → PAM → JPLOGO → LENS → DDSTARS → PLZPART → ENDPIC → ENDSCRL → CREDITS → START → END

Each part syncs to music via `dis_muscode()` calls.

## Running the Demo

### Native Linux Build

**Required packages:**
```bash
sudo apt-get install -y libopenmpt-dev libasound2-dev \
    libx11-dev libxi-dev libxcursor-dev libgl-dev \
    mesa-utils xdotool ffmpeg
```

**Build:**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

**Run (requires X display and software rendering for headless/Xvfb):**
```bash
DISPLAY=:1 LIBGL_ALWAYS_SOFTWARE=1 ./build/src/SecondReality
```

**CLI arguments:**
```
-p, --part <N>    Start from part N (0=ALKU, 1=TEST1, 2=TEST2)
-s, --single      Exit after first part completes
-l, --list        List available parts
-h, --help        Show help
```

**Example - run only TEST_PART_1:**
```bash
DISPLAY=:1 LIBGL_ALWAYS_SOFTWARE=1 ./build/src/SecondReality -p 1 -s
```

### Technical Notes

- **OpenGL 3.3**: Required for Mesa software rendering compatibility (set in `sokol_main()`)
- **VSync**: Enabled via `swap_interval = 1` for proper 60fps timing
- **GLX vs EGL**: Use GLX (not EGL) for Xvfb compatibility

### Capturing Video Output

**Record demo to video:**
```bash
# Start recording (in background)
DISPLAY=:1 ffmpeg -f x11grab -video_size 640x400 -framerate 30 -i :1 -t 10 /tmp/demo.mp4 &

# Run demo
DISPLAY=:1 LIBGL_ALWAYS_SOFTWARE=1 ./build/src/SecondReality
```

**Extract frames for analysis:**
```bash
mkdir -p /tmp/frames
ffmpeg -i /tmp/demo.mp4 -vf "fps=2" /tmp/frames/f_%03d.png

# Non-black frames typically >7KB (black/empty frames ~1.6KB)
# Frame N at 2fps = (N-1)/2 seconds into video
find /tmp/frames -name "*.png" -size +7k
```

### Comparing with Reference Video

Reference video is at `docs/reference.mp4`. Always compare both audio and video timing.

**Extract reference frames:**
```bash
mkdir -p /tmp/ref_frames
ffmpeg -i docs/reference.mp4 -t 60 -vf "fps=2" /tmp/ref_frames/ref_%03d.png
```

**Compare timing (find first non-black frame):**
```bash
# Frame N at 2fps = (N-1)/2 seconds
for i in $(seq -w 1 40); do
    size=$(stat -c%s "/tmp/ref_frames/ref_0$i.png" 2>/dev/null)
    echo "Frame $i: $size bytes"
done | grep -v ": 383 bytes"  # Filter out black frames
```

**Key timing points from reference (docs/reference.mp4):**
- 16.0s: "A Future Crew Production" fades in
- 24.0s: "First Presented at Assembly 93"
- 31.0s: "in Second Reality" / Dolby logo
- 38.0s: Horizon scene begins

### WASM/Browser Build

```bash
# Configure with Emscripten
emcmake cmake -B build-wasm -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build-wasm

# Serve locally
cd build-wasm/src && python3 -m http.server 8080
# Open http://localhost:8080/SecondReality.html
```
