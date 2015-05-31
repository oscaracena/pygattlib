// -*- mode: c++; coding: utf-8; tab-width: 4 -*-

// Copyright (C) 2014, Oscar Acena <oscaracena@gmail.com>
// This software is under the terms of GPLv3 or later.

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

extern "C" {
#include "lib/uuid.h"
}

#include <exception>
#include <iostream>

#include "beacon.h"

BeaconService::BeaconService(const std::string device = "hci0")
        : DiscoveryService(device) {}


void
BeaconService::process_input(unsigned char* buffer, int size,
		boost::python::dict & ret) {
	if(size != BEACON_LE_ADVERTISING_LEN) return;

	unsigned char* ptr = buffer + HCI_EVENT_HDR_SIZE + 1;
	evt_le_meta_event* meta = (evt_le_meta_event*) ptr;

	if (meta->subevent != EVT_LE_ADVERTISING_REPORT
	        || (uint8_t)buffer[BLE_EVENT_TYPE] != LE_META_EVENT) {
		return;
	}

    le_advertising_info* info = (le_advertising_info*) (meta->data + 1);
	beacon_adv* beacon_info = (beacon_adv*) (info->data + 5);

	if(beacon_info->company_id != BEACON_COMPANY_ID
			|| beacon_info->type != BEACON_TYPE
			|| beacon_info->data_len != BEACON_DATA_LEN) {
		return;
	}

	char addr[18];
	ba2str(&info->bdaddr, addr);
	boost::python::list data;

	//uuid bytes to string conversion
	char uuid[MAX_LEN_UUID_STR + 1];
	uuid[MAX_LEN_UUID_STR] = '\0';
	bt_uuid_t btuuid;
	bt_uuid128_create(&btuuid, beacon_info->uuid);
	bt_uuid_to_string(&btuuid, uuid, sizeof(uuid));

	data.append(uuid);
	data.append(beacon_info->major);
	data.append(beacon_info->minor);
	data.append(beacon_info->power);
	data.append(beacon_info->rssi);
	ret[addr] = data;
}


boost::python::dict
BeaconService::scan(int timeout) {
	boost::python::dict retval;

	enable_scan_mode();
	get_advertisements(timeout, retval);
	disable_scan_mode();

	return retval;
}
