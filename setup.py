#!/usr/bin/python
# -*- mode: python; coding: utf-8 -*-

import os
import sys
import subprocess
import platform
from distutils.core import setup, Extension

extension_modules = list()

def find_MS_SDK():
    candidate_roots = (os.getenv('ProgramFiles'), os.getenv('ProgramW6432'),
            os.getenv('ProgramFiles(x86)'))

    if sys.version < '3.3':
        MS_SDK = r'Microsoft SDKs\Windows\v6.0A' # Visual Studio 9
    else:
        MS_SDK = r'Microsoft SDKs\Windows\v7.0A' # Visual Studio 10

    candidate_paths = (
            MS_SDK,
            'Microsoft Platform SDK for Windows XP',
            'Microsoft Platform SDK',
    )

    for candidate_root in candidate_roots:
        for candidate_path in candidate_paths:
            candidate_sdk = os.path.join(candidate_root, candidate_path)
            if os.path.exists(candidate_sdk):
                return candidate_sdk

if sys.platform == 'win32':
    PSDK_PATH = find_MS_SDK()
    if PSDK_PATH is None:
        raise SystemExit("Could not find the Windows Platform SDK")

    lib_path = os.path.join(PSDK_PATH, 'Lib')
    if '64' in platform.architecture()[0]:
        lib_path = os.path.join(lib_path, 'x64')

    ext_mod = Extension ('gattlib',
                        include_dirs = [
                                "%s/Include" % PSDK_PATH,
                                "./win",
                                "c:\\local\\boost_1_58_0\\"],
                        library_dirs = [lib_path, "c:\\local\\boost_1_58_0\\stage\\lib\\"],
                        extra_compile_args = [ '/EHsc' ],
                        sources=['win/bindings.cpp', 'win/gattlib.cpp', 'win/gattservices.cpp'],)
    extension_modules = [ ext_mod ]
elif sys.platform.startswith('linux'):
    glib_headers = subprocess.check_output(
        "pkg-config --cflags glib-2.0".split()).decode('utf-8')
    glib_headers = glib_headers.strip().split("-I")
    glib_headers = [x.strip() for x in glib_headers if x]

    glib_libs = subprocess.check_output(
        "pkg-config --libs glib-2.0".split()).decode('utf-8')
    glib_libs = glib_libs.strip().split("-l")
    glib_libs = [x.strip() for x in glib_libs if x]

    if sys.version_info.major == 3:
        boost_libs = ["boost_python-py34"]
    else:
        boost_libs = ["boost_python"]
    extension_modules = [
        Extension(
            'gattlib',
            ['src/gattservices.cpp',
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

            libraries = glib_libs + boost_libs + [
                "boost_thread", "bluetooth"
            ],

            include_dirs = glib_headers + [
                'src/bluez',
            ],

            define_macros = [('VERSION', '"5.25"')],
        )
    ],

    
setup(
    name = 'gattlib',
    version = "0.20150131",
    description = "Library to access Bluetooth LE devices",
    author = "Oscar Acena",
    author_email = "oscar.acena@gmail.com",
    url = "https://bitbucket.org/OscarAcena/pygattlib",
    ext_modules = extension_modules,
)
