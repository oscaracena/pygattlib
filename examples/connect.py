#!/usr/bin/python
# -*- mode: python; coding: utf-8 -*-

# Copyright (C) 2014, Oscar Acena <oscar.acena@uclm.es>
# This software is under the terms of GPLv3 or later.

import sys
from gattlib import GATTRequester


class JustConnect(object):
    def __init__(self, address):
        self.requester = GATTRequester(address, False)
        self.connect()

    def connect(self):
        print "Connecting...",
        sys.stdout.flush()

        self.requester.connect(True)
        print "OK!"

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print "Usage: {} <addr>".format(sys.argv[0])
        sys.exit(1)

    JustConnect(sys.argv[1])
    print "Done."
