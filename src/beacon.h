// -*- mode: c++; coding: utf-8; tab-width: 4 -*-

// Copyright (C) 2014, Oscar Acena <oscaracena@gmail.com>
// This software is under the terms of GPLv3 or later.

#ifndef _BEACON_H_
#define _BEACON_H_

#include "gattservices.h"


#define LE_META_EVENT 0x0
#define EVT_LE_ADVERTISING_REPORT 0x02
#define BEACON_LE_ADVERTISING_LEN 45
#define BEACON_COMPANY_ID 0x004c
#define BEACON_TYPE 0x02
#define BEACON_DATA_LEN 0x15

typedef struct {
    uint16_t company_id;
    uint8_t type;
    uint8_t data_len;
    uint128_t uuid;
    uint16_t major;
    uint16_t minor;
    uint8_t power;
    int8_t rssi;
} beacon_adv;
typedef std::pair<std::string, beacon_adv> AddrBeaconPair;
typedef std::map<std::string, beacon_adv> BeaconDict;


class BeaconService : public DiscoveryService {
public:
    BeaconService(const std::string device);
	boost::python::dict scan(int timeout);

private:
	void enable_scan_mode();
	BeaconDict get_advertisements(int timeout);
	AddrBeaconPair process_input(unsigned char* buffer, int size);
	void disable_scan_mode();

};

#endif // _BEACON_H_
