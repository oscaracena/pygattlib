#!/usr/bin/python3
# -*- mode: python; coding: utf-8 -*-

# Copyright (C) 2023, Oscar Acena <oscaracena@gmail.com>
# This software is under the terms of Apache License v2 or later.

from pathlib import Path
from typing import Any, Callable
from threading import Thread, RLock
from uuid import uuid4
from traceback import format_exc
from functools import partial
from weakref import WeakMethod, finalize
from dasbus.connection import SystemMessageBus
from dasbus.client.proxy import AbstractObjectProxy, get_object_path
from dasbus.identifier import DBusServiceIdentifier
from dasbus.loop import EventLoop

# do not remove, used in requester.py
from dasbus.error import DBusError

from .exceptions import (
    DeviceNotFound, AdapterNotFound, ServiceNotFound, CharacteristicNotFound
)
from .utils import log, jprint


ObserverId = str

BLUEZ_SERVICE_NAME = "org.bluez"
SYSTEM_BUS = SystemMessageBus()

class Interfaces:
    ADAPTER = BLUEZ_SERVICE_NAME + ".Adapter1"
    DEVICE = BLUEZ_SERVICE_NAME + ".Device1"
    DBUS_MANAGER = "org.freedesktop.DBus.ObjectManager"
    DBUS_PROPERTIES = "org.freedesktop.DBus.Properties"
    GATT_SERVICE = 'org.bluez.GattService1'
    GATT_CHARACTERISTIC = 'org.bluez.GattCharacteristic1'

class Signals:
    INTERFACES_ADDED = "InterfacesAdded"
    INTERFACES_REMOVED = "InterfacesRemoved"
    PROPERTIES_CHANGED = "PropertiesChanged"
    DEVICE_ADDED = "DeviceAdded"
    DEVICE_REMOVED = "DeviceRemoved"

    @classmethod
    def DEVICE_PROPERTIES_CHANGED(cls, path: str) -> str:
        return f"{cls.PROPERTIES_CHANGED}:{path}"

BLUEZ_MANAGER = DBusServiceIdentifier(
    message_bus = SYSTEM_BUS,
    namespace = BLUEZ_SERVICE_NAME.split(".")
)


class BluezDevice:
    def __init__(self, path: str, spec: dict, monitor: 'BluezMonitor'):
        self._object_path = path
        self._spec = spec
        self._monitor = monitor

        self._proxy = SYSTEM_BUS.get_proxy(
            service_name = BLUEZ_SERVICE_NAME,
            object_path = path,
            interface_name = Interfaces.DEVICE
        )
        self._monitor.keep_device_synced(self)

    def __del__(self):
        self._monitor.stop_device_syncing(self)

    def prop(self, name: str) -> str:
        if name == "ObjectPath":
            return self._object_path
        value = self._spec.get(name)
        if value is None:
            raise KeyError(name)
        return value.unpack()

    def __getattr__(self, name: str) -> Any:
        return getattr(self._proxy, name)


class BluezMonitor(Thread):
    instance = None

    def __init__(self):
        super().__init__()
        self._bus = SYSTEM_BUS
        self._log = log.getChild("BMon")

        self._obs_channels = {}
        self._obs_clients = {}
        self._obs_lock = RLock()
        self._tracked_devices = {}

        # listen for new/removed devices
        self._manager = BLUEZ_MANAGER.get_proxy("/", Interfaces.DBUS_MANAGER)
        self._manager.InterfacesAdded.connect(self._on_ifaces_added)
        self._manager.InterfacesRemoved.connect(self._on_ifaces_removed)

        self._loop = EventLoop()
        self.daemon = True
        self.start()
        self._log.info(" BluezMonitor initialized")

    @classmethod
    def get(cls, *args, **kwargs):
        if cls.instance is None:
            cls.instance = BluezMonitor(*args, **kwargs)
        return cls.instance

    def run(self):
        self._loop.run()

    def stop(self):
        self._loop.quit()

    def connect(self, signal: str, callback: Callable) -> ObserverId:
        with self._obs_lock:
            observers = self._obs_channels.get(signal)
            if observers is None:
                observers = []
                self._obs_channels[signal] = observers

            oid = uuid4().hex
            observers.append(oid)
            self._obs_clients[oid] = WeakMethod(callback)
            finalize(callback.__self__, self._obs_clients.pop, oid).atexit = False
            return oid

    def disconnect(self, observer: ObserverId) -> None:
        with self._obs_lock:
            try:
                self._obs_clients.pop(observer)
            except KeyError:
                pass

    def keep_device_synced(self, device: BluezDevice) -> None:
        # NOTE: do not store a reference to 'device', we need to keep track
        # of its end of life
        object_path = device.prop("ObjectPath")
        props = self._bus.get_proxy(
            service_name = BLUEZ_SERVICE_NAME,
            object_path = object_path,
            interface_name = Interfaces.DBUS_PROPERTIES,
        )

        callback = partial(self._on_device_props_changed, path=object_path)
        props.PropertiesChanged.connect(callback)
        self._tracked_devices[object_path] = (props, device._spec, callback)
        self._log.debug(f" tracking device properties of {object_path}")

    def stop_device_syncing(self, device: BluezDevice) -> None:
        object_path = device.prop("ObjectPath")
        try:
            props, spec, callback = self._tracked_devices.pop(object_path)
            props.PropertiesChanged.disconnect(callback)
        except KeyError:
            pass
        self._log.debug(f" stop tracking device properties of {object_path}")

    def _on_device_props_changed(self, iface: str, changed: dict, invalid: list, path: str) -> None:
        fields = self._tracked_devices.get(path)
        if not fields:
            return

        # notify observer before updating device spec, in order to allow them to retrieve
        # the old property value
        SIGNAL = Signals.DEVICE_PROPERTIES_CHANGED(path)
        self._notify_observers(SIGNAL, changed, invalid)

        spec = fields[1]
        spec.update(changed)

    def _on_ifaces_added(self, path: str, ifaces: dict) -> None:
        self._log.debug(f" new interface added, {path}")
        device = ifaces.get(Interfaces.DEVICE)
        if not device:
            return

        name = device.get("Name", "")
        address = device.get("Address", None)
        if not address:
            return

        name = name.unpack() if name else ""
        address = address.unpack()
        self._notify_observers(Signals.DEVICE_ADDED, name, address)

    def _on_ifaces_removed(self, path: str, ifaces: list) -> None:
        self._log.debug(f" interface removed, {path}")
        if Interfaces.DEVICE not in ifaces:
            return

        address = self._get_addr_from_device_path(path)
        self._notify_observers(Signals.DEVICE_REMOVED, address)

    def _notify_observers(self, signal: str, *args, **kwargs) -> None:
        with self._obs_lock:
            observers = self._obs_channels.get(signal, [])
            for oid in observers[:]:
                callback = self._obs_clients.get(oid)
                if callback is None:
                    observers.remove(oid)
                    continue
                try:
                    if isinstance(callback, WeakMethod):
                        callback = callback()
                    callback(*args, **kwargs)
                except Exception:
                    msg = format_exc()
                    self._log.error(f" in callback (removed from observers list),\n{msg}")
                    observers.remove(oid)
                    self._obs_clients.pop(oid)

    def _get_addr_from_device_path(self, path: str) -> str:
        obj_name = Path(path).stem.lower()
        if obj_name.startswith("dev_"):
            obj_name = obj_name[4:]
        return obj_name.replace("_", ":")


class BluezDBus:
    def __init__(self):
        self._bus = SYSTEM_BUS
        self._manager = BLUEZ_MANAGER.get_proxy("/", Interfaces.DBUS_MANAGER)
        self._monitor = BluezMonitor.get()

        # some method forwarding
        self.connect = self._monitor.connect
        self.disconnect = self._monitor.disconnect

    def find_adapter(self, name: str) -> AbstractObjectProxy:
        objects = self._manager.GetManagedObjects()
        for path, ifaces in objects.items():
            if Interfaces.ADAPTER not in ifaces:
                continue
            if Path(path).stem == name:
                return self._bus.get_proxy(
                    service_name = BLUEZ_SERVICE_NAME,
                    object_path = path,
                    interface_name = Interfaces.ADAPTER,
                )
        raise AdapterNotFound(name)

    def find_device(self, address: str, adapter: str) -> AbstractObjectProxy:
        adapter_prx = self.find_adapter(adapter)
        objects = self._manager.GetManagedObjects()
        path_prefix = get_object_path(adapter_prx)

        address = address.lower()
        for path, ifaces in objects.items():
            device = ifaces.get(Interfaces.DEVICE)
            if device is None:
                continue
            device_address = device.get("Address").unpack().lower()
            if device_address == address and path.startswith(path_prefix):
                return BluezDevice(path, device, self._monitor)

        raise DeviceNotFound(address, adapter)

    def find_gatt_services(self, path_prefix: str = "/", primary: bool = True) -> list:
        objects = self._manager.GetManagedObjects()

        uuids = set()
        for path, ifaces in objects.items():
            if not path.startswith(path_prefix + "/"):
                continue
            service = ifaces.get(Interfaces.GATT_SERVICE)
            if not service:
                continue
            is_primary = service.get("Primary")
            if is_primary is None or is_primary.unpack() != primary:
                continue
            uuids.add(service.get("UUID"))

        return list(uuids)

    def find_gatt_characteristics(self, path_prefix: str, service_uuid: str) -> list:
        objects = self._manager.GetManagedObjects()
        service_path = self._get_service_path(objects, path_prefix, service_uuid)
        if service_path is None:
            raise ServiceNotFound(service_uuid)

        uuids = set()
        for path, ifaces in objects.items():
            if not path.startswith(path_prefix + "/"):
                continue
            char = ifaces.get(Interfaces.GATT_CHARACTERISTIC)
            if not char:
                continue
            service = char.get("Service").unpack()
            if service != service_path:
                continue
            uuids.add(char.get("UUID"))

        return list(uuids)

    def get_characteristic(self, path_prefix: str, char_uuid: str) -> AbstractObjectProxy:
        objects = self._manager.GetManagedObjects()
        for path, ifaces in objects.items():
            if not path.startswith(path_prefix + "/"):
                continue
            char = ifaces.get(Interfaces.GATT_CHARACTERISTIC)
            if not char:
                continue
            if char.get("UUID").unpack() != char_uuid:
                continue

            return self._bus.get_proxy(
                service_name = BLUEZ_SERVICE_NAME,
                object_path = path,
                interface_name = Interfaces.GATT_CHARACTERISTIC,
            )

        raise CharacteristicNotFound(char_uuid)

    def _get_service_path(self, objects: list, prefix: str, uuid: str) -> str:
        for path, ifaces in objects.items():
            if not path.startswith(prefix + "/"):
                continue
            service = ifaces.get(Interfaces.GATT_SERVICE)
            if not service:
                continue
            if service.get("UUID").unpack() == uuid:
                return path
