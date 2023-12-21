# -*- mode: python; coding: utf-8 -*-

# Copyright (C) 2023, Oscar Acena <oscaracena@gmail.com>
# This software is under the terms of Apache License v2 or later.

import json
import warnings
import logging
from functools import partial
from weakref import WeakMethod, finalize, ref as weakref
from inspect import ismethod

from .dbus import get_variant


# Code from: https://stackoverflow.com/a/56944256/870503
class LoggingCustomFormatter(logging.Formatter):
    grey = "\x1b[90m"
    white = ""
    yellow = "\x1b[33;20m"
    red = "\x1b[31;20m"
    bold_red = "\x1b[31;1m"
    reset = "\x1b[0m"
    format = "%(levelname)s:%(name)s:%(message)s"

    FORMATS = {
        logging.DEBUG: grey + format + reset,
        logging.INFO: white + format + reset,
        logging.WARNING: yellow + format + reset,
        logging.ERROR: red + format + reset,
        logging.CRITICAL: bold_red + format + reset
    }

    def format(self, record):
        log_fmt = self.FORMATS.get(record.levelno)
        formatter = logging.Formatter(log_fmt)
        return formatter.format(record)


def get_colour_logger(name):
    sh = logging.StreamHandler()
    sh.setFormatter(LoggingCustomFormatter())

    log = logging.getLogger(name)
    log.propagate = False
    log.addHandler(sh)
    return log


log = get_colour_logger("PyGattLib")


def jprint(data):
    defvalue = lambda o: o.unpack()
    print(json.dumps(data, ensure_ascii=False, indent=4, default=defvalue))


# This is a decorator used to deprecate arguments that have changed its name, or not
# supported anymore. Usage:
#
#   @deprecated_args(old_name="new_name", old_unsupported_param=None)
#   def my_func(new_name):
#       ...
#
def deprecated_args(**fields):
    def _deprecated_decorator(fn):
        def _decorator(*args, **kwargs):
            for k, v in fields.items():
                if k in kwargs:
                    user_v = kwargs.pop(k)
                    if v is None:
                        warnings.warn(f"'{k}' is deprecated (no longer supported).",
                            DeprecationWarning, stacklevel=2)
                    else:
                        warnings.warn(f"'{k}' is deprecated, use '{v}' instead.",
                            DeprecationWarning, stacklevel=2)
                        kwargs[v] = user_v
            return fn(*args, **kwargs)
        return _decorator
    return _deprecated_decorator


# This is a decorator used to convert exception domains (from SrcException to DstException)
# Use it when the code may raise an unknown exception for the user, to wrap it directly
# to a known exception class.
def wrap_exception(SrcException, DstException):
    def _wrapped(fn):
        def _deco(*args, **kwargs):
            try:
                return fn(*args, **kwargs)
            except SrcException as ex:
                msg = f"{ex.__class__} error: {ex}"
                raise DstException(msg) from None
        return _deco
    return _wrapped


def options(opts):
    return {k:get_variant(type(v), v) for k,v in opts.items()}


class WeakCallback:
    """
    Holds a method or a partial to a method using a weak reference.
    """
    def __init__(self, callback):
        self._target_object = None
        self._callable = None

        if ismethod(callback):
            self._callable = WeakMethod(callback)
            self._target_object = weakref(callback.__self__)
        elif isinstance(callback, partial):
            if ismethod(callback.func):
                func = WeakMethod(callback.func)
                self._callable = partial(func, *callback.args, **callback.keywords)
                self._target_object = weakref(callback.func.__self__)

        if self._callable is None:
            raise TypeError("Invalid callback type, use a method, or a partial with a method.")

    def __call__(self, *args, **kwargs):
        method = self._callable

        if isinstance(method, WeakMethod):
            method = method()
        elif isinstance(method, partial):
            func = method.func()
            if func is None:
                method = None
            else:
                method = partial(func, *method.args, **method.keywords)

        if method is None:
            raise RuntimeError("Referenced object does not exist anymore!")
        return method(*args, **kwargs)

    def finalize(self, func, *args, **kwargs):
        return finalize(self._target_object(), func, *args, **kwargs)
