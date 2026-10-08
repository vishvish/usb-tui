# usb-tui - terminal browser for connected USB devices
# Copyright (C) 2026 usb-tui contributors
# SPDX-License-Identifier: AGPL-3.0-only
# This program is free software under GNU AGPL version 3; see LICENSE.

CC = cc
CPPFLAGS = -D_DEFAULT_SOURCE
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -O2
LDFLAGS =
LDLIBS = -lncurses
MACOSX_DEPLOYMENT_TARGET ?= 10.11

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
CPPFLAGS += -D_DARWIN_C_SOURCE
CFLAGS += -mmacosx-version-min=$(MACOSX_DEPLOYMENT_TARGET)
LDFLAGS += -mmacosx-version-min=$(MACOSX_DEPLOYMENT_TARGET) \
	-framework IOKit -framework CoreFoundation
else
$(error This build currently needs macOS IOKit. USB backends for other systems can be added separately.)
endif

SOURCES = src/main.c src/usb_devices.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = usb-tui

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS)

src/%.o: src/%.c src/usb_devices.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(TARGET)
