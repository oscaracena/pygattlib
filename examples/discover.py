#!/usr/bin/python3
# -*- mode: python; coding: utf-8 -*-

# Copyright (C) 2014,2023, Oscar Acena <oscaracena@gmail.com>
# This software is under the terms of Apache License v2 or later.

from gattlib import DiscoveryService


service = DiscoveryService(adapter="hci0")
print("Disocver launched, waiting 10 seconds...")
devices = service.discover(timeout=10)

for address, name in list(devices.items()):
    print("name: {}, address: {}".format(name, address))

print("Done.")
