.SUFFIXES:
.PHONY: all clean

TARGET := SHIS-Direct-Lumi
BUILD := build
SOURCES := source
DATA := data
INCLUDES :=

ARCH := -march=armv8-a -mtune=cortex-a57 -mtp=soft
CFLAGS := -g -Wall -Wextra -O2 -std=gnu11 -ffunction-sections -fdata-sections $(ARCH)
CFLAGS += $(INCLUDES)
CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions
ASFLAGS := $(ARCH)
LDFLAGS = -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS := -lnx

include $(DEVKITPRO)/libnx/switch_rules

all: $(TARGET).nro

$(TARGET).nacp:
	$(NX_NACP) --create $@ --title "SHIS Direct Lumi" --author "SHIS Stream" --version "0.1.0-prototype"

$(TARGET).nro: $(TARGET).elf $(TARGET).nacp $(DATA)/icon.png

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).nro $(TARGET).nacp $(TARGET).map
