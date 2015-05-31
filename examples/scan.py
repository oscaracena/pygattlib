#!/usr/bin/python
# -*- mode: python; coding: utf-8 -*-

# Copyright (C) 2014, Oscar Acena <oscaracena@gmail.com>
# This software is under the terms of GPLv3 or later.

from gattlib import BeaconService

service = BeaconService("hci0")
devices = service.scan(4)

for address, name in list(devices.items()):
    print("name: {}, address: {}".format(name, address))

print("Done.")
