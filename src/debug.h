// -*- mode: c++; coding: utf-8; tab-width: 4 -*-

// Copyright (C) 2014, Oscar Acena <oscar.acena@uclm.es>
// This software is under the terms of GPLv3 or later.

#ifndef _MIBAND_DEBUG_H_
#define _MIBAND_DEBUG_H_

static void
hexdump(std::string data) {
    const char* c = data.data();
	uint16_t size = data.length();

	std::cout << size << " bytes: ";
    for (unsigned int i=0; i<size; i++) {
		int d = (int)*(c + i) & 0xff;
		std::cout << std::hex << d << " ";
    }
    std::cout << std::endl;
}

#endif // _MIBAND_DEBUG_H_
