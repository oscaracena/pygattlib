#!/usr/bin/python3
# -*- mode: python; coding: utf-8 -*-

# Copyright (C) 2023, Oscar Acena <oscaracena@gmail.com>
# This software is under the terms of Apache License v2 or later.

from threading import Thread
from typing import Callable

from .dbus import BluezDBus
from .utils import log, deprecated


class GATTRequester:
    @deprecated(do_connect="auto_connect", device="adapter")
    def __init__(self, address: str, auto_connect: bool = True, adapter: str = "hci0"):
        self._bluez = BluezDBus()
        self._device = self._bluez.find_device(address, adapter)

        if auto_connect:
            self.connect()

    @deprecated(channel_type=None, security_level=None, psm=None, mtu=None)
    def connect(self, wait: bool = False, on_success: Callable = None) -> None:
        if wait:
            return self._device.Connect()
        Thread(target=self._device.Connect, daemon=True).start()

    def is_connected(self) -> bool:
        return self._device.prop("Connected")

    def disconnect(self) -> None:
        self._device.Disconnect()

    def on_connect(self) -> None:
        pass

    def on_connect_failed(self) -> None:
        pass

    def on_disconnect(self) -> None:
        pass

    # FIXME: add missing former-implementation methods:
	# virtual void on_connect(int mtu) { }
	# virtual void on_connect_failed(int code) { }
	# virtual void on_disconnect() { }

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