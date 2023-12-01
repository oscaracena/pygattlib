#!/usr/bin/python3
# -*- mode: python; coding: utf-8 -*-

# Copyright (C) 2023, Oscar Acena <oscaracena@gmail.com>
# This software is under the terms of Apache License v2 or later.

import time
from gattlib import DiscoveryService


def on_new_device(name, address):
    print("name: {}, address: {}".format(name, address))

service = DiscoveryService()
service.discover(callback=on_new_device)

try:
    # You can do here other things while discovering is running
    print("Disocver launched without timeout. Waiting events...")
    while True:
        time.sleep(1)
except KeyboardInterrupt:
    service.stop()
    print("\rBye!")
