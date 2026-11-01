# dgk's own checks. Games build the kit themselves (dgk.mk).
#
#   make test     the main loop on a fake platform, and the vectors (host compiler; no screen)
DOSGL ?= $(HOME)/DOSGL
HOST_CC ?= gcc
Q ?= @
WARN := -std=gnu99 -Wall -Wextra -Werror

TEST_SRCS := tests/test_dgk.c src/app.c src/base.c src/log.c src/mix.c src/test.c

.PHONY: test clean
test: build/test_dgk
	$(Q)build/test_dgk

build/test_dgk: $(TEST_SRCS) $(wildcard include/dgk/*.h src/*.h)
	@mkdir -p $(dir $@)
	$(Q)$(HOST_CC) $(WARN) -O2 -ffp-contract=off -Iinclude -Isrc -I$(DOSGL)/include -o $@ $(TEST_SRCS) -lm

clean:
	rm -rf build out
