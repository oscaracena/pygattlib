# -*- mode: python; coding: utf-8 -*-

# Copyright (C) 2023,2024 Oscar Acena <oscaracena@gmail.com>
# This software is under the terms of Apache License v2 or later.

import time
from traceback import format_exc
from typing import Optional, Dict, Callable

from .dbus import BluezDBus, Signals
from .utils import log, deprecated_args, options


Name = str
Address = str
Devices = Dict[Name, Address]


class DiscoveryService:
    @deprecated_args(device="adapter")
    def __init__(self, adapter: str = "hci0"):
        self._bluez = BluezDBus()
        self._adapter = self._bluez.find_adapter(adapter)
        self._adapter.SetDiscoveryFilter(options({"Transport": "le"}))
        self._log = log.getChild("DS")

        self._devices = {}
        self._callback = None
        self._running = False
        self._obs_id = None

    def discover(self, timeout: int = 0, callback: Callable = None) -> Optional[Devices]:
        """
        If timeout > 0, it will take that time to make a discovery, and return
        the discovered devices. If callback is given, it will also call it on each
        advertisement.
        If timeout <= 0, you must provide a callback, as it will return inmediatly.
        To stop an asyncronous discovery, call `stop()`.
        """

        # FIXME: add support for 'removed-device' event, but without breaking the old
        # interface (if possible)

        if self._running:
            raise RuntimeError("Discover already running!")
        if timeout <= 0 and callback is None:
            raise ValueError("You must set a timeout or provide a callback.")

        self._devices = {}
        self._callback = callback
        self._obs_id = self._bluez.connect_signal(Signals.DEVICE_ADDED, self._on_new_device)
        self._adapter.StartDiscovery()
        self._running = True
        self._log.info(" discover launched")

        if timeout > 0:
            time.sleep(timeout)
            self.stop()
            return self._devices

    def stop(self) -> None:
        self._adapter.StopDiscovery()
        self._bluez.disconnect_signal(self._obs_id)
        self._running = False
        self._log.info(" discover stopped")

    def _on_new_device(self, name: str, address: str) -> None:
        self._devices[address] = name

        if self._callback:
            try:
                self._callback(name, address)
            except Exception:
                msg = format_exc()
                self._log.error(f" in callback,\n{msg}")

    def _on_delete_device(self, address: str) -> None:
        try:
            self._devices.pop(address)
        except KeyError:
            return
