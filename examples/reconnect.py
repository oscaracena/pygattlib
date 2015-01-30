#!/usr/bin/python
# -*- mode: python; coding: utf-8 -*-

# Copyright (C) 2014, Oscar Acena <oscaracena@gmail.com>
# This software is under the terms of GPLv3 or later.

from __future__ import print_function

import sys
import time
from gattlib import GATTRequester


class ConnectAndDisconnect(object):
    def __init__(self, address):
        self.requester = GATTRequester(address, False)

        print("I will connect & disconnect many times...")

        for i in range(3):
            self.connect()
            self.disconnect()

    def connect(self):
        print("Connecting...", end=' ')
        sys.stdout.flush()

        self.requester.connect(True)
        print("OK!")

        time.sleep(1)

    def disconnect(self):
        print("Disconnecting...", end=' ')
        sys.stdout.flush()

        self.requester.disconnect()
        print("OK!")

        time.sleep(1)


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: {} <addr>".format(sys.argv[0]))
        sys.exit(1)

    ConnectAndDisconnect(sys.argv[1])
    print("Done.")
