# ARM/BlocksDS DS app for the swos-vm-c mechanical port -- step 12 ("Adapter
# from VM state to the DS renderer"). Builds the SAME VM sources the desktop
# tests use (../src, ../include -- no copies), links libnds+GL2D, and adds
# this project's own DS-adapter-only code (source/*.c): a minimal match
# bootstrap (rosters/formation/kickoff state -- NOT part of the mechanical
# C#-port fidelity, see source/match_bootstrap.c's header comment) and a
# GL2D renderer.
#
# The rendering pattern (glLoadSpriteSet/glLoadTileSet, camera scroll,
# glBegin2D/glSprite/glEnd2D loop) and the graphics assets (graphics/*.png
# +.grit, source/player_atlas.*, ball_atlas.*, player_frame_centers.*,
# pitch_map.h) are COPIES of files already proven working in the sibling
# ../../swos-ds project (extracted by its own tooling from the user's
# legally-owned GOG SWOS copy) -- copied for reuse, swos-ds itself is left
# untouched. swos-ds's own gameplay code (game_state.c/player.c physics) is
# NOT reused -- this project's simulation is swos-vm-c's own mechanically
# ported VM (../src), a completely different and much more complete engine.
#
# Template copied from ../nds-checkpoint/Makefile (ARM toolchain paths) and
# ../../swos-ds/Makefile (GFXDIRS asset pipeline), same BlocksDS install on
# this machine.

export TMP		:= C:/msys64/tmp
export TEMP		:= C:/msys64/tmp

export BLOCKSDS			?= C:/msys64/opt/wonderful/thirdparty/blocksds/core
export BLOCKSDSEXT		?= C:/msys64/opt/wonderful/thirdparty/blocksds/external

export WONDERFUL_TOOLCHAIN	?= C:/msys64/opt/wonderful
export PATH			:= C:/msys64/opt/wonderful/bin:$(PATH)
ARM_NONE_EABI_PATH	?= $(WONDERFUL_TOOLCHAIN)/toolchain/gcc-arm-none-eabi/bin/

NAME		:= swos_vm_ds_app

GAME_TITLE	:= swos-vm-c
GAME_SUBTITLE	:= mechanical-port VM on real GL2D renderer
GAME_AUTHOR	:= swos-vm-c project
GAME_ICON	:= icon.gif

COMPDB		:= 0

# Source code paths
# -----------------
# ../src and ../include are the ACTUAL ported VM sources under test -- not
# copied here. source/ holds this app's own adapter code + the copied
# atlas/tilemap data tables. graphics/ holds the copied PNG+.grit pairs,
# auto-converted to texture headers by grit at build time (see GFXDIRS).

SOURCEDIRS	:= source ../src
INCLUDEDIRS	:= ../include
GFXDIRS		:= graphics
BINDIRS		:=
AUDIODIRS	:= audio
NITROFSDIR	:=

DEFINES		:=

ARM7ELF		:= $(BLOCKSDS)/sys/arm7/main_core/arm7_maxmod.elf
LIBS		:= -lmm9 -lnds9
LIBDIRS		:= $(BLOCKSDS)/libs/maxmod \
		   $(BLOCKSDS)/libs/libnds

BUILDDIR	:= build/$(NAME)
ELF		:= build/$(NAME).elf
DUMP		:= build/$(NAME).dump
MAP		:= build/$(NAME).map
ROM		:= $(NAME).nds

SOUNDBANKINFODIR	:= $(BUILDDIR)/maxmod
SOUNDBANKDIR		:= $(BUILDDIR)/maxmod

PREFIX		:= $(ARM_NONE_EABI_PATH)arm-none-eabi-
CC		:= $(PREFIX)gcc
CXX		:= $(PREFIX)g++
LD		:= $(PREFIX)gcc
OBJDUMP		:= $(PREFIX)objdump
MKDIR		:= mkdir
RM		:= rm -rf

ifeq ($(VERBOSE),1)
V		:=
else
V		:= @
endif

ifneq ($(GFXDIRS),)
    SOURCES_PNG	:= $(shell find -L $(GFXDIRS) -name "*.png")
    INCLUDEDIRS	+= $(addprefix $(BUILDDIR)/,$(GFXDIRS))
endif
ifneq ($(AUDIODIRS),)
    # NOT `-regex '...\(...\)...'` (BlocksDS's own template, copied
    # verbatim into swos-ds's Makefile too) -- GNU Make's own $(shell ...)
    # argument-boundary scanner counts ALL parentheses in the raw text,
    # including ones inside a quoted string meant for the shell, and gets
    # confused by the escaped `\(...\)` grouping: the whole $(shell) call
    # silently evaluates empty (no error, just zero matches) under this
    # machine's GNU Make, even though the identical `find` command run
    # directly in a shell works fine. Confirmed by isolating it in a
    # throwaway Makefile before "fixing" anything -- not guessed. Avoiding
    # raw parens entirely sidesteps the parser confusion.
    SOURCES_AUDIO	:= $(shell find -L $(AUDIODIRS) -name "*.it" -o -name "*.mod" -o -name "*.s3m" -o -name "*.wav" -o -name "*.xm")
    ifneq ($(SOURCES_AUDIO),)
        INCLUDEDIRS	+= $(SOUNDBANKINFODIR)
    endif
endif

SOURCES_S	+= $(shell find -L $(SOURCEDIRS) -name "*.s")
SOURCES_C	+= $(shell find -L $(SOURCEDIRS) -name "*.c")
SOURCES_CPP	+= $(shell find -L $(SOURCEDIRS) -name "*.cpp")

DEFINES		+= -D__NDS__ -D__BLOCKSDS__ -DARM9

ARCH		:= -mthumb -mcpu=arm946e-s+nofp

SPECS		:= $(BLOCKSDS)/sys/crts/ds_arm9.specs

WARNFLAGS	:= -Wall

LIBS		+= -lc

INCLUDEFLAGS	:= $(foreach path,$(INCLUDEDIRS),-I$(path)) \
		   $(foreach path,$(LIBDIRS),-I$(path)/include)

LIBDIRSFLAGS	:= $(foreach path,$(LIBDIRS),-L$(path)/lib)

ASFLAGS		+= -x assembler-with-cpp $(INCLUDEFLAGS) $(DEFINES) \
		   $(ARCH) -ffunction-sections -fdata-sections \
		   -specs=$(SPECS)

CFLAGS		+= $(WARNFLAGS) $(INCLUDEFLAGS) $(DEFINES) \
		   $(ARCH) -O2 -ffunction-sections -fdata-sections \
		   -specs=$(SPECS)

LDFLAGS		+= $(ARCH) $(LIBDIRSFLAGS) -Wl,-Map,$(MAP) $(DEFINES) \
		   -Wl,--start-group $(LIBS) -Wl,--end-group -specs=$(SPECS)

OBJS_ASSETS	:= $(addsuffix .o,$(addprefix $(BUILDDIR)/,$(SOURCES_PNG)))
HEADERS_ASSETS	:= $(patsubst %.png,%.h,$(addprefix $(BUILDDIR)/,$(SOURCES_PNG)))

ifneq ($(SOURCES_AUDIO),)
    ifeq ($(strip $(NITROFSDIR)),)
        OBJS_ASSETS		+= $(SOUNDBANKDIR)/soundbank.c.o
    endif
    HEADERS_ASSETS	+= $(SOUNDBANKINFODIR)/soundbank.h
endif

OBJS_SOURCES	:= $(addsuffix .o,$(addprefix $(BUILDDIR)/,$(SOURCES_S))) \
		   $(addsuffix .o,$(addprefix $(BUILDDIR)/,$(SOURCES_C))) \
		   $(addsuffix .o,$(addprefix $(BUILDDIR)/,$(SOURCES_CPP)))

OBJS		:= $(OBJS_ASSETS) $(OBJS_SOURCES)

DEPS		:= $(OBJS:.o=.d)

.PHONY: all clean dump

all: $(ROM)

ifeq ($(strip $(GAME_SUBTITLE)),)
    GAME_FULL_TITLE := $(GAME_TITLE);$(GAME_AUTHOR)
else
    GAME_FULL_TITLE := $(GAME_TITLE);$(GAME_SUBTITLE);$(GAME_AUTHOR)
endif

$(ROM): $(ELF)
	@echo "  NDSTOOL $@"
	$(V)$(BLOCKSDS)/tools/ndstool/ndstool -c $@ \
		-7 $(ARM7ELF) -9 $(ELF) \
		-b $(GAME_ICON) "$(GAME_FULL_TITLE)"

$(ELF): $(OBJS)
	@echo "  LD      $@"
	$(V)$(LD) -o $@ $(OBJS) $(LDFLAGS)

$(DUMP): $(ELF)
	@echo "  OBJDUMP   $@"
	$(V)$(OBJDUMP) -h -C -S $< > $@

dump: $(DUMP)

clean:
	@echo "  CLEAN"
	$(V)$(RM) $(ROM) $(DUMP) build compile_commands.json

$(BUILDDIR)/%.s.o : %.s
	@echo "  AS      $<"
	@$(MKDIR) -p $(@D)
	$(V)$(CC) $(ASFLAGS) -MMD -MP -c -o $@ $<

$(BUILDDIR)/%.c.o : %.c
	@echo "  CC      $<"
	@$(MKDIR) -p $(@D)
	$(V)$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

$(BUILDDIR)/%.cpp.o : %.cpp
	@echo "  CXX     $<"
	@$(MKDIR) -p $(@D)
	$(V)$(CXX) $(CXXFLAGS) -MMD -MP -c -o $@ $<

$(BUILDDIR)/%.png.o $(BUILDDIR)/%.h : %.png %.grit
	@echo "  GRIT    $<"
	@$(MKDIR) -p $(@D)
	$(V)$(BLOCKSDS)/tools/grit/grit $< -ftc -W1 -o$(BUILDDIR)/$*
	$(V)$(CC) $(CFLAGS) -MMD -MP -c -o $(BUILDDIR)/$*.png.o $(BUILDDIR)/$*.c
	$(V)touch $(BUILDDIR)/$*.png.o $(BUILDDIR)/$*.h

ifneq ($(SOURCES_AUDIO),)

$(SOUNDBANKINFODIR)/soundbank.h: $(SOURCES_AUDIO)
	@echo "  MMUTIL  $^"
	@$(MKDIR) -p $(SOUNDBANKDIR)
	@$(MKDIR) -p $(SOUNDBANKINFODIR)
	$(V)$(BLOCKSDS)/tools/mmutil/mmutil $^ -d \
		-o$(SOUNDBANKDIR)/soundbank.bin -h$(SOUNDBANKINFODIR)/soundbank.h

ifeq ($(strip $(NITROFSDIR)),)
$(SOUNDBANKDIR)/soundbank.c.o: $(SOUNDBANKINFODIR)/soundbank.h
	@echo "  BIN2C   soundbank.bin"
	$(V)$(BLOCKSDS)/tools/bin2c/bin2c $(SOUNDBANKDIR)/soundbank.bin \
		$(SOUNDBANKDIR)
	@echo "  CC.9    soundbank_bin.c"
	$(V)$(CC) $(CFLAGS) -MMD -MP -c -o $(SOUNDBANKDIR)/soundbank.c.o \
		$(SOUNDBANKDIR)/soundbank_bin.c
endif

endif

# All assets must be built before the source code
$(SOURCES_S) $(SOURCES_C) $(SOURCES_CPP): $(HEADERS_ASSETS)

-include $(DEPS)
