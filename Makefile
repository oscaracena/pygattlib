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

.PHONY: clean
clean:
	find . -name "*.pyc" -print -delete
	find . -name "__pycache__" -print -delete
	$(RM) -fr build dist MANIFEST gattlib.egg-info
