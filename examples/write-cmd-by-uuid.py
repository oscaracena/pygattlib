#!/usr/bin/python3
# -*- mode: python; coding: utf-8 -*-

# Copyright (C) 2024, Oscar Acena <oscaracena@gmail.com>
# This software is under the terms of Apache License v2 or later.

# NOTE: Perform a scan to ensure that your device is available. Use the DiscoveryService or
# a system tool (like 'bluetoothctl scan le').

import sys
from gattlib import GATTRequester, DeviceNotFound


if len(sys.argv) != 3:
    print(f"Usage: {sys.argv[0]} <addr> <UUID>")
    sys.exit(1)

address = sys.argv[1]
uuid = sys.argv[2]

try:
    requester = GATTRequester(address=address, auto_connect=False)
    print("> Connecting... ")
    requester.connect(wait=True)
    print(f"> Connected: {requester.is_connected()}.")
    print(f"> Writing 0xF to given UUID...")

    requester.write_by_uuid(uuid, b"\x0F")

except DeviceNotFound:
    print("Sorry, can't find that device. Try a discovert first!")
