TARGET  := gba_vampire_survivors
BUILD   := build
SRC     := src

DEVKITPRO ?= /opt/devkitpro
DEVKITARM ?= $(DEVKITPRO)/devkitARM
LIBGBA    := $(DEVKITPRO)/libgba

CC      := $(DEVKITARM)/bin/arm-none-eabi-gcc
OBJCOPY := $(DEVKITARM)/bin/arm-none-eabi-objcopy

# -------------------------------------------------------
# COMPILACIÓN
# -------------------------------------------------------
CFLAGS := \
	-mthumb \
	-mthumb-interwork \
	-mcpu=arm7tdmi \
	-O2 \
	-ffunction-sections \
	-fdata-sections \
	-fno-common \
	-I$(LIBGBA)/include \
	-I$(SRC)

# -------------------------------------------------------
# LINKER (IMPORTANTE: usar gba.specs)
# -------------------------------------------------------
LDFLAGS := \
	-mthumb \
	-mthumb-interwork \
	-mcpu=arm7tdmi \
	-specs=gba.specs \
	-L$(LIBGBA)/lib \
	-lgba \
	-Wl,--gc-sections \
	-Wl,-Map,$(BUILD)/$(TARGET).map \
	-Wl,--no-warn-rwx-segments

# -------------------------------------------------------
# SOURCES
# -------------------------------------------------------
SOURCES := $(wildcard $(SRC)/*.c)
OBJECTS := $(patsubst $(SRC)/%.c,$(BUILD)/%.o,$(SOURCES))

# -------------------------------------------------------
# BUILD RULES
# -------------------------------------------------------
all: $(BUILD) $(TARGET).gba

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: $(SRC)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

$(TARGET).gba: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	@echo "ROM generada: $(TARGET).gba"

clean:
	rm -rf $(BUILD) *.gba *.elf *.map
