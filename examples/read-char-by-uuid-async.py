#!/usr/bin/python3
# -*- mode: python; coding: utf-8 -*-

# Copyright (C) 2023, Oscar Acena <oscaracena@gmail.com>
# This software is under the terms of Apache License v2 or later.

import sys
from threading import Event
from gattlib import GATTRequester


if len(sys.argv) != 3:
    print("Usage: {} <addr> <characteristic-UUID>".format(sys.argv[0]))
    sys.exit(1)

ready = Event()

def on_response(value):
    print(f"Value: {value}")
    ready.set()

print("Connecting...")
requester = GATTRequester(sys.argv[1], auto_connect=True)
print("Async reading given characteristic...")
requester.read_by_uuid_async(
    char_uuid = sys.argv[2],
    on_response = on_response)
print("Waiting response...")
ready.wait()
print("Done!")
