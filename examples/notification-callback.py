#!/usr/bin/python3
# -*- mode: python; coding: utf-8 -*-

# Copyright (C) 2023, Oscar Acena <oscaracena@gmail.com>
# This software is under the terms of Apache License v2 or later.

import time
import sys
from gattlib import GATTRequester


if len(sys.argv) != 3:
    print("Usage: {} <addr> <characteristic-UUID>".format(sys.argv[0]))
    sys.exit(1)

def on_notification(value):
    print(f"- notification, value: {value}")

requester = GATTRequester(sys.argv[1], auto_connect=True)
requester.enable_notifications(sys.argv[2], on_notification)
print("Connected, waiting events...")

try:
    while True:
        time.sleep(0.2)
except KeyboardInterrupt:
    print("\rBye!")
