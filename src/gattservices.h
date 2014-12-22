// -*- mode: c++; coding: utf-8; tab-width: 4 -*-

// Copyright (C) 2014, Oscar Acena <oscar.acena@uclm.es>
// This software is under the terms of GPLv3 or later.

#ifndef _GATTSERVICES_H_
#define _GATTSERVICES_H_

#include <boost/python/dict.hpp>
#include <map>

#define EIR_NAME_SHORT     0x08  /* shortened local name */
#define EIR_NAME_COMPLETE  0x09  /* complete local name */

typedef std::pair<std::string, std::string> StringPair;
typedef std::map<std::string, std::string> StringDict;

class DiscoveryService {
public:
	DiscoveryService(const std::string device);
	~DiscoveryService();
	boost::python::dict discover(int timeout);


private:
	void enable_scan_mode();
	StringDict get_advertisements(int timeout);
	StringPair process_input(unsigned char* buffer, int size);
	std::string parse_name(uint8_t* data, size_t size);
	void disable_scan_mode();

	std::string _device;
	int _device_desc;
	int _timeout;
};

#endif // _GATTSERVICES_H_
