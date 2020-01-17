# -*- mode: makefile-gmake; coding: utf-8 -*-

all:
	$(MAKE) -C src $@

pypi-upload:
	python3 setup.py sdist
	twine upload dist/*

%:
	$(MAKE) -C src $@

.PHONY: clean
clean:
	$(MAKE) -C src $@
	$(RM) -fr dist MANIFEST gattlib.egg-info
