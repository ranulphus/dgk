# libs.mk - the libraries behind dgk/midi.h (src/midi.c) and dgk/net.h (src/net.c), built into a game's build
# directory:
#
#   make -f $(DGK)/libs.mk OUT=DIR GLOS=DIR DJCC="..." DJAR="..." dos
#       OUT/dos/libdgkmidi.a: GLOS's FM driver playing dgk's own General MIDI instruments and loudness curve
#       (src/fmbank.c, src/fmtab.c, made by tools/fmbank.py, in place of GLOS's Freedoom bank and its tables)
#       and sdk/fm/fmsynth.c (the chip found and written), compiled from GLOS's checkout
#       OUT/dos/libdgknet.a: ENet (third_party/enet) on GLOS's sockets (src/enet_glos.c in place of its unix.c)
#       OUT/dos/libdgkglos.a: GLOS's own library, sdk/libglos's glos.c and socket.c (the API, GLOS's MIDI ports,
#       glos_running and glos_is_app; the BSD socket calls), shared by the two above
#       A game links them in that order: libdgkmidi.a libdgknet.a libdgkglos.a (the linker takes only what is
#       used: a game without a network carries none of socket.c)
#   make -f $(DGK)/libs.mk OUT=DIR GLOS=DIR stage
#       OUT/glos: the seven of GLOS's files the host music library needs, copied, for a build inside RetroGPU's
#       dev container (which sees the game's checkout and dgk's, not GLOS's)
#   make -f $(DGK)/libs.mk OUT=DIR GLOS=DIR host
#       OUT/host/libdgkmidi.a: GLOS's FM driver, dgk's instruments, ymfm's OPL3 (third_party/ymfm) and
#       src/midi_ymfm.cpp; link it with -lstdc++
#       OUT/host/libdgknet.a: ENet with its own unix.c (the host's sockets)
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
ENET_CORE := $(filter-out $(DGK)/third_party/enet/unix.c,$(wildcard $(DGK)/third_party/enet/*.c))
ENET_HDRS := $(wildcard $(DGK)/third_party/enet/include/enet/*.h)
GLOS_LIB  := sdk/libglos/glos.c sdk/libglos/socket.c
MIDI_OBJS := $(notdir $(FM_SRCS:.c=.o) $(BANK:.c=.o)) fmsynth.o
dos: $(OUT)/dos/libdgkmidi.a $(OUT)/dos/libdgknet.a $(OUT)/dos/libdgkglos.a
$(OUT)/dos/libdgkmidi.a: $(addprefix $(GLOS)/,$(FM_SRCS) $(FM_HDRS) sdk/fm/fmsynth.c include/glos/fmsynth.h) \
                         $(BANK) $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OUT)/dos/obj
	$(Q)echo "  DJCC    GLOS's FM driver, dgk's bank and sdk/fm (dgk/midi.h)"
	$(Q)set -e; for f in $(FM_SRCS); do $(DJCC) $(call fm_flags,$(GLOS)) -march=i586 -w \
	  -c -o $(OUT)/dos/obj/$$(basename $$f .c).o $(GLOS)/$$f; done
	$(Q)set -e; for f in $(BANK); do $(DJCC) $(call fm_flags,$(GLOS)) -march=i586 -w \
	  -c -o $(OUT)/dos/obj/$$(basename $$f .c).o $$f; done
	$(Q)$(DJCC) -O2 -march=i586 -w -I$(GLOS)/include -c -o $(OUT)/dos/obj/fmsynth.o $(GLOS)/sdk/fm/fmsynth.c
	$(Q)rm -f $@ && $(DJAR) rcs $@ $(addprefix $(OUT)/dos/obj/,$(MIDI_OBJS))
$(OUT)/dos/libdgknet.a: $(ENET_CORE) $(ENET_HDRS) $(DGK)/src/enet_glos.c $(GLOS)/include/glos/net.h \
                        $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OUT)/dos/net
	$(Q)echo "  DJCC    ENet on GLOS's sockets (dgk/net.h)"
	$(Q)set -e; for f in $(ENET_CORE) $(DGK)/src/enet_glos.c; do $(DJCC) -std=gnu99 -O2 -march=i586 -w \
	  -I$(DGK)/third_party/enet/include -I$(GLOS)/include -I$(GLOS)/include/glos/posix \
	  -c -o $(OUT)/dos/net/$$(basename $$f .c).o $$f; done
	$(Q)rm -f $@ && $(DJAR) rcs $@ $(OUT)/dos/net/*.o
$(OUT)/dos/libdgkglos.a: $(addprefix $(GLOS)/,$(GLOS_LIB) include/glos/glos.h include/glos/api.h include/glos/net.h) \
                         $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OUT)/dos/glos
	$(Q)echo "  DJCC    libglos (GLOS's sdk/libglos: dgk/midi.h's and dgk/net.h's)"
	$(Q)set -e; for f in $(GLOS_LIB); do $(DJCC) -std=gnu99 -O2 -march=i586 -w -I$(GLOS)/include \
	  -c -o $(OUT)/dos/glos/$$(basename $$f .c).o $(GLOS)/$$f; done
	$(Q)rm -f $@ && $(DJAR) rcs $@ $(OUT)/dos/glos/*.o

# ---- Host (Linux, headless) -----------------------------------------------------------------------------------
stage: $(OUT)/glos/stamp
$(OUT)/glos/stamp: $(addprefix $(GLOS)/,$(FM_SRCS) $(FM_HDRS))
	$(Q)set -e; for f in $(FM_SRCS) $(FM_HDRS); do mkdir -p $(OUT)/glos/$$(dirname $$f); \
	  cp $(GLOS)/$$f $(OUT)/glos/$$f; done
	$(Q)mkdir -p $(OUT)/glos/include && touch $@

host: $(OUT)/host/libdgkmidi.a $(OUT)/host/libdgknet.a
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
# ENet with its own unix.c, on the host's sockets.
ENET_HOST_FLAGS := -std=gnu99 -O2 -fPIC -w -I$(DGK)/third_party/enet/include -DHAS_SOCKLEN_T=1 -DHAS_FCNTL=1 \
                   -DHAS_POLL=1 -DHAS_GETADDRINFO=1 -DHAS_GETNAMEINFO=1 -DHAS_INET_PTON=1 -DHAS_INET_NTOP=1 \
                   -DHAS_MSGHDR_FLAGS=1
$(OUT)/host/libdgknet.a: $(wildcard $(DGK)/third_party/enet/*.c) $(ENET_HDRS) $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OUT)/host/net
	$(Q)echo "  CC      ENet (dgk/net.h)"
	$(Q)set -e; for f in $(wildcard $(DGK)/third_party/enet/*.c); do $(HOST_CC) $(ENET_HOST_FLAGS) \
	  -c -o $(OUT)/host/net/$$(basename $$f .c).o $$f; done
	$(Q)rm -f $@ && $(HOST_AR) rcs $@ $(OUT)/host/net/*.o
