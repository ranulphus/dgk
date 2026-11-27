# dgk.mk - include from a game's Makefile, with DGK set to this checkout:
#
#   DGK ?= $(HOME)/dgk
#   include $(DGK)/dgk.mk
#
# then compile $(DGK_SRCS) plus one platform ($(DGK_SDL) for DOS and Linux,
# $(DGK_HEADLESS) for OSMesa tests) with $(DGK_CFLAGS), against DOS-GL's
# own <GL/gl.h> on every target. A game that builds inside the RetroGPU dev
# container mounts this checkout there with MGA_EXTRA_DIR=$(DGK).

DGK_SRCS     := $(addprefix $(DGK)/src/,base.c cfg.c log.c app.c gfx.c text.c mix.c test.c pak.c replay.c \
                  bench.c save.c path.c)
DGK_SDL      := $(DGK)/src/plat_sdl.c
DGK_HEADLESS := $(DGK)/src/plat_headless.c
DGK_HDRS     := $(wildcard $(DGK)/include/dgk/*.h $(DGK)/src/*.h)
DGK_CFLAGS   := -I$(DGK)/include -I$(DGK)/src
# Music (dgk/midi.h): $(DGK_MIDI) with the rest, and a library of libs.mk's (GLOS's FM driver; on DOS with
# GLOS's own library, off DOS ymfm): $(MAKE) -f $(DGK)/libs.mk OUT=... GLOS=$(GLOS) dos|stage|host. GLOS is the
# checkout beside this one; on DOS the music's file needs its headers ($(DGK_MIDI_DOS_CFLAGS)).
GLOS         ?= $(abspath $(DGK)/../GLOS)
DGK_MIDI     := $(DGK)/src/midi.c
DGK_MIDI_DOS_CFLAGS := -I$(GLOS)/include
DGK_LIBS_MK  := $(DGK)/libs.mk
# The network (dgk/net.h): $(DGK_NET) with the rest, compiled with $(DGK_NET_CFLAGS) (ENet's headers; on DOS
# $(DGK_NET_DOS_CFLAGS) too, GLOS's), and libs.mk's libdgknet.a (ENet: on DOS on GLOS's sockets, off DOS its own
# unix.c). On DOS link libs.mk's libraries in this order: libdgkmidi.a libdgknet.a libdgkglos.a (GLOS's own
# library last, shared by the two: since dgk/net.h, libdgkmidi.a no longer carries glos.c).
DGK_NET      := $(DGK)/src/net.c
DGK_NET_CFLAGS := -I$(DGK)/third_party/enet/include
DGK_NET_DOS_CFLAGS := -I$(GLOS)/include -I$(GLOS)/include/glos/posix
# Host tools: packs (tools/pakw.c with src/base.c) and font atlases.
DGK_PAKW     := $(DGK)/tools/pakw.c
DGK_FONTBAKE := $(DGK)/tools/fontbake.py
