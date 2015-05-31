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
	void enable_beacon(const std::string uuid, int major, int minor,
	        int txpower, int interval);
    void disable_beacon();

protected:
	void process_input(unsigned char* buffer, int size,
			boost::python::dict & ret);

};

#endif // _BEACON_H_
