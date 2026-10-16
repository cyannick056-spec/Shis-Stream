.SUFFIXES:
.PHONY: all clean

TOPDIR := $(CURDIR)
TARGET := SHIS-Direct-Lumi
BUILD := build
SOURCES := source
DATA := data
INCLUDES :=
APP_TITLE := SHIS Direct Lumi
APP_AUTHOR := SHIS Stream
APP_VERSION := 0.1.0-prototype
ICON := data/icon.png

ARCH := -march=armv8-a -mtune=cortex-a57
CFLAGS := -g -Wall -Wextra -O2 -std=gnu11 -ffunction-sections -fdata-sections $(ARCH)
CFLAGS += -I$(DEVKITPRO)/libnx/include
LDFLAGS := -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(TARGET).map
LD := $(CC)
CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions
ASFLAGS := $(ARCH)
LIBDIRS := $(PORTLIBS) $(LIBNX)
LIBS := -lnx

include $(DEVKITPRO)/libnx/switch_rules

all: $(TARGET).nro


clean:
	rm -f $(TARGET).elf $(TARGET).nro $(TARGET).nacp $(TARGET).map
