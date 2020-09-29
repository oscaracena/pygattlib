# -*- mode: makefile-gmake; coding: utf-8 -*-

all:
	$(MAKE) -C src $@

pypi-test-upload:
	python3 setup.py sdist
	twine upload --repository testpypi dist/*
	@echo "pip install -i https://test.pypi.org/simple/ gattlib --force-reinstall"

pypi-upload:
	python3 setup.py sdist
	twine upload dist/*

%:
	$(MAKE) -C src $@

.PHONY: clean
clean:
	$(MAKE) -C src $@
	$(RM) -fr build dist MANIFEST gattlib.egg-info
