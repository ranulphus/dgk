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
# Music (dgk/midi.h): $(DGK_MIDI) with the rest, and a library of libs.mk's (GLOS's FM driver; on DOS GLOS's
# own library too, off DOS ymfm): $(MAKE) -f $(DGK)/libs.mk OUT=... GLOS=$(GLOS) dos|stage|host. GLOS is the
# checkout beside this one; on DOS the music's file needs its headers ($(DGK_MIDI_DOS_CFLAGS)).
GLOS         ?= $(abspath $(DGK)/../GLOS)
DGK_MIDI     := $(DGK)/src/midi.c
DGK_MIDI_DOS_CFLAGS := -I$(GLOS)/include
DGK_LIBS_MK  := $(DGK)/libs.mk
# Host tools: packs (tools/pakw.c with src/base.c) and font atlases.
DGK_PAKW     := $(DGK)/tools/pakw.c
DGK_FONTBAKE := $(DGK)/tools/fontbake.py
