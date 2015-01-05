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

        self.wr("19 2c 78 91 5c 01 2c ae 5d 01 31"
                "   35 35 33 30 33 37 33 35 36 0f")
        # wr(req, "27 0e 0b 19 14 13 00 ff ff ff ff ff ff")

        import time
        time.sleep(10)

        # data = self.requester.read_by_handle(0x1)[0]
        # print bytearray(data)

    def wr(self, info):
        fields = info.split()
        handle = int(fields[0], 16)
        data = map(lambda x: int(x, 16), fields[1:])

        print ("write, handle: {:x}, data: {}"
               .format(handle, fields[1:]))

        print self.requester.write_by_handle(handle, str(bytearray(data)))

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
