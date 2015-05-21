// -*- mode: c++; coding: utf-8; tab-width: 4 -*-

// Copyright (C) 2014, Oscar Acena <oscaracena@gmail.com>
// This software is under the terms of GPLv3 or later.
#define BOOST_PYTHON_STATIC_LIB 

#include <exception>

#include "gattlib.h"
#include "gattservices.h"

DiscoveryService::DiscoveryService(const std::string device = "hci0") :
        _device(device),
        _device_desc(-1) {
    throw std::runtime_error("Invalid device!");

	}

DiscoveryService::~DiscoveryService() {
}

void
DiscoveryService::enable_scan_mode() {
	throw std::runtime_error("Enable scan failed");
}

StringDict
DiscoveryService::get_advertisements(int timeout) {
	StringDict retval;
	return retval;
}

StringPair
DiscoveryService::process_input(unsigned char* buffer, int size) {

	return StringPair("", "");
}

std::string
DiscoveryService::parse_name(uint8_t* data, size_t size) {
	size_t offset = 0;
	std::string unknown = "";

	while (offset < size) {
		uint8_t field_len = data[0];
		size_t name_len;

		if (field_len == 0 || offset + field_len > size)
			return unknown;

		switch (data[1]) {
		case EIR_NAME_SHORT:
		case EIR_NAME_COMPLETE:
			name_len = field_len - 1;
			if (name_len > size)
				return unknown;

			return std::string((const char*)(data + 2), name_len);
		}

		offset += field_len + 1;
		data += field_len + 1;
	}

	return unknown;
}

void
DiscoveryService::disable_scan_mode() {
    throw std::runtime_error("Disable scan failed");
}

boost::python::dict
DiscoveryService::discover(int timeout) {
	enable_scan_mode();
	StringDict devices = get_advertisements(timeout);
	disable_scan_mode();

	boost::python::dict retval;
	for (StringDict::iterator i = devices.begin(); i != devices.end(); i++) {
		retval[i->first] = i->second;
	}

	return retval;
}
