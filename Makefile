# Desktop build only -- no BlocksDS/libnds dependency. Proves the ported
# code compiles and runs before it's ever cross-compiled for the DS (per the
# plan in ../DEVLOG.md 2026-09-15: "kompilacja desktopowa" after every module).
#
# `make` on this machine doesn't pass TMP/TEMP through to the recipe's child
# process, so native gcc.exe falls back to C:\Windows (not writable) unless
# we `export` them here -- see ../swos-ds/Makefile and memory
# feedback-blocksds-tmp-env-fix.
export TMP  := C:/msys64/tmp
export TEMP := C:/msys64/tmp

# This project's actual DS target builds with BlocksDS's arm-none-eabi-gcc
# (see ../swos-ds/Makefile), never this. devkitPro's mingw64 gcc is used
# ONLY here, as a native x86_64 host compiler to run this repo's tests on
# desktop before anything is cross-compiled -- no other host gcc/clang/MSVC
# was found on this machine. `cc1.exe` needs mingw64/bin on PATH to find
# libmpfr-6.dll etc (it's not next to cc1.exe itself), so prepend it too.
# := not ?=: GNU Make's built-in CC=cc default counts as "already set" for ?=.
export PATH := /c/devkitPro/msys2/mingw64/bin:$(PATH)
CC := /c/devkitPro/msys2/mingw64/bin/gcc.exe
CFLAGS ?= -std=c11 -Wall -Wextra -Iinclude
BUILD ?= build

SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))

TEST_SRCS := $(wildcard tests/*.c)
TEST_BINS := $(patsubst tests/%.c,$(BUILD)/%,$(TEST_SRCS))

.PHONY: all test clean

all: test

$(BUILD)/%.o: src/%.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%: tests/%.c $(OBJS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $< $(OBJS) -o $@

test: $(TEST_BINS)
	@for t in $(TEST_BINS); do echo "== $$t =="; ./$$t || exit 1; done

clean:
	rm -rf $(BUILD)
