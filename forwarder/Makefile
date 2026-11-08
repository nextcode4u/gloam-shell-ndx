BLOCKSDS ?= /opt/blocksds/core
NAME := gloam-forwarder
GAME_TITLE := Gloam Shell NDX
GAME_SUBTITLE := Return to your SD launcher
GAME_AUTHOR := Gloam Shell NDX
GAME_ICON := icon.png
SOURCEDIRS := arm9/source
INCLUDEDIRS := arm9/include ../arm9/include
ARM7ELF := ../build/frontend-arm7.elf
NDSTOOL_ARGS := -g GLDX 00 GLOAMFORWARD -u 00030004 -uc 3
CFLAGS := -Wextra -Werror=implicit-function-declaration
include $(BLOCKSDS)/sys/default_makefiles/rom_arm9/Makefile

# Same tested request serializer as the launcher, compiled without its UI model.
OBJS += build/system_routes.o
$(ELF): build/system_routes.o
build/system_routes.o: ../arm9/source/system_routes.c ../arm9/include/system_routes.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(ARM7ELF):
	$(MAKE) -C .. build/frontend-arm7.elf

# Installable DSi-only homebrew. GLDX gives title ID 00030004-474C4458.
# Both metadata and binary are unsigned; installation requires launcher patches.
$(ROM): $(ARM7ELF) icon.png
