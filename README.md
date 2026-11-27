# dgk: the DOS-GL Kit

The game-agnostic layer under DOS-GL games: the main loop, the platform,
drawing within DOS-GL's OpenGL 1.1 subset, text, a mixer, packs, saves,
replays, DOSBench records and the Loop A test hooks. One source builds for
DOS (DJGPP, SDL3 with DOS-GL's bridge, plain DOS or GLOS), Linux (SDL3,
desktop GL) and headless tests (OSMesa, a virtual clock).

It grew inside [Fifth Wheel](https://github.com/ranulphus/FifthWheel) as `kit/` and was split out,
with its history, when a second game (Echoes of the Void, `~/Frontier`)
needed it. Each game builds it from the checkout beside it (`~/dgk`), on main
as it is, as it does RetroGPU (DOS-GL, the HAL): no pins (the owner's decision,
2026-11-02: before alpha everything is on main trunk).

## Modules

| Header | What |
|---|---|
| `dgk/app.h` | the loop: fixed 60 Hz ticks, a draw with the tick fraction, the swap; keys, the mouse (pointer or relative), typed text and the first joystick; the command line every game shares |
| `dgk/base.h` | integer types, arenas, CRC-32, FNV-1a, PCG32 |
| `dgk/gfx.h` | camera matrices, vertex-coloured meshes, the 640x480 overlay, triangle and draw counts |
| `dgk/vec.h` | float and double 3-vectors; doubles for positions a float can't hold, drawn relative to the camera |
| `dgk/text.h` | bitmap text from an atlas baked by `tools/fontbake.py` |
| `dgk/mix.h` | an integer mixer: 12 mono voices, volume and pitch |
| `dgk/midi.h` | music as General MIDI messages, wherever the game runs: the FM chip (GLOS's driver, dgk's instruments), GLOS's MIDI ports, an MPU-401, an emulated OPL3 off DOS |
| `dgk/net.h` | the network, wherever the game runs: ENet for the game's links and UDP for finding games on the LAN, on GLOS's sockets, the host's, or none on plain DOS; what there is and why not |
| `dgk/pak.h` | packs: named, CRC-checked sections loaded whole (`tools/pakw.c` writes them) |
| `dgk/path.h` | a file's path on this platform: the game's data and what it writes, named once as DOS would |
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

A game's `check-deps` checks only that the checkout is there; after a change
here, each game's quick regression. Builds that run in the RetroGPU dev
container mount the checkout with `MGA_EXTRA_DIR=$(DGK)`.

## Music

`dgk/midi.h` (`$(DGK_MIDI)`) needs a library that `libs.mk` builds into the
game's build directory: `dos` (GLOS's FM driver with dgk's instruments and
GLOS's sdk/fm in `libdgkmidi.a`, GLOS's own library in `libdgkglos.a`, both
compiled from GLOS's checkout beside this one: `GLOS`, no copies; link order
under Network), or `stage` then `host` (the same driver and instruments
on ymfm's OPL3; staged because the dev container sees no GLOS checkout; link
with `-lstdc++`). On DOS the music's file wants `$(DGK_MIDI_DOS_CFLAGS)`.
No port is probed on a guess: the FM chip only when BLASTER is set, an
MPU-401 only at BLASTER's P.

The instruments (`src/fmbank.c`, from `tools/fmbank.py`: `make fmbank`) are
Fifth Wheel's, extended to all 128 General MIDI programs and the 47 drums,
in GENMIDI's layout for GLOS's driver, which reads them in place of its
Freedoom bank. `make midplay` builds MIDPLAY for the host (a MIDI file into
a WAV; `-sweep`: every instrument's level, pitch and brightness), `make
midplay-dos` for DOS (a MIDI file live on the chip).

## Network

`dgk/net.h` (`$(DGK_NET)`, compiled with `$(DGK_NET_CFLAGS)`, on DOS
`$(DGK_NET_DOS_CFLAGS)` too) and `libs.mk`'s `libdgknet.a`: ENet 1.3.18
(`third_party/enet`, unchanged) for a game's links, on GLOS's sockets on DOS
(`src/enet_glos.c` in place of ENet's `unix.c`) and the host's elsewhere, and
small UDP calls (`dgk_udp_*`) for finding games on the LAN, so a game needs
no socket headers. `dgk_net_status()` says what there is:

- `DGK_NET_OK`: ENet and UDP work (Linux, headless; GLOS 2.2 or later with
  its UDP, as an app or, from GLOS 2.3, full screen);
- `DGK_NET_NONE`: plain DOS. No packet driver is looked for and no port or
  interrupt is touched: nothing on a guess, as with the music;
- `DGK_NET_OLD`: GLOS before 2.2 (no UDP);
- `DGK_NET_WINDOW`: GLOS 2.2 full screen (its sockets are an app's).

Each game words these for its players. On DOS a wait on the network
(`dgk_net_service`, `dgk_udp_recv` with a time) goes in slices of 4 ms with
dgk serviced between: under GLOS a socket call that waits holds the
program's own interrupt handlers (the timer's, the keyboard's, the sound's)
until it returns.

On DOS a game links `libs.mk`'s libraries in this order: `libdgkmidi.a
libdgknet.a libdgkglos.a`. GLOS's own library (`sdk/libglos`'s `glos.c` and
`socket.c`, from GLOS's checkout) is the last, shared by the two; the linker
takes only what is used, so a game without a network carries none of it.

## Checks

`make test` runs the main loop on a fake platform (no SDL, no GL) and
checks its input tick by tick, the vectors, the music (a General MIDI
flute's A through GLOS's driver, dgk's instruments and ymfm), and the
network on loopback (UDP both ways; ENet's server and client connecting
through `dgk_net_service`), with no LAN needed. The games' own suites
(Fifth Wheel's `make regress`, Echoes' EV11 and its GLOS steps, Loop A)
cover the platforms.

The mouse wheel needs a driver that reports it: SDL's on Linux does; DOS's
INT 33h driver and GLOS's pointer don't yet.

## Licence

MIT (`LICENSE`). `third_party/ymfm`: BSD-3-Clause (its `LICENSE`), in the
host builds only. `third_party/enet`: MIT (its `LICENSE`; `README.md` there
says where it came from). GLOS's driver and library, built from GLOS's
checkout: MIT.
