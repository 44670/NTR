.SUFFIXES:
	# Checks to see if DEVKITARM path is set. Warn the user if not.
	ifeq ($(strip $(DEVKITARM)),)
		$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
	endif

	# Pre-initializing the required rules
	TOPDIR := $(CURDIR)
include $(DEVKITARM)/3ds_rules

	# Setting up the important variables
	TARGET := $(notdir $(CURDIR))
	LIBDIRS := $(CTRULIB)
	BUILD := build
	CLEAN := clean
	SOURCE := source

	# Uses -O3 -s, for *.c files
	SOURCE_O3S := \
		source/dsp

	# Uses -Os -s, for *.c files
	SOURCE_OSS := \
		source/ns \
		source \
		source/libctru

	# Uses -Os -s, for *.s files
	SOURCE_OS := \
		source/ns \
		source \
		source/libctru

	INCLUDES := \
		include \
		include/jpeg \
		$(DEVKITPRO)/portlibs/armv6k/include

	BIN := bin
	OBJ := obj
	LIBPATH := -L .

	# Setting up the wildcard variables
	CFILES_O3S := $(foreach dir,$(SOURCE_O3S),$(wildcard $(dir)/*.c))
	CFILES_OSS := $(foreach dir,$(SOURCE_OSS),$(wildcard $(dir)/*.c))
	CPPFILES := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.cpp))
	SFILES := $(foreach dir,$(SOURCE_OSS),$(wildcard $(dir)/*.s))
	BINFILES := $(foreach dir,$(BIN),$(notdir $(wildcard $(dir)/*.*)))
	OBJFILES := $(foreach dir,$(CURDIR),$(wildcard $(dir)/*.o))

	# Setting up the flags
	ARMFLAGS := -march=armv6 -mlittle-endian -fomit-frame-pointer -ffast-math -march=armv6k -mtune=mpcore -mfloat-abi=hard
	CFLAGS_O3 := -O3 -s -g
	CFLAGS_OS := -Os -s -g
	SFLAGS_OS := -Os -s

# Setting up group variables
export INCLUDE := \
	$(foreach dir,$(INCLUDES),-I$(dir))

.PHONY: clean $(BUILD) old_3ds new_3ds all

all: $(BUILD) clean

$(BUILD): clean old_3ds new_3ds

clean:
	@ > include/gen.h
	@rm -fr *.o
	@rm -fr *.bin
	@rm -fr *.map
	@rm -fr *.out

old_3ds:
	@arm-none-eabi-gcc $(CFLAGS_O3) $(INCLUDE) $(CFILES_O3S) -c $(ARMFLAGS)
	@arm-none-eabi-gcc $(CFLAGS_OS) $(INCLUDE) $(CFILES_OSS) -c $(ARMFLAGS)
	@arm-none-eabi-gcc $(SFLAGS_OS) $(SFILES) -c $(ARMFLAGS)
	@arm-none-eabi-ld $(LIBPATH) -g -A armv6k -pie --print-gc-sections -T 3ds.ld -Map=test.map *.o -lc -lm -lgcc --nostdlib
	@cp -r *.o $(OBJ)/ 
	@cp a.out $(BIN)/homebrew.elf
	@arm-none-eabi-objcopy -O binary a.out payload.bin -S
	@cp payload.bin release/ntr.o3ds.bin

new_3ds:
	@echo "#define HAS_JPEG (1)" > include/gen.h
	@arm-none-eabi-gcc $(CFLAGS_O3) $(INCLUDE) $(CFILES_O3S) -c $(ARMFLAGS)
	@arm-none-eabi-gcc $(SFLAGS_OS) $(SFILES) -c $(ARMFLAGS)
	@arm-none-eabi-ld $(LIBPATH) -g -A armv6k -pie --print-gc-sections -T 3ds.ld -Map=test.map *.o -lc -lm -lgcc --nostdlib
	@cp -r *.o $(OBJ)/ 
	@cp a.out $(BIN)/homebrew.elf
	@arm-none-eabi-objcopy -O binary a.out payload.bin -S
	@cp payload.bin release/ntr.n3ds.bin
