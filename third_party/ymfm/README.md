# ymfm (third party)

The OPL3 (YMF262) emulation behind dgk/midi.h on Linux and headless builds (src/midi_ymfm.cpp): Aaron Giles'
ymfm, the nine files an OPL3 needs (the OPL family's file also holds the OPL4, hence the PCM and ADPCM files),
unchanged.

- From: https://github.com/aaronsgiles/ymfm, commit 81aec25ccbb98f4873a255f7551ac4dadac59b4a (2026-07-27),
  `src/` (archive sha256 9bcf001b30f53754c49cf2fb864792a64702b8aeebb105baa908ad5dd33f443f), fetched 2026-11-26.
- Licence: BSD-3-Clause, Copyright (c) 2021, Aaron Giles (LICENSE here, and each file's header). Only in the
  host builds: the DOS builds play a real chip.
