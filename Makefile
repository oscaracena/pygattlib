# -*- mode: makefile-gmake; coding: utf-8 -*-

# Copyright (C) 2023, Oscar Acena <oscaracena@gmail.com>
# This software is under the terms of Apache License v2 or later.

DESTDIR  ?= ~
LIB_NAME  = python3-gattlib-dbus


all:

python-wheel:
	python3 -m build --wheel

pypi-test-upload:
	python3 setup.py sdist
	twine upload --repository testpypi dist/*
	@echo "pip install -i https://test.pypi.org/simple/ gattlib-dbus --force-reinstall"

pypi-upload:
	python3 setup.py sdist
	twine upload dist/*

# NOTE: this rule is used by 'ian' to create the Debian package. To directly install,
# use pip instead.
install: wheel
	install -vd $(DESTDIR)/usr/share/$(LIB_NAME)/dist/
	install -v -m 444 dist/*.whl $(DESTDIR)/usr/share/$(LIB_NAME)/dist/

	python3 setup.py install \
	    --prefix=$(DESTDIR)/usr/ \
		--root=/ \
		--single-version-externally-managed \
	    --no-compile \
	    --install-layout=deb

wheel:
	python3 -m build --wheel || \
		(echo "\n### NOTE: Make sure that python3-build is installed.\n" && \
		false)

package: clean
	ian build -c

.PHONY: clean
clean:
	find . -name "*.pyc" -print -delete
	find . -name "__pycache__" -print -delete
	$(RM) -fr build dist MANIFEST *.egg-info
