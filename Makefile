#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)

containing = $(foreach v,$2,$(if $(findstring $1,$v),$v))
not-containing = $(foreach v,$2,$(if $(findstring $1,$v),,$v))

include $(DEVKITARM)/3ds_rules

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# DATA is a list of directories containing data files
# INCLUDES is a list of directories containing header files
#---------------------------------------------------------------------------------
# TARGET	:=
BUILD		:=	build
SOURCES		:=	source source/dsp source/jpeg source/ns source/libctru
DATA		:=
INCLUDES	:=	include include/jpeg
OLD_3DS_TARGET := o3ds_ntr_payload
NEW_3DS_TARGET := n3ds_ntr_payload

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS_DEF	:=	-Wall \
			-fomit-frame-pointer -ffunction-sections -fdata-sections \
			$(ARCH)

ifeq ($(strip $(NEW_3DS)),1)
TARGET := $(NEW_3DS_TARGET)
CFLAGS_VAR := $(INCLUDE) -DHAS_JPEG=1 -O3
else
TARGET := $(OLD_3DS_TARGET)

ifeq ($(findstring nightshift,$(basename $%)),nightshift)
CFLAGS_VAR := $(INCLUDE) -O3
else
CFLAGS_VAR := $(INCLUDE) -Os
endif

endif

CFLAGS	:= $(CFLAGS_DEF) $(CFLAGS_VAR)

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++11

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-pie -nostartfiles -T$(TOPDIR)/3ds.ld -g $(ARCH) -Wl,--gc-sections -Wl,-Map,$(notdir $*.map)

LIBS	:= -lm

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:=


#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#---------------------------------------------------------------------------------
	export LD	:=	$(CC)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

export OFILES_SOURCES 	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES))

export OFILES := $(OFILES_BIN) $(OFILES_SOURCES)

export HFILES	:=	$(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all old_3ds new_3ds both

#---------------------------------------------------------------------------------
all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(NEW_3DS_TARGET).bin $(NEW_3DS_TARGET).elf $(OLD_3DS_TARGET).bin $(OLD_3DS_TARGET).elf

old_3ds:
	@echo making ntr_payload for old_3ds ...
	@$(MAKE) NEW_3DS=0

new_3ds:
	@echo making ntr_payload for new_3ds ...
	@$(MAKE) NEW_3DS=1

pre_both: old_3ds
	@rm -fr $(BUILD)

both: pre_both new_3ds



#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(OUTPUT).bin: $(OUTPUT).elf
	@echo "creating $(notdir $@)"
	@$(OBJCOPY)  -O binary $< $@ -S

$(OUTPUT).elf	:	$(OFILES)

$(OFILES_SOURCES) : $(HFILES)


#---------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data
#---------------------------------------------------------------------------------
%.bin.o	%_bin.h :	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
