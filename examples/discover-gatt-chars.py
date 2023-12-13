#!/usr/bin/python3
# -*- mode: python; coding: utf-8 -*-

# Copyright (C) 2023, Oscar Acena <oscaracena@gmail.com>
# This software is under the terms of Apache License v2 or later.

import sys
from gattlib import GATTRequester


if len(sys.argv) != 3:
    print("Usage: {} <addr> <service-UUID>".format(sys.argv[0]))
    sys.exit(1)

print("Connecting...")
requester = GATTRequester(sys.argv[1], auto_connect=True)

print("Find GATT Characteristics of given service...")
chars = requester.discover_characteristics(service_uuid=sys.argv[2])
for c in chars:
    print(f"- {c}")

print("Done.")
