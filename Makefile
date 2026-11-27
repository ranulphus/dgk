# dgk's own checks. Games build the kit themselves (dgk.mk).
#
#   make test     the main loop on a fake platform, and the vectors (host compiler; no screen)
RETROGPU ?= $(HOME)/RetroGPU
DOSGL := $(RETROGPU)/gl
HOST_CC ?= gcc
Q ?= @
WARN := -std=gnu99 -Wall -Wextra -Werror

GLOS ?= $(HOME)/GLOS
TEST_SRCS := tests/test_dgk.c src/app.c src/base.c src/log.c src/mix.c src/test.c src/midi.c src/path.c src/net.c

# MIDPLAY (tools/midplay.c): a MIDI file through dgk/midi.h; on the host into a WAV (and -sweep, every
# instrument measured), on DOS live (build/dos/MIDPLAY.EXE).
DJGPP_PREFIX ?= $(HOME)/.local/opt/djgpp-gcc1220
DJCC := env LD_LIBRARY_PATH=$(DJGPP_PREFIX)/hostlib $(DJGPP_PREFIX)/bin/i586-pc-msdosdjgpp-gcc
DJAR := env LD_LIBRARY_PATH=$(DJGPP_PREFIX)/hostlib $(DJGPP_PREFIX)/bin/i586-pc-msdosdjgpp-ar
MIDPLAY_SRCS := tools/midplay.c src/midi.c src/log.c src/base.c
build/midplay: $(MIDPLAY_SRCS) src/mix.c $(wildcard include/dgk/*.h) build/dgk/host/libdgkmidi.a
	$(Q)$(HOST_CC) $(WARN) -O2 -Iinclude -Isrc -o $@ $(MIDPLAY_SRCS) src/mix.c build/dgk/host/libdgkmidi.a -lstdc++ -lm
build/dgk/dos/libdgkmidi.a: FORCE
	$(Q)$(MAKE) -s -f libs.mk OUT=build/dgk GLOS=$(GLOS) DJCC="$(DJCC)" DJAR="$(DJAR)" dos
build/dgk/dos/libdgkglos.a: build/dgk/dos/libdgkmidi.a
build/dos/MIDPLAY.EXE: $(MIDPLAY_SRCS) $(wildcard include/dgk/*.h) build/dgk/dos/libdgkmidi.a
	@mkdir -p $(dir $@)
	$(Q)$(DJCC) $(WARN) -O2 -march=i586 -Iinclude -Isrc -I$(GLOS)/include -o $@ $(MIDPLAY_SRCS) \
	  build/dgk/dos/libdgkmidi.a build/dgk/dos/libdgkglos.a -lm
midplay: build/midplay
midplay-dos: build/dos/MIDPLAY.EXE
.PHONY: test clean FORCE midplay midplay-dos
test: build/test_dgk
	$(Q)build/test_dgk

# The music's and the network's libraries (libs.mk): GLOS's FM driver from GLOS's checkout, ymfm, and ENet.
build/dgk/host/libdgkmidi.a: FORCE
	$(Q)$(MAKE) -s -f libs.mk OUT=build/dgk GLOS=$(GLOS) host
build/dgk/host/libdgknet.a: build/dgk/host/libdgkmidi.a
build/test_dgk: $(TEST_SRCS) $(wildcard include/dgk/*.h src/*.h) build/dgk/host/libdgkmidi.a build/dgk/host/libdgknet.a
	@mkdir -p $(dir $@)
	$(Q)$(HOST_CC) $(WARN) -O2 -ffp-contract=off -Iinclude -Isrc -I$(DOSGL)/include -Ithird_party/enet/include \
	  -o $@ $(TEST_SRCS) build/dgk/host/libdgkmidi.a build/dgk/host/libdgknet.a -lstdc++ -lm
FORCE:

clean:
	rm -rf build out
