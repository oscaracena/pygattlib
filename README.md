Index
=======

* [Description](#description)
* [Installation](#installation)
    * [Python pip](#python-pip)
    * [Debian way](#debian-way)
    * [Compiling from source](#compiling-from-source)
* [Usage](#usage)
    * [Discovering devices](#discovering-devices)
    * [Reading data](#reading-data)
    * [Reading data asynchronously](#reading-data-asynchronously)
    * [Writing data](#writing-data)
    * [Receiving notifications](#receiving-notifications)
* [Disclaimer](#disclaimer)


Description
===========

This is a Python library to use the GATT Protocol for Bluetooth LE
devices. It is a wrapper around the implementation used by gatttool in
bluez package. It does not call other binaries to do its job :)


Installation
============

There are many ways of installing this library: using Python Pip,
using the Debian package, or manually compiling it.

Python pip
----------

First, you need to install the depends. In Debian/Ubuntu this will be something like:

    sudo apt install pkg-config libboost-python-dev libboost-thread-dev libbluetooth-dev libglib2.0-dev python-dev

Now, install as ever:

    pip install gattlib

Debian way
----------

There is a single Debian package available from
[https://github.com/oscaracena/pygattlib/releases](https://github.com/oscaracena/pygattlib/releases). Just download it and install using the following command:

    sudo apt install ./python3-gattlib*.deb

Compiling from source
---------------------

You should install the needed packages, which are described on `DEPENDS`
file. Take special care about versions: libbluetooth-dev should be
4.101 or greater. Then, just type:

    make

Then, to install, just:

    make install


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

```python
from gattlib import DiscoveryService

service = DiscoveryService("hci0")
devices = service.discover(2)

for address, name in devices.items():
    print("name: {}, address: {}".format(name, address))
```


Reading data
------------

First of all, you need to create a GATTRequester, passing the address
of the device to connect to. Then, you can read a value defined by
either by its handle or by its UUID. For example:

```python
from gattlib import GATTRequester

req = GATTRequester("00:11:22:33:44:55")
name = req.read_by_uuid("00002a00-0000-1000-8000-00805f9b34fb")[0]
steps = req.read_by_handle(0x15)[0]
```


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

```python
from gattlib import GATTRequester, GATTResponse

req = GATTRequester("00:11:22:33:44:55")
response = GATTResponse()

req.read_by_handle_async(0x15, response)
while not response.received():
    time.sleep(0.1)

steps = response.received()[0]
```

And then, an example that inherits from GATTResponse to be notified
when the response arrives:

```python
from gattlib import GATTRequester, GATTResponse

class NotifyYourName(GATTResponse):
    def on_response(self, name):
        print("your name is: {}".format(name))

response = NotifyYourName()
req = GATTRequester("00:11:22:33:44:55")
req.read_by_handle_async(0x15, response)

while True:
    # here, do other interesting things
    sleep(1)
```


Writing data
------------

The process to write data is the same as for read. Create a GATTRequest object,
and use the method `write_by_handle` to send the data. This method will issue a
`write request`. As a note, data must be a bytes object. See the following
example:

```python
from gattlib import GATTRequester

req = GATTRequester("00:11:22:33:44:55")
req.write_by_handle(0x10, bytes([14, 4, 56]))
```

You can also use the `write_cmd()` to send a write command instead. It has the
same parameters as `write_by_handle`: the handler id and a bytes object. As an
example:

```python
from gattlib import GATTRequester

req = GATTRequester("00:11:22:33:44:55")
req.write_cmd(0x001e, bytes([16, 1, 4]))
```


Receiving notifications
-----------------------

To receive notifications from remote device, you need to overwrite the
`on_notification` method of `GATTRequester`. This method is called
each time a notification arrives, and has two params: the handle where
the notification was produced, and a string with the data that came in
the notification event. The following is a brief example:

```python
from gattlib import GATTRequester

class Requester(GATTRequester):
    def on_notification(self, handle, data):
        print("- notification on handle: {}\n".format(handle))
```

You can receive indications as well. Just overwrite the method
`on_indication` of `GATTRequester`.


Disclaimer
==========

This software may harm your device. Use it at your own risk.

    THERE IS NO WARRANTY FOR THE PROGRAM, TO THE EXTENT PERMITTED BY
    APPLICABLE LAW. EXCEPT WHEN OTHERWISE STATED IN WRITING THE COPYRIGHT
    HOLDERS AND/OR OTHER PARTIES PROVIDE THE PROGRAM “AS IS” WITHOUT
    WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE. THE ENTIRE RISK AS TO THE QUALITY AND
    PERFORMANCE OF THE PROGRAM IS WITH YOU. SHOULD THE PROGRAM PROVE
    DEFECTIVE, YOU ASSUME THE COST OF ALL NECESSARY SERVICING, REPAIR OR
    CORRECTION.