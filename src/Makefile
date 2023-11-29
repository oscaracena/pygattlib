# -*- mode: makefile-gmake; coding: utf-8 -*-

# Copyright (C) 2014,2020,2023 Oscar Acena <oscaracena@gmail.com>
# This software is under the terms of Apache License v2 or later.

# 2023/11/28 - Update bluez sources to version 5.70:
# - https://github.com/bluez/bluez/releases/tag/5.70

#
# NOTE: Sync changes between this file and setup.py
#

PLATFORM := $(shell uname -m)
TARGETS   = gattlib.so
OBJECTS   = bluez/attrib/att.o \
			bluez/attrib/gatt.o \
			bluez/attrib/gattrib.o \
			bluez/attrib/utils.o \
			bluez/src/shared/att.o \
			bluez/src/shared/crypto.o \
			bluez/src/shared/queue.o \
			bluez/src/shared/util.o \
			bluez/src/shared/io-glib.o \
			bluez/src/shared/gatt-client.o \
			bluez/src/shared/timeout-glib.o \
			bluez/src/shared/gatt-db.o \
			bluez/src/shared/gatt-helpers.o \
			bluez/src/shared/log.o \
			bluez/src/log.o \
			bluez/lib/uuid.o \
			bluez/lib/sdp.o \
			bluez/lib/bluetooth.o \
			bluez/lib/hci.o \
			bluez/btio/btio.o \
            gattservices.o \
			gattlib.o \
			bindings.o \
			beacon.o \

export PKG_CONFIG_PATH := $(PKG_CONFIG_PATH):/usr/lib/$(PLATFORM)-linux-gnu/pkgconfig/

BOOST_PYTHON  = $(shell python3 ../setup.py --get-boost-lib)

CFLAGS  += -fPIC -Wall -Wl,--no-undefined \
           -Ibluez \
		   $$(python3-config --includes) \
		   $$(pkg-config --cflags glib-2.0)

CFLAGS  += -ggdb -DDEBUG

CFLAGS  += -DVERSION='"5.70"'
CXXFLAGS = $(CFLAGS)
LDFLAGS  = -l$(BOOST_PYTHON) \
		   -lboost_thread \
		   $$(python3-config --ldflags --embed) \
           $$(pkg-config --libs glib-2.0)

# NOTE: do NOT link with shared bluetooth as it may generate a problem with
# versions

all: $(TARGETS)

gattlib.so: $(OBJECTS)
	$(CXX) $(CXXFLAGS) -shared -o $@ $^ $(LDFLAGS)

print-boost-ver:
	@echo $(BOOST_PYTHON)

patch-bluez:
	QUILT_PATCHES=patches quilt push -a

.PHONY: clean
clean:
	rm -f *.o *.so* *~ $(OBJECTS)
