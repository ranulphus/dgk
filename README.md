# dgk: the DOS-GL Kit

The game-agnostic layer under DOS-GL games: the main loop, the platform,
drawing within DOS-GL's OpenGL 1.1 subset, text, a mixer, packs, saves,
replays, DOSBench records and the Loop A test hooks. One source builds for
DOS (DJGPP, SDL3 with DOS-GL's bridge, plain DOS or GLOS), Linux (SDL3,
desktop GL) and headless tests (OSMesa, a virtual clock).

It grew inside [Fifth Wheel](https://github.com/ranulphus/FifthWheel) as `kit/` and was split out,
with its history, when a second game (Echoes of the Void, `~/Frontier`)
needed it. Each game pins a commit of this repository in its `deps.mk`, as
it pins DOSGL.

## Modules

| Header | What |
|---|---|
| `dgk/app.h` | the loop: fixed 60 Hz ticks, a draw with the tick fraction, the swap; keys, the mouse (pointer or relative), typed text and the first joystick; the command line every game shares |
| `dgk/base.h` | integer types, arenas, CRC-32, FNV-1a, PCG32 |
| `dgk/gfx.h` | camera matrices, vertex-coloured meshes, the 640x480 overlay, triangle and draw counts |
| `dgk/vec.h` | float and double 3-vectors; doubles for positions a float can't hold, drawn relative to the camera |
| `dgk/text.h` | bitmap text from an atlas baked by `tools/fontbake.py` |
| `dgk/mix.h` | an integer mixer: 12 mono voices, volume and pitch |
| `dgk/pak.h` | packs: named, CRC-checked sections loaded whole (`tools/pakw.c` writes them) |
| `dgk/save.h`, `dgk/cfg.h` | saves and settings files, replaced atomically (DOS's `rename` does not overwrite) |
| `dgk/replay.h` | recorded input per tick and a state hash a second |
| `dgk/bench.h` | DOSBench records |
| `dgk/test.h`, `dgk/log.h` | HX- test lines on COM1 for Loop A; log lines |

`src/plat_sdl.c` and `src/plat_headless.c` are the two platforms behind
`src/plat.h`.

## Using it

```make
DGK ?= $(HOME)/dgk
include $(DGK)/dgk.mk
# compile $(DGK_SRCS) and $(DGK_SDL) (or $(DGK_HEADLESS)) with $(DGK_CFLAGS)
```

A game's `deps.mk` holds `DGK_PIN`; its `check-deps` refuses a dgk older
than the pin and says when dgk has moved past it. Builds that run in the
MGA-Glide dev container mount the checkout with `MGA_EXTRA_DIR=$(DGK)`.

## Checks

`make test` runs the main loop on a fake platform (no SDL, no GL) and
checks its input tick by tick, and the vectors. The games' own suites
(Fifth Wheel's `make regress`, Loop A) cover the platforms.

The mouse wheel needs a driver that reports it: SDL's on Linux does; DOS's
INT 33h driver and GLOS's pointer don't yet.

## Licence

MIT (`LICENSE`).
