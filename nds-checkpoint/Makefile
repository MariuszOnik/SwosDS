# ARM/BlocksDS checkpoint for the swos-vm-c mechanical port (2026-09-16,
# after step 4). Builds the SAME VM sources the desktop tests use (../src,
# ../include -- no copies) for the real arm946e-s+nofp target and links a
# tiny libnds console app (source/main.c) that re-runs a slice of already
# host-verified checks. This is NOT the swos-ds game -- it exercises no
# rendering, just confirms the VM code (goto-heavy Flags state machine,
# swosAsr32 shifts, Q16.16 math) builds and behaves identically under the
# optimizing ARM toolchain as it already does on x86_64 desktop and against
# real C# (see ../README.md "Status: ARM checkpoint").
#
# Template copied from ../../swos-ds/Makefile (same BlocksDS install on this
# machine) and trimmed: no GFXDIRS/BINDIRS/AUDIODIRS, no NitroFS -- this
# project has no game assets, only C source.

export TMP		:= C:/msys64/tmp
export TEMP		:= C:/msys64/tmp

export BLOCKSDS			?= C:/msys64/opt/wonderful/thirdparty/blocksds/core
export BLOCKSDSEXT		?= C:/msys64/opt/wonderful/thirdparty/blocksds/external

export WONDERFUL_TOOLCHAIN	?= C:/msys64/opt/wonderful
ARM_NONE_EABI_PATH	?= $(WONDERFUL_TOOLCHAIN)/toolchain/gcc-arm-none-eabi/bin/

NAME		:= swos_vm_checkpoint

GAME_TITLE	:= swos-vm-c checkpoint
GAME_SUBTITLE	:= ARM/BlocksDS build+run smoke test
GAME_AUTHOR	:= swos-vm-c project
GAME_ICON	:= icon.gif

COMPDB		:= 0

# Source code paths
# -----------------
# ../src and ../include are the ACTUAL ported VM sources under test -- not
# copied here. source/ holds only this checkpoint's own main.c.

SOURCEDIRS	:= source ../src
INCLUDEDIRS	:= ../include
GFXDIRS		:=
BINDIRS		:=
AUDIODIRS	:=
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

OBJS		:= $(addsuffix .o,$(addprefix $(BUILDDIR)/,$(SOURCES_S))) \
		   $(addsuffix .o,$(addprefix $(BUILDDIR)/,$(SOURCES_C))) \
		   $(addsuffix .o,$(addprefix $(BUILDDIR)/,$(SOURCES_CPP)))

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

-include $(DEPS)
