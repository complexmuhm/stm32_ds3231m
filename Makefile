OBJDIR=obj
NAME=main
OBJNAME=$(OBJDIR)/$(NAME)

# Selecting Core
CORTEX_M=3

# Use newlib-nano. To disable it, specify USE_NANO=
USE_NANO=--specs=nano.specs

# Use seimhosting or not
USE_SEMIHOST=--specs=rdimon.specs
USE_NOHOST=--specs=nosys.specs

CORE=CM$(CORTEX_M)
BASE=../..

# Compiler & Linker
CC=arm-none-eabi-gcc
CXX=arm-none-eabi-g++

# Options for specific architecture
ARCH_FLAGS=-mthumb -mcpu=cortex-m$(CORTEX_M)

# Startup code
STARTUP=startup_ARM$(CORE).S

# -Os -flto -ffunction-sections -fdata-sections to compile for code size
CFLAGS=$(ARCH_FLAGS) $(STARTUP_DEFS) -Os -flto -ffunction-sections -fdata-sections -Og -g
CXXFLAGS=$(CFLAGS)

# Link for code size
GC=-Wl,--gc-sections

# Create map file
MAP=-Wl,-Map=$(OBJNAME).map
STARTUP_DEFS=-D__STARTUP_CLEAR_BSS -D__START=main
LDSCRIPTS=-L. -T nokeep.ld
LFLAGS=$(USE_NANO) $(USE_NOHOST) $(LDSCRIPTS) $(GC) $(MAP)

# Defines for CMSIS include files
DEFINES= -DSTM32F103xB

$(OBJNAME).bin: $(OBJNAME).axf
	arm-none-eabi-objcopy -O binary $< $@

$(OBJNAME).axf: $(NAME).cpp $(STARTUP)
	$(CXX) $(DEFINES) $^ $(CFLAGS) $(LFLAGS) -o $@

clean: 
	rm -f $(OBJDIR)/*

flash: $(OBJNAME).bin
	st-flash write $(OBJNAME).bin 0x08000000
