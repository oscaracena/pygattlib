#!/usr/bin/python3
# -*- mode: python; coding: utf-8 -*-

# Copyright (C) 2023, Oscar Acena <oscaracena@gmail.com>
# This software is under the terms of Apache License v2 or later.

import json
import warnings
import logging


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


_ch = logging.StreamHandler()
_ch.setFormatter(LoggingCustomFormatter())

log = logging.getLogger("PyGattLib")
log.propagate = False
log.setLevel(logging.DEBUG)
log.addHandler(_ch)

# NOTE: remove this in production
logging.basicConfig()


def jprint(data):
    defvalue = lambda o: o.unpack()
    print(json.dumps(data, ensure_ascii=False, indent=4, default=defvalue))


# This is a decorator used to deprecate arguments that have changed its name, or not
# supported anymore. Usage:
#
#   @deprecated(old_name="new_name", old_unsupported_param=None)
#   def my_func(new_name):
#       ...
#
def deprecated(**fields):
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
