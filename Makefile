BLOCKSDS ?= /opt/blocksds/core
NAME := gloam-shell-ndx
GAME_TITLE := Gloam Shell NDX
GAME_SUBTITLE := DSi SD frontend
GAME_AUTHOR := Gloam Shell NDX
GAME_ICON := forwarder/icon.png
SOURCEDIRS := arm9/source third_party/lodepng
INCLUDEDIRS := common/include arm9/include third_party/lodepng third_party/stb $(BLOCKSDS)/libs/version/include
ARM7ELF := build/frontend-arm7.elf
DEFINES := -DFRONTEND_DECODE_PROGRESS -DLODEPNG_NO_COMPILE_ENCODER -DLODEPNG_NO_COMPILE_ANCILLARY_CHUNKS
CFLAGS := -g -Wextra -Werror=implicit-function-declaration
# The SDK discovers sources with find. Pin their order so extracting the source
# ZIP on a different filesystem cannot change linker order and the ROM hash.
# override keeps the SDK's later += discovery from appending duplicates.
override SOURCES_S := $(sort $(shell find -L $(SOURCEDIRS) -name "*.s"))
override SOURCES_C := $(sort $(shell find -L $(SOURCEDIRS) -name "*.c"))
override SOURCES_CPP := $(sort $(shell find -L $(SOURCEDIRS) -name "*.cpp"))
include $(BLOCKSDS)/sys/default_makefiles/rom_arm9/Makefile

$(ELF): Makefile

$(ROM): $(ARM7ELF) $(GAME_ICON)

# The driver override below is pinned to BlocksDS 1.25.0. Review it when upgrading.
MELONDS_SD_COMPAT ?= 1
ARM7_FLAGS := -g -Os -mthumb -mcpu=arm7tdmi -ffunction-sections -fdata-sections \
    -specs=$(BLOCKSDS)/sys/crts/ds_arm7.specs -I$(BLOCKSDS)/libs/libnds/include -Ithird_party/libnds -Icommon/include \
    -DFRONTEND_MELONDS_SD_COMPAT=$(MELONDS_SD_COMPAT)

build/arm7/main.o: arm7/source/main.c common/include/hardware_protocol.h
	@mkdir -p $(@D)
	$(ARM_NONE_EABI_PATH)arm-none-eabi-gcc $(ARM7_FLAGS) -c $< -o $@

build/arm7/sdmmc.twl.c.o: third_party/libnds/sdmmc_compat.twl.c third_party/libnds/common/libnds_internal.h
	@mkdir -p $(@D)
	$(ARM_NONE_EABI_PATH)arm-none-eabi-gcc $(ARM7_FLAGS) -c $< -o $@

$(ARM7ELF): build/arm7/main.o build/arm7/sdmmc.twl.c.o
	$(ARM_NONE_EABI_PATH)arm-none-eabi-gcc -mthumb -mcpu=arm7tdmi \
		-specs=$(BLOCKSDS)/sys/crts/ds_arm7.specs \
		-L$(BLOCKSDS)/libs/libnds/lib -o $@ $^ -lnds7 -lc

LOADER_INPUTS := $(shell find third_party/nds-bootloader -type f)
build/homebrew-loader/load.bin: $(LOADER_INPUTS) tools/build_homebrew_loader.py
	python3 tools/build_homebrew_loader.py --blocksds $(BLOCKSDS) --tools $(ARM_NONE_EABI_PATH)

build/$(NAME)/arm9/source/loader_data.s.o: build/homebrew-loader/load.bin

# Optional DSi Menu return tile is bundled in development SD packages.
.PHONY: forwarder
all: forwarder
forwarder: $(ARM7ELF)
	$(MAKE) -C forwarder
	python3 tools/build_forwarder.py --package-only

# Generated archives stay under dist/; dependencies are fetched into build/.
.PHONY: sd
sd:
	python3 tools/build_release.py

# Optional emulator/player overlay, with matching source and license companions.
.PHONY: addons addons-check addons-pinned
addons:
	python3 tools/build_addons.py

addons-check:
	python3 tools/prepare_addons_release.py --check-only

addons-pinned:
	python3 tools/build_addons.py --pinned
