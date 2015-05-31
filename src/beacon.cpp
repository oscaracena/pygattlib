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
BeaconService::enable_scan_mode() {
	int result;
	uint8_t scan_type = 0x01;
	uint16_t interval = htobs(0x0010);
	uint16_t window = htobs(0x0010);
	uint8_t own_type = 0x00;
	uint8_t filter_policy = 0x00;

	result = hci_le_set_scan_parameters
		(_device_desc, scan_type, interval, window,
		 own_type, filter_policy, 10000);

	if (result < 0)
		throw std::runtime_error
			("Set scan parameters failed (are you root?)");

	result = hci_le_set_scan_enable(_device_desc, 0x01, 1, 10000);
	if (result < 0)
		throw std::runtime_error("Enable scan failed");
}


BeaconDict
BeaconService::get_advertisements(int timeout) {
	struct hci_filter old_options;
	socklen_t slen = sizeof(old_options);
	if (getsockopt(_device_desc, SOL_HCI, HCI_FILTER,
				   &old_options, &slen) < 0)
		throw std::runtime_error("Could not get socket options");

	struct hci_filter new_options;
	hci_filter_clear(&new_options);
	hci_filter_set_ptype(HCI_EVENT_PKT, &new_options);
	hci_filter_set_event(EVT_LE_META_EVENT, &new_options);

	if (setsockopt(_device_desc, SOL_HCI, HCI_FILTER,
				   &new_options, sizeof(new_options)) < 0)
		throw std::runtime_error("Could not set socket options\n");

	int len;
	unsigned char buffer[HCI_MAX_EVENT_SIZE];
	struct timeval wait;
	fd_set read_set;
	wait.tv_sec = timeout;
	int ts = time(NULL);

	BeaconDict retval;
	while(1) {
		FD_ZERO(&read_set);
		FD_SET(_device_desc, &read_set);

		int ret = select(FD_SETSIZE, &read_set, NULL, NULL, &wait);
		if (ret <= 0)
			break;

		len = read(_device_desc, buffer, sizeof(buffer));
        if(len != BEACON_LE_ADVERTISING_LEN) continue;
        AddrBeaconPair info = process_input(buffer, len);
		if (not retval.count(info.first) and not info.first.empty())
			retval.insert(info);


		int elapsed = time(NULL) - ts;
		if (elapsed >= timeout)
			break;

		wait.tv_sec = timeout - elapsed;
	}

	setsockopt(_device_desc, SOL_HCI, HCI_FILTER,
			   &old_options, sizeof(old_options));
	return retval;
}

AddrBeaconPair
BeaconService::process_input(unsigned char* buffer, int size) {
	unsigned char* ptr = buffer + HCI_EVENT_HDR_SIZE + 1;
	evt_le_meta_event* meta = (evt_le_meta_event*) ptr;

	if (meta->subevent != EVT_LE_ADVERTISING_REPORT
	        || (uint8_t)buffer[BLE_EVENT_TYPE] != LE_META_EVENT) {
		return AddrBeaconPair();
	}

    le_advertising_info* info = (le_advertising_info*) (meta->data + 1);
	beacon_adv* beacon_info = (beacon_adv*) (info->data + 5);

	if(beacon_info->company_id != BEACON_COMPANY_ID
			|| beacon_info->type != BEACON_TYPE
			|| beacon_info->data_len != BEACON_DATA_LEN) {
		return AddrBeaconPair();
	}

	char addr[18];
	ba2str(&info->bdaddr, addr);

    return AddrBeaconPair(addr, *beacon_info);
}


void
BeaconService::disable_scan_mode() {
	if (_device_desc == -1)
		throw std::runtime_error("Could not disable scan, not enabled yet");

	int result = hci_le_set_scan_enable(_device_desc, 0x00, 1, 10000);
	if (result < 0)
		throw std::runtime_error("Disable scan failed");
}

boost::python::dict
BeaconService::scan(int timeout) {
	enable_scan_mode();
	BeaconDict devices = get_advertisements(timeout);
	disable_scan_mode();

	boost::python::dict retval;
	for (BeaconDict::iterator i = devices.begin(); i != devices.end(); i++) {
		boost::python::list retn;

		//uuid bytes to string conversion
		char uuid[MAX_LEN_UUID_STR + 1];
		uuid[MAX_LEN_UUID_STR] = '\0';
		bt_uuid_t btuuid;
		bt_uuid128_create(&btuuid, i->second.uuid);
		bt_uuid_to_string(&btuuid, uuid, sizeof(uuid));

		retn.append(uuid);
		retn.append(i->second.major);
		retn.append(i->second.minor);
		retn.append(i->second.power);
		retn.append(i->second.rssi);
		retval[i->first] = retn;
	}

	return retval;
}
