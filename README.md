Description
===========

This is a Python library to use the GATT Protocol for Bluetooth LE
devices. It is a wrapper around the implementation used by gatttool in
bluez package.

Installation
============

There are many ways of installing this library: using Python Pip,
using the Debian package, or manually compiling it.

Python pip
----------

Install as ever (you may need to install the packages listed on DEPENDS files):

    $ sudo apt-get install $(cat DEPENDS)
    $ sudo pip install gattlib

Debian way
----------

Add the following line to your sources list:

    deb http://babel.esi.uclm.es/arco sid main

And install using apt-get (or similar):

    $ sudo apt-get update
    $ sudo apt-get install python-gattlib

Compiling from source
---------------------

You should install the needed packages, which are described on DEPENDS file:

    $ sudo apt-get install $(cat DEPENDS)

And then, just type:

    $ make
    [...]

    $ make install

Usage
=====

This library provides two ways of work: sync and async. The Bluetooth
LE GATT protocol is asynchronous, so, when you need to read some
value, you make a petition, and wait for response. From the
perspective of the programmer, when you call a read method, you need
to pass it a callback object, and it will return inmediatly. The
response will be "injected" on that callback object.

This Python library allows you to call using a callback object
(async), or without it (sync). If you does not provide a callback
(working sync.), the library internally will create one, and will wait
until a response arrives, or a timeout expires. Then, the call will
return with the received data.

Discovering devices
-------------------

To discover BLE devices, use the `DiscoveryService` provided. You need
to create an instance of it, indicating the Bluetooth device you want
to use. Then call the method `discover`, with a timeout. It will
return a dictionary with the address and name of all devices that
responded the discovery.

**Note**: it is very likely that you will need admin permissions to do
a discovery, so run this script using `sudo` (or something similar).

As example:

    from gattlib import DiscoveryService

    service = DiscoveryService("hci0")
    devices = service.discover(2)

    for address, name in devices.items():
        print "name: {}, address: {}".format(name, address)

Reading data
------------

First of all, you need to create a GATTRequester, passing the address
of the device to connect to. Then, you can read a value defined by
either by its handle or by its UUID. For example:

    from gattlib import GATTRequester

    req = GATTRequester("00:11:22:33:44:55")
    name = req.read_by_uuid("00002a00-0000-1000-8000-00805f9b34fb")[0]
    steps = req.read_by_handle(0x15)[0]

Reading data asynchronously
--------------------------

The process is almost the same: you need to create a GATTRequester
passing the address of the device to connect to. Then, create a
GATTResponse object, on which receive the response from your
device. This object will be passed to the `async` method used.

**NOTE**: It is important to maintain the Python process alive, or the
response will never arrive. You can `wait` on that response object, or you
can do other things meanwhile.

The following is an example of response waiting:

    from gattlib import GATTRequester, GATTResponse

    req = GATTRequester("00:11:22:33:44:55")
    response = GATTResponse()

    req.read_by_handle_async(0x15, response)
    while not response.received():
        time.sleep(0.1)

    steps = response.received()[0]

And then, an example that inherits from GATTResponse to be notified
when the response arrives:

    from gattlib import GATTRequester, GATTResponse

    class NotifyYourName(GATTResponse):
        def on_response(self, name):
            print "your name is: {}".format(name)

    response = NotifyYourName()
    req = GATTRequester("00:11:22:33:44:55")
    req.read_by_handle_async(0x15, response)

    while True:
        # here, do other interesting things
        sleep(1)

Writing data
------------

The process to write data is the same as for read. Create a
GATTRequest object, and use the method `write_by_handle` to send the
data. As a note, data must be a string, but you can convert it from
`bytearray` or something similar. See the following example:

    from gattlib import GATTRequester

    req = GATTRequester("00:11:22:33:44:55")
    req.write_by_handle(0x10, str(bytearray([14, 4, 56])))