#!/usr/bin/python3
# -*- mode: python; coding: utf-8 -*-

# Copyright (C) 2023, Oscar Acena <oscaracena@gmail.com>
# This software is under the terms of Apache License v2 or later.

from threading import Thread
from typing import Callable

from .dbus import BluezDBus, DBusError, Signals
from .exceptions import BTIOException
from .utils import log, deprecated, wrap_exception


class GATTRequester:
    @deprecated(do_connect="auto_connect", device="adapter")
    def __init__(self, address: str, auto_connect: bool = True, adapter: str = "hci0"):
        self._bluez = BluezDBus()
        self._device = self._bluez.find_device(address, adapter)
        self._log = log.getChild("GATTReq")

        obj_path = self._device.prop("ObjectPath")
        self._bluez.connect(
            Signals.DEVICE_PROPERTIES_CHANGED(obj_path), self._on_props_changed)

        self._on_connect_cb = None
        self._on_fail_cb = None

        if auto_connect:
            self.connect()

    @deprecated(channel_type=None, security_level=None, psm=None, mtu=None)
    @wrap_exception(DBusError, BTIOException)
    def connect(self, wait: bool = False, on_connect: Callable = None,
                on_fail: Callable = None, on_disconnect: Callable = None) -> None:

        self._on_connect_cb = on_connect
        self._on_fail_cb = on_fail
        self._on_disconnect_cb = on_disconnect

        def _do_connect():
            try:
                already_connected = self.is_connected()
                self._device.Connect()

                # If was already connected, the property will not change, and
                # the callback will not be called. Call it here.
                if already_connected:
                    self.on_connect()
            except Exception as ex:
                self.on_connect_failed(str(ex))

        if not wait:
            Thread(target=_do_connect, daemon=True).start()
        else:
            _do_connect()

    def is_connected(self) -> bool:
        return self._device.prop("Connected")

    def disconnect(self) -> None:
        self._device.Disconnect()

    def on_connect(self) -> None:
        if self._on_connect_cb:
            self._on_connect_cb()

    def on_connect_failed(self, msg: str) -> None:
        if self._on_fail_cb is not None:
            return self._on_fail_cb(f"Exception: {msg}")
        self._log.warning(" Exception on async connect, but no 'on_fail' callback!")

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

    # FIXME: add missing former-implementation methods:
	# virtual void on_notification(const uint16_t handle, const std::string data);
	# virtual void on_indication(const uint16_t handle, const std::string data);

	# static boost::python::object connect_kwarg(boost::python::tuple args, boost::python::dict kwargs);
	# static boost::python::object update_connection_parameters_kwarg(boost::python::tuple args, boost::python::dict kwargs);
	# void extract_connection_parameters(PyKwargsExtracter &e);
	# void update_connection_parameters();
	# void exchange_mtu_async(uint16_t mtu, GATTResponse* response);
	# boost::python::object exchange_mtu(uint16_t mtu);
	# void set_mtu(uint16_t mtu);
	# void read_by_handle_async(uint16_t handle, GATTResponse* response);
	# boost::python::object read_by_handle(uint16_t handle);
	# void read_by_uuid_async(std::string uuid, GATTResponse* response);
	# boost::python::object read_by_uuid(std::string uuid);
	# void write_by_handle_async(uint16_t handle, std::string data, GATTResponse* response);
    # boost::python::object write_by_handle(uint16_t handle, std::string data);
	# void write_cmd(uint16_t handle, std::string data);
	# void enable_notifications_async(uint16_t handle, bool notifications, bool indications, GATTResponse* response);
	# void enable_notifications(uint16_t handle, bool notifications, bool indications);

	# friend void connect_cb(GIOChannel*, GError*, gpointer);
	# friend gboolean disconnect_cb(GIOChannel* channel, GIOCondition cond, gpointer userp);
	# friend void events_handler(const uint8_t* data, uint16_t size, gpointer userp);

	# boost::python::object discover_primary();
	# void discover_primary_async(GATTResponse* response);
	# boost::python::object find_included(int start = 0x0001, int end = 0xffff);
	# void find_included_async(GATTResponse* response, int start = 0x0001, int end = 0xffff);
	# boost::python::object discover_characteristics(int start = 0x0001, int end = 0xffff, std::string uuid = "");
	# void discover_characteristics_async(GATTResponse* response, int start = 0x0001, int end = 0xffff, std::string uuid = "");
	# boost::python::object discover_descriptors(int start = 0x0001, int end = 0xffff, std::string uuid = "");
	# void discover_descriptors_async(GATTResponse* response, int start = 0x0001, int end = 0xffff, std::string uuid = "");

	# GIOChannel* get_channel() { return _channel; }