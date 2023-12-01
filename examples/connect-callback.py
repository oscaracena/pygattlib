#!/usr/bin/python3
# -*- mode: python; coding: utf-8 -*-

# Copyright (C) 2023, Oscar Acena <oscaracena@gmail.com>
# This software is under the terms of Apache License v2 or later.

# NOTE: Perform a scan to ensure that your device is available. Use the DiscoveryService or
# a system tool (like 'bluetoothctl scan le').

import sys
import time
from gattlib import GATTRequester


if len(sys.argv) != 2:
    print(f"Usage: {sys.argv[0]} <addr>")
    sys.exit(1)

def on_connect():
    print("connected!")

print("Connecting... ", end=' ')
requester = GATTRequester(address=sys.argv[1], auto_connect=False)
requester.connect(wait=False, on_success=on_connect)

try:
    while True:
        time.sleep(1)
except KeyboardInterrupt:
    print("\rDone!")
