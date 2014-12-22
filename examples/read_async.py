#!/usr/bin/python
# -*- mode: python; coding: utf-8 -*-

# Copyright (C) 2014, Oscar Acena <oscar.acena@uclm.es>
# This software is under the terms of GPLv3 or later.

import sys
import time
from gattlib import GATTRequester, GATTResponse


class AsyncReader(object):
    def __init__(self, address):
        self.requester = GATTRequester(address)
        self.response = GATTResponse()

        self.connect()
        self.request_data()
        self.wait_response()

    def connect(self):
        print "Connecting...",
        sys.stdout.flush()

        self.requester.wait_connection()
        print "OK!"

    def request_data(self):
        self.requester.read_by_handle_async(0x1, self.response)

    def wait_response(self):
        while not self.response.received():
            time.sleep(0.1)

        data = self.response.received()[0]

        print "bytes received:",
        for b in data:
            print hex(ord(b)),
        print ""


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print "Usage: {} <addr>".format(sys.argv[0])
        sys.exit(1)

    AsyncReader(sys.argv[1])
    print "Done."
