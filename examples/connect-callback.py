#!/usr/bin/python3
# -*- mode: python; coding: utf-8 -*-

# Copyright (C) 2023, Oscar Acena <oscaracena@gmail.com>
# This software is under the terms of Apache License v2 or later.

# NOTE: Perform a scan to ensure that your device is available. Use the DiscoveryService or
# a system tool (like 'bluetoothctl scan le').

import sys
from threading import Event

from gattlib import GATTRequester


if len(sys.argv) != 2:
    print(f"Usage: {sys.argv[0]} <addr>")
    sys.exit(1)

stop = Event()

def on_connect():
    print("Connected OK.")
    stop.set()

def on_fail(msg):
    print("Could not connect! :(")
    print(f"ERROR was: {msg}")
    stop.set()

def on_disconnect():
    print("Disconnected!")

print("Connecting... ")
requester = GATTRequester(address=sys.argv[1], auto_connect=False)
requester.connect(wait=False,
    on_connect=on_connect, on_fail=on_fail, on_disconnect=on_disconnect)

try:
    stop.wait()
except KeyboardInterrupt:
    print("\rBye!")
