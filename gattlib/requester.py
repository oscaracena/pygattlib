# -*- mode: python; coding: utf-8 -*-

# Copyright (C) 2023, Oscar Acena <oscaracena@gmail.com>
# This software is under the terms of Apache License v2 or later.

from threading import Thread
from typing import Callable
from functools import partial

from .dbus import BluezDBus, DBusError, Signals, AbstractObjectProxy
from .exceptions import BTIOException
from .utils import log, deprecated_args, deprecated_method, wrap_exception, options


class GATTRequester:
    @deprecated_args(do_connect="auto_connect", device="adapter")
    def __init__(self, address: str, auto_connect: bool = True, adapter: str = "hci0"):
        self._bluez = BluezDBus()
        self._device = self._bluez.find_device(address, adapter)
        self._log = log.getChild("GATTReq")

        obj_path = self._device.prop("ObjectPath")
        self._bluez.connect_signal(
            Signals.OBJECT_PROPERTIES_CHANGED(obj_path), self._on_props_changed)

        self._on_connect_cb = None
        self._on_disconnect_cb = None
        self._on_fail_cb = None
        self._notify_ids = {}

        # forward some methods
        self.prop = self._device.prop

        if auto_connect:
            self.connect()

    @deprecated_args(channel_type=None, security_level=None, psm=None, mtu=None)
    @wrap_exception(DBusError, BTIOException)
    def connect(self, wait: bool = False, on_connect: Callable = None,
            on_fail: Callable = None, on_disconnect: Callable = None) -> None:

        self._on_connect_cb = on_connect
        self._on_fail_cb = on_fail
        self._on_disconnect_cb = on_disconnect

        # FIXME: On wait=True, wait until 'Connected' becames True

        def _do_connect():
            try:
                already_connected = self.is_connected()
                self._device.Connect()

                # If it was already connected, the property will not change, and
                # the callback will not be called. Call it here.
                if already_connected:
                    self.on_connect()
            except Exception as ex:
                self.on_connect_failed(str(ex))

        if not wait:
            Thread(target=_do_connect, daemon=True).start()
        else:
            _do_connect()

    def pair(self):
        """NOTE: This method will not work unless there is a pairing agent open. Use with
        caution. Or even better, for pairing, use the system's agent directly."""
        self._device.Pair()

    def is_connected(self) -> bool:
        return self._device.prop("Connected")

    def disconnect(self) -> None:
        self._device.Disconnect()

    def discover_primary(self) -> list:
        return self._bluez.find_gatt_services(
            path_prefix = self._device.prop("ObjectPath"),
            primary = True
        )

    @deprecated_args(start=None, end=None, uuid="service_uuid")
    def discover_characteristics(self, service_uuid: str) -> list:
        return self._bluez.find_gatt_characteristics(
            path_prefix = self._device.prop("ObjectPath"),
            service_uuid = service_uuid.lower(),
        )

    @deprecated_args(uuid="char_uuid")
    def read_by_uuid(self, char_uuid: str) -> list:
        char, path = self.get_characteristic(char_uuid)
        return char.ReadValue({})

    @deprecated_args(uuid="char_uuid", response="response_cb")
    def read_by_uuid_async(self, char_uuid: str, response_cb: Callable) -> None:

        def _do_read():
            value = self.read_by_uuid(char_uuid)
            response_cb(value)

        Thread(target=_do_read, daemon=True).start()

    def write_by_uuid(self, char_uuid: str, data: bytes) -> None:
        char, path = self.get_characteristic(char_uuid)
        return char.WriteValue(data, options({"type": "request"}))

    def write_cmd_by_uuid(self, char_uuid: str, data: bytes) -> None:
        char, path = self.get_characteristic(char_uuid)
        return char.WriteValue(data, options({"type": "command"}))

    @deprecated_method(replaced_by="write_by_uuid")
    def write_by_handle(self) -> None: pass

    @deprecated_method(replaced_by="write_cmd_by_uuid")
    def write_cmd(self) -> None: pass

    @deprecated_args(handle=None, notifications=None, indications=None)
    def enable_notifications(self, char_uuid: str, callback: Callable = None,
            filter: list = ("value",)) -> None:

        if callback is None:
            callback = self.on_notification

        char, path = self.get_characteristic(char_uuid)
        if 'notify' not in char.Flags and 'indicate' not in char.Flags:
            raise TypeError("This characteristic does not allow Notifications.")

        # FIXME: wait until 'Notifying' is True

        self._bluez.listen_for_property_changes(path)
        notify_id = self._bluez.connect_signal(
            Signals.OBJECT_PROPERTIES_CHANGED(path),
            partial(self._on_filter_notification, callback=callback, filter=filter))
        self._notify_ids[char_uuid] = notify_id
        char.StartNotify()

    def disable_notifications(self, char_uuid: str) -> None:
        char, path = self.get_characteristic(char_uuid)
        self._bluez.stop_listening_for_property_changes(path)
        notify_id = self._notify_ids.pop(char_uuid, None)
        if notify_id is not None:
            self._bluez.disconnect_signal(notify_id)
        char.StopNotify()

    def _on_filter_notification(self, changed: dict, invalid: list,
            callback: Callable, filter: list) -> None:

        # we use lower case key names
        changed = {k.lower():v for k, v in changed.items()}
        invalid = [k.lower() for k in invalid]

        kwargs = {}
        if filter is None:
            kwargs = {k:v.unpack() for k, v in changed.items()}
            if invalid:
                kwargs.update({k:None for k in invalid})
        else:
            for key in filter:
                if key in invalid:
                    kwargs[key] = None
                elif key in changed:
                    kwargs[key] = changed[key].unpack()

        if kwargs:
            callback(**kwargs)

    def on_notification(self, **kwargs):
        raise NotImplementedError("You must overwrite this method!")

    def on_connect(self) -> None:
        if self._on_connect_cb:
            self._on_connect_cb()

    def on_connect_failed(self, msg: str) -> None:
        if self._on_fail_cb is not None:
            return self._on_fail_cb(f"Exception: {msg}")
        self._log.warning(" Exception on async connect, but no 'on_fail' callback!"
            f"\nInfo: {msg}")

    def on_disconnect(self) -> None:
        if self._on_disconnect_cb:
            self._on_disconnect_cb()

    def _on_props_changed(self, changed: dict, invalid: list):
        connected = changed.get("Connected")
        if connected is None:
            return

        if connected:
            self.on_connect()
        else:
            self.on_disconnect()

    def get_characteristic(self, char_uuid: str) -> AbstractObjectProxy:
        """
        Handy method to retrieve a D-Bus Interface proxy for the given characteristic.
        Helpful in other gattlib components, not so useful for end-users, as they will
        talk with D-Bus directly.
        """
        path_prefix = self._device.prop("ObjectPath")
        return self._bluez.get_characteristic_by_uuid(path_prefix, char_uuid.lower())

    # FIXME: add missing former-implementation methods:
	# virtual void on_notification(const uint16_t handle, const std::string data);
	# virtual void on_indication(const uint16_t handle, const std::string data);

	# boost::python::object connect_kwarg(boost::python::tuple args, boost::python::dict kwargs);
	# boost::python::object update_connection_parameters_kwarg(boost::python::tuple args, boost::python::dict kwargs);
	# void extract_connection_parameters(PyKwargsExtracter &e);
	# void update_connection_parameters();
	# void exchange_mtu_async(uint16_t mtu, GATTResponse* response);
	# boost::python::object exchange_mtu(uint16_t mtu);
	# void set_mtu(uint16_t mtu);

    # void read_by_handle_async(uint16_t handle, GATTResponse* response);
	# boost::python::object read_by_handle(uint16_t handle);
	# void write_by_handle_async(uint16_t handle, std::string data, GATTResponse* response);

    # void enable_notifications_async(uint16_t handle, bool notifications, bool indications, GATTResponse* response);

    # void discover_primary_async(GATTResponse* response);

    # friend void connect_cb(GIOChannel*, GError*, gpointer);
	# friend gboolean disconnect_cb(GIOChannel* channel, GIOCondition cond, gpointer userp);
	# friend void events_handler(const uint8_t* data, uint16_t size, gpointer userp);

    # boost::python::object find_included(int start = 0x0001, int end = 0xffff);
	# void find_included_async(GATTResponse* response, int start = 0x0001, int end = 0xffff);
    # boost::python::object discover_characteristics(int start = 0x0001, int end = 0xffff, std::string uuid = "");
	# void discover_characteristics_async(GATTResponse* response, int start = 0x0001, int end = 0xffff, std::string uuid = "");

    # boost::python::object discover_descriptors(int start = 0x0001, int end = 0xffff, std::string uuid = "");
	# void discover_descriptors_async(GATTResponse* response, int start = 0x0001, int end = 0xffff, std::string uuid = "");

	# GIOChannel* get_channel() { return _channel; }
