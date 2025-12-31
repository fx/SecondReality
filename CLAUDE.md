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
