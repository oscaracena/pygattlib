#!/usr/bin/python3
# -*- mode: python; coding: utf-8 -*-

# Copyright (C) 2023, Oscar Acena <oscaracena@gmail.com>
# This software is under the terms of Apache License v2 or later.

import sys
from gattlib import GATTRequester


if len(sys.argv) < 2:
    print("Usage: {} <addr>".format(sys.argv[0]))
    sys.exit(1)

print("Connecting...")
requester = GATTRequester(sys.argv[1], auto_connect=True)

print("Find GATT Primary services...")
primary = requester.discover_primary()
for prim in primary:
    print(f"- {prim}")

print("Done.")
