# -*- mode: makefile-gmake; coding: utf-8 -*-

# Copyright (C) 2023, Oscar Acena <oscaracena@gmail.com>
# This software is under the terms of Apache License v2 or later.

all:

python-wheel:
	python3 -m build --wheel

pypi-test-upload:
	python3 setup.py sdist
	twine upload --repository testpypi dist/*
	@echo "pip install -i https://test.pypi.org/simple/ gattlib --force-reinstall"

pypi-upload:
	python3 setup.py sdist
	twine upload dist/*

# NOTE: this rule is used by 'ian' to create the Debian package. To directly install,
# use pip instead.
install:
	python3 setup.py install \
	    --prefix=$(DESTDIR)/usr/ \
		--root=/ \
		--single-version-externally-managed \
	    --no-compile \
	    --install-layout=deb

package: clean
	ian build -c

.PHONY: clean
clean:
	find . -name "*.pyc" -print -delete
	find . -name "__pycache__" -print -delete
	$(RM) -fr build dist MANIFEST *.egg-info
