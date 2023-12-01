#!/usr/bin/python3
# -*- mode: python; coding: utf-8 -*-

# Copyright (C) 2023, Oscar Acena <oscaracena@gmail.com>
# This software is under the terms of Apache License v2 or later.

# NOTE: Perform a scan to ensure that your device is available. Use the DiscoveryService or
# a system tool (like 'bluetoothctl scan le').

import sys
from gattlib import GATTRequester, DeviceNotFound


if len(sys.argv) != 2:
    print(f"Usage: {sys.argv[0]} <addr>")
    sys.exit(1)

try:
    requester = GATTRequester(address=sys.argv[1], auto_connect=False)
    print("Connecting... ")
    requester.connect(wait=True)
    print("Connected OK.")
except DeviceNotFound:
    print("Sorry, can't find that device. Try a discovert first!")
