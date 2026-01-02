# ALKU Port Status

## Overview
ALKU is the opening credits sequence of Second Reality. It displays intro text screens synced to music, followed by scrolling credits over a horizon image.

## Current State: In Progress

### What Works
- Basic part structure (init, update, render, cleanup)
- Text rendering with `prtc()` function
- Palette fades (`dofade()` - instant, not 64-frame like original)
- Phase state machine for intro sequence
- Music playback via libopenmpt (subsong 4 selected)
- Frame-based sync timing in `dis_sync()`

### Sync Timing (dis.c)
The S3M has pattern jump commands (Bxx) in orders 0-3 that break sequential playback. Solution: use elapsed frame time instead of music position.

```
sync 0: 0-15s   (frames 0-900)    - intro music, black screen
sync 1: 15-20s  (frames 900-1200) - "Future Crew Production"
sync 2: 20-25s  (frames 1200-1500) - "Assembly 93"
sync 3: 25-30s  (frames 1500-1800) - "Second Reality"
sync 4+: 30s+   (frames 1800+)    - credits/horizon
```

### Known Issues
1. **Fades are instant** - Original `dofade()` took 64 frames; ported version does all 64 steps in 1 frame (no `dis_waitb()` in loop)
2. **Font not loaded** - Need to load ALKU font from original data files
3. **Horizon image** - Need to load and display the background image
4. **Credits scroll** - Basic structure exists but needs testing

### Key Files
- `/workspace/src/parts/alku/alku.c` - Main ALKU port
- `/workspace/src/core/dis.c` - DIS sync implementation
- `/workspace/src/audio/music.c` - Music subsystem
- `/workspace/ALKU/MAIN.C` - Original source reference

### Original DIS ordersync1 Table (for reference)
```
order 0        -> sync 0 (intro)
order 2        -> sync 1 (Future Crew)
order 3        -> sync 2 (Assembly 93)
order 3 row 47 -> sync 3 (Second Reality)
order 4 row 47 -> sync 4 (Graphics credits)
order 5 row 47 -> sync 5 (Music credits)
order 6 row 47 -> sync 6 (Code credits)
order 7 row 47 -> sync 7 (Additional credits)
order 8 row 47 -> sync 8 (Exit)
```

### Next Steps
1. Test current sync timing visually
2. Fix fade timing if needed (make fades gradual)
3. Load actual font and horizon graphics
4. Verify credits phase works correctly
