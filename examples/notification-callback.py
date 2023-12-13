#!/usr/bin/python3
# -*- mode: python; coding: utf-8 -*-

# Copyright (C) 2023, Oscar Acena <oscaracena@gmail.com>
# This software is under the terms of Apache License v2 or later.

import sys
from gattlib import GATTRequester


if len(sys.argv) != 3:
    print("Usage: {} <addr> <characteristic-UUID>".format(sys.argv[0]))
    sys.exit(1)

def on_notification(char, data):
    print(f"- notification on char {char}\n  data: {data}")

requester = GATTRequester(sys.argv[1], auto_connect=True)
requester.enable_notifications(sys.argv[2])
print("Connected, waiting events...")

import time
time.sleep(50)
