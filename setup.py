#!/usr/bin/python3
# -*- mode: python; coding: utf-8 -*-

import os
import sys
import subprocess
from setuptools import setup, Extension
# from versions import get_boost_version

extension_modules = []


def get_boost_version(out=None):
    if out is None:
        out = subprocess.check_output(
            r"ldconfig -p | grep -E 'libboost_python.*\.so '", shell=True)

    ver = os.path.splitext(out.split()[0][3:])[0].decode()
    return ver

def tests():
    # case: python3-py3x.so
    out = b"""
        libboost_python3-py36.so (libc6,x86-64) => /usr/lib/x86_64-linux-gnu/libboost_python3-py36.so
        libboost_python-py27.so (libc6,x86-64) => /usr/lib/x86_64-linux-gnu/libboost_python-py27.so
    """
    got = get_boost_version(out)
    expected = 'boost_python3-py36'
    assert got == expected, "Case 1: got: '{}'".format(got)

    # case: python3x.so
    out = b"""
        libboost_python38.so (libc6,x86-64) => /lib/x86_64-linux-gnu/libboost_python38.so
    """
    got = get_boost_version(out)
    expected = 'boost_python38'
    assert got == expected, "Case 2: got: '{}'".format(got)

    # case: python-py3x
    out = b"""
	    libboost_python-py35.so (libc6,hard-float) => /usr/lib/arm-linux-gnueabihf/libboost_python-py35.so
	    libboost_python-py27.so (libc6,hard-float) => /usr/lib/arm-linux-gnueabihf/libboost_python-py27.so
    """
    got = get_boost_version(out)
    expected = 'boost_python-py35'
    assert got == expected, "Case 3: got: '{}'".format(got)

    print("OK, all seems fine.")


if sys.platform.startswith('linux'):
    glib_headers = subprocess.check_output(
        "pkg-config --cflags glib-2.0".split()).decode('utf-8')
    glib_headers = glib_headers.strip().split("-I")
    glib_headers = [x.strip() for x in glib_headers if x]

    glib_libs = subprocess.check_output(
        "pkg-config --libs glib-2.0".split()).decode('utf-8')
    glib_libs = glib_libs.strip().split("-l")
    glib_libs = [x.strip() for x in glib_libs if x]

    boost_py = get_boost_version()

    extension_modules = [
        Extension(
            'gattlib',
            ['src/gattservices.cpp',
             'src/beacon.cpp',
             'src/bindings.cpp',
             'src/gattlib.cpp',
             'src/bluez/lib/uuid.c',
             'src/bluez/attrib/gatt.c',
             'src/bluez/attrib/gattrib.c',
             'src/bluez/attrib/utils.c',
             'src/bluez/attrib/att.c',
             'src/bluez/src/shared/crypto.c',
             'src/bluez/src/log.c',
             'src/bluez/btio/btio.c'],

            libraries=glib_libs + [boost_py, "boost_thread", "bluetooth"],
            include_dirs=glib_headers + ['src/bluez'],
            define_macros=[('VERSION', '"5.25"')]
        )
    ]
else:
    raise OSError("Not supported OS")


if __name__ == "__main__":
    if len(sys.argv) >= 2:
        if sys.argv[1] == "--get-boost-lib":
            print(get_boost_version())
            sys.exit(0)
        if sys.argv[1] == "--run-test":
            tests()
            sys.exit(0)

with open("README.md", "r") as fh:
    long_description = fh.read()

setup(
    name='gattlib',
    version="0.20201113",
    description="Library to access Bluetooth LE devices",
    long_description=long_description,
    long_description_content_type="text/markdown",
    author="Oscar Acena",
    author_email="oscar.acena@gmail.com",
    url="https://github.com/oscaracena/pygattlib",
    ext_modules=extension_modules,
)
