.SUFFIXES:
.PHONY: all clean

TARGET := SHIS-Direct-Lumi
SOURCES := source
DATA := data

ARCH := -march=armv8-a -mtune=cortex-a57
CFLAGS := -g -Wall -Wextra -O2 -std=gnu11 -ffunction-sections -fdata-sections $(ARCH)
CFLAGS += -I$(DEVKITPRO)/libnx/include
CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions
ASFLAGS := $(ARCH)
LIBDIRS := $(DEVKITPRO)/libnx/lib $(PORTLIBS)
LIBS := $(DEVKITPRO)/libnx/lib/libnx.a

include $(DEVKITPRO)/libnx/switch_rules

all: $(TARGET).nro

$(TARGET).nacp:
	$(NX_NACP) --create $@ --title "SHIS Direct Lumi" --author "SHIS Stream" --version "0.1.0-prototype"

$(TARGET).nro: $(TARGET).elf $(TARGET).nacp $(DATA)/icon.png

clean:
	rm -f $(TARGET).elf $(TARGET).nro $(TARGET).nacp $(TARGET).map
