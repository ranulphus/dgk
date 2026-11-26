# libs.mk - the libraries behind dgk/midi.h (src/midi.c), built into a game's build directory:
#
#   make -f $(DGK)/libs.mk OUT=DIR GLOS=DIR DJCC="..." DJAR="..." dos
#       OUT/dos/libdgkmidi.a: GLOS's FM driver playing dgk's own General MIDI instruments and loudness curve
#       (src/fmbank.c, src/fmtab.c, made by tools/fmbank.py, in place of GLOS's Freedoom bank and its tables),
#       sdk/fm/fmsynth.c (the chip found and written), and sdk/libglos/glos.c (GLOS's MIDI ports;
#       glos_running, glos_is_app), compiled from GLOS's checkout
#   make -f $(DGK)/libs.mk OUT=DIR GLOS=DIR stage
#       OUT/glos: the seven of GLOS's files the host library needs, copied, for a build inside RetroGPU's dev
#       container (which sees the game's checkout and dgk's, not GLOS's)
#   make -f $(DGK)/libs.mk OUT=DIR GLOS=DIR host
#       OUT/host/libdgkmidi.a: GLOS's FM driver, dgk's instruments, ymfm's OPL3 (third_party/ymfm) and
#       src/midi_ymfm.cpp; link it with -lstdc++
#
# GLOS's driver files are the kernel's own, built freestanding with its headers, as GLOS builds sdk/fm
# (include/glos/fmsynth.h says how); dgk's bank is built the same way.
DGK := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
OUT ?= build/dgk
GLOS ?= $(abspath $(DGK)/../GLOS)
HOST_CC ?= gcc
HOST_CXX ?= g++
HOST_AR ?= ar
Q ?= @

FM_SRCS := kernel/drv/fm.c kernel/drv/midiq.c sdk/fm/fmq.c
BANK    := $(DGK)/src/fmbank.c $(DGK)/src/fmtab.c
FM_HDRS := kernel/include/fm.h kernel/include/midiq.h kernel/include/types.h kernel/include/libc/string.h
fm_flags = -O2 -ffreestanding -I$(1)/include -I$(1)/kernel/include -I$(1)/kernel/include/libc
YMFM := ymfm_opl ymfm_adpcm ymfm_pcm

.PHONY: dos host stage

# ---- DOS ------------------------------------------------------------------------------------------------------
dos: $(OUT)/dos/libdgkmidi.a
$(OUT)/dos/libdgkmidi.a: $(addprefix $(GLOS)/,$(FM_SRCS) $(FM_HDRS) sdk/fm/fmsynth.c include/glos/fmsynth.h \
                           sdk/libglos/glos.c include/glos/glos.h) $(BANK) $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OUT)/dos/obj
	$(Q)echo "  DJCC    GLOS's FM driver, dgk's bank, sdk/fm and libglos (dgk/midi.h)"
	$(Q)set -e; for f in $(FM_SRCS); do $(DJCC) $(call fm_flags,$(GLOS)) -march=i586 -w \
	  -c -o $(OUT)/dos/obj/$$(basename $$f .c).o $(GLOS)/$$f; done
	$(Q)set -e; for f in $(BANK); do $(DJCC) $(call fm_flags,$(GLOS)) -march=i586 -w \
	  -c -o $(OUT)/dos/obj/$$(basename $$f .c).o $$f; done
	$(Q)$(DJCC) -O2 -march=i586 -w -I$(GLOS)/include -c -o $(OUT)/dos/obj/fmsynth.o $(GLOS)/sdk/fm/fmsynth.c
	$(Q)$(DJCC) -O2 -march=i586 -w -I$(GLOS)/include -c -o $(OUT)/dos/obj/glos.o $(GLOS)/sdk/libglos/glos.c
	$(Q)rm -f $@ && $(DJAR) rcs $@ $(OUT)/dos/obj/*.o

# ---- Host (Linux, headless) -----------------------------------------------------------------------------------
stage: $(OUT)/glos/stamp
$(OUT)/glos/stamp: $(addprefix $(GLOS)/,$(FM_SRCS) $(FM_HDRS))
	$(Q)set -e; for f in $(FM_SRCS) $(FM_HDRS); do mkdir -p $(OUT)/glos/$$(dirname $$f); \
	  cp $(GLOS)/$$f $(OUT)/glos/$$f; done
	$(Q)mkdir -p $(OUT)/glos/include && touch $@

host: $(OUT)/host/libdgkmidi.a
$(OUT)/host/libdgkmidi.a: $(addprefix $(GLOS)/,$(FM_SRCS) $(FM_HDRS)) $(BANK) $(DGK)/src/midi_ymfm.cpp \
                          $(wildcard $(DGK)/third_party/ymfm/*) $(DGK)/include/dgk/mix.h $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OUT)/host/obj
	$(Q)echo "  CC      GLOS's FM driver, dgk's bank, ymfm (dgk/midi.h)"
	$(Q)set -e; for f in $(FM_SRCS); do $(HOST_CC) $(call fm_flags,$(GLOS)) -fPIC -w \
	  -c -o $(OUT)/host/obj/$$(basename $$f .c).o $(GLOS)/$$f; done
	$(Q)set -e; for f in $(BANK); do $(HOST_CC) $(call fm_flags,$(GLOS)) -fPIC -w \
	  -c -o $(OUT)/host/obj/$$(basename $$f .c).o $$f; done
	$(Q)set -e; for f in $(YMFM); do $(HOST_CXX) -std=c++14 -O2 -fPIC -w -c -o $(OUT)/host/obj/$$f.o \
	  $(DGK)/third_party/ymfm/$$f.cpp; done
	$(Q)$(HOST_CXX) -std=c++14 -O2 -fPIC -Wall -Wextra -Werror -I$(DGK)/include -isystem $(DGK)/third_party/ymfm \
	  -c -o $(OUT)/host/obj/midi_ymfm.o $(DGK)/src/midi_ymfm.cpp
	$(Q)rm -f $@ && $(HOST_AR) rcs $@ $(OUT)/host/obj/*.o
