# PHASE 4 (2026-09-16 -- see ../README.md "Status: Phase 4"): the "sprite
# laboratory" the plan asks for BEFORE wiring RenderCommand into GL2D on
# the DS -- a standalone SDL2 viewer to visually verify RENDER_FRAMES'
# anchor points and atlas resolution one global image index at a time.
# Desktop only. Needs only swos_render_frames.c from ../src (real geometry/
# atlas lookup, no Memory/VM/gameplay at all -- this tool never calls
# swosMemoryInit or ticks the game loop, it just browses the mapping
# table), plus its own texture decoder for the three real atlas PNGs'
# already-grit-converted BlocksDS output (../nds-app/build's generated
# bitmap+palette arrays -- see source/grit_texture.c) and
# ../sdl-debug/source/digit_font.h (reused, not duplicated, for the on-
# screen index/anchor HUD -- same "no SDL_ttf on this machine" situation
# sdl-debug already solved once).
#
# Same host toolchain as ../Makefile/../sdl-debug/Makefile (devkitPro's
# bundled mingw64 gcc -- the only native x86_64 host compiler on this
# machine; unrelated to the actual DS target, which stays BlocksDS's
# arm-none-eabi-gcc, untouched by this tool) and the same TMP/TEMP
# workaround -- see ../Makefile's own comment / memory
# feedback_blocksds_tmp_env_fix.
export TMP  := C:/msys64/tmp
export TEMP := C:/msys64/tmp

export PATH := /c/devkitPro/msys2/mingw64/bin:$(PATH)
CC := /c/devkitPro/msys2/mingw64/bin/gcc.exe
SDL2_CONFIG := /c/devkitPro/msys2/mingw64/bin/sdl2-config
SDL2_CFLAGS := $(shell $(SDL2_CONFIG) --cflags)
SDL2_LIBS   := $(filter-out -mwindows,$(shell $(SDL2_CONFIG) --libs))

# The three real atlas textures' grit output must already exist -- built by
# `make` in ../nds-app (this tool reads that build's generated .c files
# directly rather than re-running grit itself, to guarantee byte-identical
# pixel data to what the DS actually ships).
NDS_APP_GFX := ../nds-app/build/swos_vm_ds_app/graphics

CFLAGS ?= -std=c11 -Wall -Wextra -I../include -I../sdl-debug/source -I$(NDS_APP_GFX) $(SDL2_CFLAGS)
BUILD ?= build

APP_SRCS := $(wildcard source/*.c)
APP_OBJS := $(patsubst source/%.c,$(BUILD)/%.o,$(APP_SRCS))

.PHONY: all run clean check-gfx

all: check-gfx $(BUILD)/swos_sprite_lab.exe $(BUILD)/SDL2.dll

check-gfx:
	@test -f $(NDS_APP_GFX)/player_atlas_texture.h || \
		(echo "Run 'make' in ../nds-app first (needs its grit-converted texture headers)." && exit 1)

$(BUILD)/%.o: source/%.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/render_frames.o: ../src/swos_render_frames.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# player_atlas.c/ball_atlas.c are nds-app's own generated PLAYER_texcoords[]/
# BALL_texcoords[] -- pure data (no libnds/GL2D include), reused directly
# (not copied), same pattern as sdl-debug reusing match_bootstrap.c.
$(BUILD)/player_atlas.o: ../nds-app/source/player_atlas.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/ball_atlas.o: ../nds-app/source/ball_atlas.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/keeper_atlas.o: ../nds-app/source/keeper_atlas.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# The actual pixel/palette DATA for the three real atlases -- grit's output
# from ../nds-app's own build (see check-gfx above), pure C arrays, no
# libnds/GL2D dependency either.
$(BUILD)/%_texture.o: $(NDS_APP_GFX)/%_texture.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

TEXTURE_OBJS := $(BUILD)/player_atlas_texture.o $(BUILD)/player_atlas_team2_texture.o $(BUILD)/keeper_atlas_texture.o $(BUILD)/ball_atlas_texture.o

EXTRA_OBJS := $(BUILD)/render_frames.o $(BUILD)/player_atlas.o $(BUILD)/keeper_atlas.o $(BUILD)/ball_atlas.o $(TEXTURE_OBJS)

$(BUILD)/swos_sprite_lab.exe: $(APP_OBJS) $(EXTRA_OBJS)
	$(CC) $(CFLAGS) $(APP_OBJS) $(EXTRA_OBJS) -o $@ $(SDL2_LIBS)

$(BUILD)/SDL2.dll: /c/devkitPro/msys2/mingw64/bin/SDL2.dll
	cp $< $@

run: all
	./$(BUILD)/swos_sprite_lab.exe

clean:
	rm -rf $(BUILD)
