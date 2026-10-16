.SUFFIXES:
.PHONY: all clean

TARGET := SHIS-Direct-Lumi
BUILD := build
SOURCES := source
DATA := data
INCLUDES :=

CFLAGS := -std=gnu11 -O2 -Wall -Wextra -Wno-unused-parameter -ffunction-sections -fdata-sections
CFLAGS += $(INCLUDES) -D__SWITCH__
CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions
ASFLAGS := -g
LDFLAGS := -specs=$(DEVKITPRO)/libnx/switch.specs -Wl,-Map,$(TARGET).map
LIBS := -lnx

ifneq ($(strip $(DEVKITPRO)),)
include $(DEVKITPRO)/libnx/switch_rules
endif

all: $(TARGET).nro

$(TARGET).elf: $(SOURCES)/main.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

$(TARGET).nro: $(TARGET).elf
	$(ELF2NRO) $< $@ --icon=$(DATA)/icon.png --nacp=$(DATA)/$(TARGET).nacp
	$(NX_NACP) --create $(DATA)/$(TARGET).nacp --title "SHIS Direct Lumi" --author "SHIS Stream" --version "0.1.0-prototype"

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).nro $(TARGET).map $(DATA)/$(TARGET).nacp
