Index
=======

* [Description](#description)
* [Installation](#installation)
    * [Python pip](#python-pip)
    * [Debian package](#debian-package)
* [Usage](#usage)
    * [Discovering devices](#discovering-devices)
    * [GATTRequester](#gattrequester)
    * [Discovering GATT services](#discovering-gatt-services)
    * [Reading data](#reading-data)
    * [Reading data asynchronously](#reading-data-asynchronously)
    * [Writing data](#writing-data)
    * [Receiving notifications](#receiving-notifications)
* [Disclaimer](#disclaimer)


Description
===========

This is a Python library to use the GATT Protocol for Bluetooth LE devices.
It uses D-Bus to control the underlying hardware. It does not call other
binaries to do its job :)


Installation
============

You can install this library using Python `pip`. If you use Debian/Ubuntu, you may also
install using the provided Debian package.

Python pip
----------

As easy as always:

    pip install gattlib

Debian package
--------------

There is a single Debian package available from
[https://github.com/oscaracena/pygattlib/releases](https://github.com/oscaracena/pygattlib/releases). Just download it and install using the following command:

    sudo apt install ./python3-gattlib*.deb


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
to create an instance of it, indicating the Bluetooth adapter you want
to use. Then call the method `discover`. Here you have some options. If
you provide a `timeout`, then it will wait that amount of time and
return a dictionary with the address and name of all the devices that
responded the discovery. For example:

```python
from gattlib import DiscoveryService

service = DiscoveryService("hci0")
devices = service.discover(timeout=5)

for address, name in devices.items():
    print("name: {}, address: {}".format(name, address))
```

If you don't provide a timeout, then you must give a
`callback` function. The `discover` will return inmediatly, but the
process will still be running on a separated thread. When a new device
is discovered, the callback will be called (with the name and address as
it's arguments). For example:

```python
import time
from gattlib import DiscoveryService

def on_new_device(name, address):
    print("name: {}, address: {}".format(name, address))

service = DiscoveryService("hci0")
service.discover(callback=on_new_device)

try:
    # You can do here other things, while discovering is still running
    time.sleep(9999)
except KeyboardInterrupt:
    service.stop()
```

As a third option, you may provide both the `timeout` and the `callback`.
In that case, the call to `discover` is blocking, and it will return the
discovered devices when the timeout expired. Also, while it is running and
a new device is found, it will call the provided `callback`.


GATTRequester
-------------

In order to manage a discovered device, you need to create an instance of
`GATTRequester`, using its address. Then, you can call `connect()` to create
a new connection to that device. If the device is not paired, it will try to
pair as well. For example:

```python
import sys
from gattlib import GATTRequester

if len(sys.argv) < 2:
    print("Usage: {} <addr>".format(sys.argv[0]))
    sys.exit(1)

print("Connecting...")
requester = GATTRequester(sys.argv[1], auto_connect=False)
requester.connect()
print("Done.")
```

The `connect()` method has the following arguments:

* `wait: boolean`, to indicate if you want to wait for this method to complete
  or not.
* `on_connect: Callable`, used to provide a callback that will be called when the
  connection is stablished, and will be called again if the connection is reset.
* `on_fail: Callable`, used to provide a callback to be called when an error on
  connection process occur.
* `on_disconnect: Callable`, a callback that will be called if the device is
  disconnected, for whatever reason it be.

You may also subclass the `GATTRequester`, and override the `on_connect()`,
`on_connect_failed(msg: str)` and `on_disconnect()` methods. Something like this:

```python
class MyRequester(GATTRequester):
    def on_connect(self):
        print("Connected OK.")

    def on_connect_failed(self, msg):
        print("Could not connect! :(")
        print(f"ERROR was: {msg}")

    def on_disconnect(self):
        print("Disconnected!")
```


Discovering GATT services
-------------------------

Once connected to a device, you can retrieve a list of its GATT services,
calling `discover_primary()`. This method will return a list of Service UUIDs.
For example:

```python
import sys
from gattlib import GATTRequester

if len(sys.argv) < 2:
    print("Usage: {} <addr>".format(sys.argv[0]))
    sys.exit(1)

requester = GATTRequester(sys.argv[1], auto_connect=True)

print("Find GATT Primary services...")
primary = requester.discover_primary()
for prim in primary:
    print(f"- {prim}")
```

You can, also, discover the list of characteristics of a service, given its
UUID. To do so, use the method `discover_characteristics()`. Like this:

```python
import sys
from gattlib import GATTRequester


if len(sys.argv) != 3:
    print("Usage: {} <addr> <service-UUID>".format(sys.argv[0]))
    sys.exit(1)

requester = GATTRequester(sys.argv[1], auto_connect=True)

print("Find GATT Characteristics of given service...")
chars = requester.discover_characteristics(service_uuid=sys.argv[2])
for c in chars:
    print(f"- {c}")
```


Reading data
------------

First of all, you need to create a `GATTRequester`, passing the address
of the device to connect to. Then, you can read some characteristic's value
given its UUID. For example:

```python
from gattlib import GATTRequester

req = GATTRequester("00:11:22:33:44:55")
value = req.read_by_uuid("00002a00-0000-1000-8000-00805f9b34fb")
```


Reading data asynchronously
--------------------------

The process is almost the same: you need to create a `GATTRequester`
passing the address of the device to connect to. Then, define a function
callback on which receive the response from your device. This callback
will be passed to the `*_async` method used.

**NOTE**: It is important to keep the Python process alive, or the
response will never arrive. You can `wait` on an `Event` object, or you
can do other things meanwhile.

The following is an example of async reading:

```python
import sys
from threading import Event
from gattlib import GATTRequester

ready = Event()

def on_response(value):
    print(f"Value: {value}")
    ready.set()

requester = GATTRequester("00:11:22:33:44:55")
requester.read_by_uuid_async(
    char_uuid = sys.argv[2],
    on_response = on_response)
print("Async reading, waiting response...")
ready.wait()
```


Writing data
------------

The process to write data is the same as for read. Create a `GATTRequest` object,
and use the method `write_by_uuid` to send the data. This method will issue a
`write request`. As a note, data must be a bytes object. See the following
example:

```python
from gattlib import GATTRequester

req = GATTRequester("00:11:22:33:44:55")
req.write_by_uuid(0x10, bytes([14, 4, 56]))
```

You can also use the `write_cmd()` to send a write command instead. It has the
same parameters as `write_by_uuid`: the uuid id and a bytes object. As an
example:

```python
from gattlib import GATTRequester

req = GATTRequester("00:11:22:33:44:55")
req.write_cmd(0x001e, bytes([16, 1, 4]))
```


Receiving notifications
-----------------------

FIXME: update!

To receive notifications from a remote device, you will need to enable them
on each characteristic (that supports notifications or indications).

For that, you have two options: 1) provide a callback function:

```python
from gattlib import GATTRequester

def on_notification(value):
    print(f"- notification arrived, value: {value}")

char_uuid = "00002a00-0000-1000-8000-00805f9b34fb"
req = GATTRequester("00:11:22:33:44:55")
req.enable_notifications(char_uuid, on_notification)
```

or 2) override the `on_notification()` method of `GATTRequester`:

```python
from gattlib import GATTRequester

class Requester(GATTRequester):
    def on_notification(self, value):
        print(f"- notification arrived, value: {value}")

char_uuid = "00002a00-0000-1000-8000-00805f9b34fb"
req = GATTRequester("00:11:22:33:44:55")
req.enable_notifications(char_uuid)
```

This second option is not recommended when you enable notifications on different characteristics, as they will share the same callback, and you will have trouble filtering the notification source. In that case, set a different callback, or use a `partial` to add a parameter with the source id.


Troubleshooting
===============

If you encounter any problem, ensure first that your hardware is compatible and
working fine. To check if your adapter supports BLE, you can use:

    sudo hciconfig hci0 lestates

To check if the device that you want to talk to is discoverable, run:

    bluetoothctl scan le

And check if it appears on the results. Moreover, you need the bluetooth service
registered on DBus. To see if that's the case, run:

    gdbus introspect --system --dest org.bluez --object-path /org/bluez/hci0


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
