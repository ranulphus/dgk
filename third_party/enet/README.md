# ENet (third party)

The reliable UDP library behind dgk/net.h (src/net.c): Lee Salzman's ENet 1.3.18, its library sources and
headers unchanged, all but `win32.c` (no Windows build). Its platform layer on DOS is dgk's own
`src/enet_glos.c` (adapted from this `unix.c`, on GLOS's sockets); Linux and headless builds use `unix.c` as it
is.

- From: http://enet.bespin.org/download/enet-1.3.18.tar.gz (ENet 1.3.18, 14 April 2024; archive sha256
  2a8a0c5360d68bb4fcd11f2e4c47c69976e8d2c85b109dd7d60b1181a4f85d36), the 20 files here compared byte for byte
  with that archive on 2026-11-26 (they came to dgk from Echoes of the Void's `third_party/enet`, vendored at its
  EV11 in November 2026).
- Licence: MIT, Copyright (c) 2002-2024 Lee Salzman (`LICENSE` here).
