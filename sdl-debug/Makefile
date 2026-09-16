# swos-vm-c step 12.5: minimal SDL2 diagnostic frontend. Desktop only --
# builds the SAME ported VM sources the main desktop tests use (../src,
# ../include -- no copies) plus this project's own DS adapter setup
# (../nds-app/source/match_bootstrap.c, linked directly, not copied) and a
# small SDL2 renderer/HUD/logger under source/.
#
# Same host toolchain as ../Makefile and ../nds-checkpoint/../Makefile
# (devkitPro's bundled mingw64 gcc -- the only native x86_64 host compiler
# found on this machine) and the same TMP/TEMP export workaround -- see
# ../Makefile's own comment / memory feedback_blocksds_tmp_env_fix.
export TMP  := C:/msys64/tmp
export TEMP := C:/msys64/tmp

export PATH := /c/devkitPro/msys2/mingw64/bin:$(PATH)
CC := /c/devkitPro/msys2/mingw64/bin/gcc.exe
SDL2_CONFIG := /c/devkitPro/msys2/mingw64/bin/sdl2-config
SDL2_CFLAGS := $(shell $(SDL2_CONFIG) --cflags)
# Strip -mwindows from sdl2-config's default libs: this tool's diagnostic
# dumps (stage snapshots, per-slot detail, event log) go to stdout/the
# console, which -mwindows (GUI subsystem, no console attached) would
# silently swallow -- a console app can still open an SDL window fine.
SDL2_LIBS   := $(filter-out -mwindows,$(shell $(SDL2_CONFIG) --libs))

CFLAGS ?= -std=c11 -Wall -Wextra -I../include -I../nds-app/source $(SDL2_CFLAGS)
BUILD ?= build

VM_SRCS := $(wildcard ../src/*.c)
VM_OBJS := $(patsubst ../src/%.c,$(BUILD)/vm_%.o,$(VM_SRCS))

APP_SRCS := $(wildcard source/*.c)
APP_OBJS := $(patsubst source/%.c,$(BUILD)/%.o,$(APP_SRCS))

.PHONY: all run clean

all: $(BUILD)/swos_vm_sdl_debug.exe $(BUILD)/SDL2.dll

$(BUILD)/vm_%.o: ../src/%.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: source/%.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# match_bootstrap.c is the DS app's own setup code, reused here directly
# (not copied) -- it has no libnds/GL2D dependency, only the portable
# swos_*.h headers, so it builds fine as a desktop object too. Same pattern
# as ../tests/test_step12_integration_golden.c.
$(BUILD)/match_bootstrap.o: ../nds-app/source/match_bootstrap.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/swos_vm_sdl_debug.exe: $(VM_OBJS) $(APP_OBJS) $(BUILD)/match_bootstrap.o
	$(CC) $(CFLAGS) $(VM_OBJS) $(APP_OBJS) $(BUILD)/match_bootstrap.o -o $@ $(SDL2_LIBS)

$(BUILD)/SDL2.dll: /c/devkitPro/msys2/mingw64/bin/SDL2.dll
	cp $< $@

run: all
	./$(BUILD)/swos_vm_sdl_debug.exe

clean:
	rm -rf $(BUILD)
