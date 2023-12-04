#!/usr/bin/python3
# -*- mode: python; coding: utf-8 -*-

# Copyright (C) 2023, Oscar Acena <oscaracena@gmail.com>
# This software is under the terms of Apache License v2 or later.

import sys
from gattlib import GATTRequester


if len(sys.argv) != 3:
    print("Usage: {} <addr> <characteristic-UUID>".format(sys.argv[0]))
    sys.exit(1)

print("Connecting...")
requester = GATTRequester(sys.argv[1], auto_connect=True)

print("Reading given characteristic...")
value = requester.read_by_uuid(sys.argv[2])
print(f"Value: {value}")
