#!/usr/bin/python
# -*- mode: python; coding: utf-8 -*-

# Copyright (C) 2014, Oscar Acena <oscar.acena@uclm.es>
# This software is under the terms of GPLv3 or later.

import sys
import time
from gattlib import GATTRequester, GATTResponse


class NotifyMeYourValue(GATTResponse):
    def __init__(self):
        super(NotifyMeYourValue, self).__init__()
        self.done = False

    def on_response(self, data):
        print "bytes received:",
        for b in data:
            print hex(ord(b)),
        print ""

        self.done = True


class AsyncReader(object):
    def __init__(self, address):
        self.requester = GATTRequester(address)
        self.response = NotifyMeYourValue()

        self.connect()
        self.request_data()
        self.loop()

    def connect(self):
        print "Connecting...",
        sys.stdout.flush()

        self.requester.wait_connection()
        print "OK!"

    def request_data(self):
        self.requester.read_by_handle_async(0x1, self.response)

    def loop(self):
        while not self.response.done:
            time.sleep(0.1)


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print "Usage: {} <addr>".format(sys.argv[0])
        sys.exit(1)

    AsyncReader(sys.argv[1])
