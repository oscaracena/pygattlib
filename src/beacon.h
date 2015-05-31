// -*- mode: c++; coding: utf-8; tab-width: 4 -*-

// Copyright (C) 2014, Oscar Acena <oscaracena@gmail.com>
// This software is under the terms of GPLv3 or later.

#ifndef _BEACON_H_
#define _BEACON_H_

#include "gattservices.h"

class BeaconService : public DiscoveryService {
public:
    BeaconService(const std::string device);
	boost::python::dict scan(int timeout);

private:
	void enable_scan_mode();
	StringDict get_advertisements(int timeout);
	StringPair process_input(unsigned char* buffer, int size);
	void disable_scan_mode();

};

#endif // _BEACON_H_
