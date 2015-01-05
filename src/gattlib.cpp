// -*- mode: c++; coding: utf-8; tab-width: 4 -*-

// Copyright (C) 2014, Oscar Acena <oscar.acena@uclm.es>
// This software is under the terms of GPLv3 or later.

#include <boost/thread/thread.hpp>
#include <sys/ioctl.h>
#include <iostream>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "gattlib.h"


IOService::IOService(bool run) {
    if (run)
	start();
}

void
IOService::start() {
    PyEval_InitThreads();
    boost::thread iothread(*this);
}

void
IOService::operator()() {
    GMainLoop *event_loop = g_main_loop_new(NULL, FALSE);

    g_main_loop_run(event_loop);
    g_main_loop_unref(event_loop);
}

static volatile IOService _instance(true);

GATTResponse::GATTResponse() :
_status(0) {
}

void
GATTResponse::on_response(const std::string data) {
    _data.append(data);
}

void
GATTResponse::notify(uint8_t status) {
    _status = status;
    _event.set();
}

bool
GATTResponse::wait(uint16_t timeout) {
    // std::cout << " - wait called" << std::endl;
    // std::cout.flush();

    if (not _event.wait(timeout))
	return false;

    // std::cout << " - wait exited as true" << std::endl;
    // std::cout.flush();

    if (_status != 0) {
	std::string msg = "Characteristic value/descriptor operation failed: ";
	msg += att_ecode2str(_status);
	throw std::runtime_error(msg);
    }

    return true;
}

boost::python::list
GATTResponse::received() {
    return _data;
}

GATTRequester::GATTRequester(std::string address, bool do_connect) :
_state(STATE_DISCONNECTED),
    _address(address),
    _channel(NULL),
    _attrib(NULL) {

    if (do_connect)
	connect();
}

GATTRequester::~GATTRequester() {
    if (_channel != NULL) {
	g_io_channel_shutdown(_channel, TRUE, NULL);
	g_io_channel_unref(_channel);
    }

    if (_attrib != NULL) {
	g_attrib_unref(_attrib);
    }
}

void
GATTRequester::on_notification(const uint16_t handle, const std::string data) {
    std::cout << "on notification, handle: 0x" << std::hex << handle << std::endl;
}

void
GATTRequester::on_indication(const uint16_t handle, const std::string data) {
    std::cout << "on indication, handle: 0x" << std::hex << handle << std::endl;
}

void
events_handler(const uint8_t* data, uint16_t size, gpointer userp) {
    GATTRequester* request = (GATTRequester*)userp;
    uint16_t handle = htobs(bt_get_le16(&data[1]));

    switch(data[0]) {
    case ATT_OP_HANDLE_NOTIFY:
	request->on_notification(handle, std::string((const char*)data, size));
	return;
    case ATT_OP_HANDLE_IND:
	request->on_indication(handle, std::string((const char*)data, size));
	break;
    default:
	throw std::runtime_error("Invalid event opcode!");
    }

    size_t plen;
    uint8_t* output = g_attrib_get_buffer(request->_attrib, &plen);
    uint16_t olen = enc_confirmation(output, plen);

    if (olen > 0)
     	g_attrib_send(request->_attrib, 0, output, olen, NULL, NULL, NULL);
}

void
connect_cb(GIOChannel* channel, GError* err, gpointer userp) {
    if (err) {
	std::string msg(err->message);
	g_error_free(err);
	throw std::runtime_error(msg);
    }

    GError *gerr = NULL;
    uint16_t mtu;
    uint16_t cid;
    bt_io_get(channel, &gerr,
	      BT_IO_OPT_IMTU, &mtu,
	      BT_IO_OPT_CID, &cid,
	      BT_IO_OPT_INVALID);

    // Can't detect MTU, using default
    if (gerr) {
	g_error_free(gerr);
	mtu = ATT_DEFAULT_LE_MTU;
    }

    if (cid == ATT_CID)
	mtu = ATT_DEFAULT_LE_MTU;

    GATTRequester* request = (GATTRequester*)userp;
    request->_attrib = g_attrib_new(channel, mtu);

    g_attrib_register(request->_attrib, ATT_OP_HANDLE_NOTIFY, GATTRIB_ALL_HANDLES,
		      events_handler, userp, NULL);
    g_attrib_register(request->_attrib, ATT_OP_HANDLE_IND, GATTRIB_ALL_HANDLES,
		      events_handler, userp, NULL);

    request->_state = GATTRequester::STATE_CONNECTED;
}

void
GATTRequester::connect(bool wait) {
    if (_state != STATE_DISCONNECTED)
	throw std::runtime_error("Already connecting or connected");
    _state = STATE_CONNECTING;

    GError *gerr = NULL;
    _channel = gatt_connect
	("hci0",           // 'hciX'
	 _address.c_str(), // 'mac address'
	 "public",         // 'public' '[public | random]'
	 "low",            // 'low' '[low | medium | high]'
	 0,                // 0, int
	 0,                // 0, mtu
	 connect_cb,
	 &gerr,
	 (gpointer)this);

    if (_channel == NULL) {
	g_error_free(gerr);
	throw std::runtime_error(gerr->message);
    }

    if (wait)
	check_channel();
}

bool
GATTRequester::is_connected() {
    return _state == STATE_CONNECTED;
}

static void
read_by_handler_cb(guint8 status, const guint8* data, guint16 size, gpointer userp) {

    // Note: first byte is the payload size, remove it
    GATTResponse* response = (GATTResponse*)userp;
    response->on_response(std::string((const char*)data + 1, size - 1));
    response->notify(status);
}

void
GATTRequester::read_by_handle_async(uint16_t handle, GATTResponse* response) {
    check_channel();
    gatt_read_char(_attrib, handle, read_by_handler_cb, (gpointer)response);
}

boost::python::list
GATTRequester::read_by_handle(uint16_t handle) {
    GATTResponse response;
    read_by_handle_async(handle, &response);

    if (not response.wait(MAX_WAIT_FOR_PACKET))
	// FIXME: now, response is deleted, but is still registered on
	// GLIB as callback!!
	throw std::runtime_error("Device is not responding!");

    return response.received();
}

static void
read_by_uuid_cb(guint8 status, const guint8* data, guint16 size, gpointer userp) {

    struct att_data_list* list;
    list = dec_read_by_type_resp(data, size);
    if (list == NULL)
	return;

    GATTResponse* response = (GATTResponse*)userp;
    for (int i=0; i<list->num; i++) {
	uint8_t* item = list->data[i];

	// Remove handle addr
	item += 2;

	std::string value((const char*)item, list->len - 2);
	response->on_response(value);
    }

    att_data_list_free(list);
    response->notify(status);
}

void
GATTRequester::read_by_uuid_async(std::string uuid, GATTResponse* response) {
    uint16_t start = 0x0001;
    uint16_t end = 0xffff;
    bt_uuid_t btuuid;

    check_channel();
    if (bt_string_to_uuid(&btuuid, uuid.c_str()) < 0)
	throw std::runtime_error("Invalid UUID\n");

    gatt_read_char_by_uuid(_attrib, start, end, &btuuid, read_by_uuid_cb,
			   (gpointer)response);

}

boost::python::list
GATTRequester::read_by_uuid(std::string uuid) {
    GATTResponse response;
    read_by_uuid_async(uuid, &response);

    if (not response.wait(MAX_WAIT_FOR_PACKET))
	// FIXME: now, response is deleted, but is still registered on
	// GLIB as callback!!
	throw std::runtime_error("Device is not responding!");

    return response.received();
}

static void
write_by_handle_cb(guint8 status, const guint8* data, guint16 size, gpointer userp) {
    GATTResponse* response = (GATTResponse*)userp;
    response->on_response(std::string((const char*)data, size));
    response->notify(status);
}

void
GATTRequester::write_by_handle_async(uint16_t handle, std::string data,
				     GATTResponse* response) {
    check_channel();
    gatt_write_char(_attrib, handle, (const uint8_t*)data.data(), data.size(),
		    write_by_handle_cb, (gpointer)response);
}

boost::python::list
GATTRequester::write_by_handle(uint16_t handle, std::string data) {
    // std::cout << " - write by handle" << std::endl;

    GATTResponse response;
    write_by_handle_async(handle, data, &response);

    // std::cout << " - write done" << std::endl;
    if (not response.wait(MAX_WAIT_FOR_PACKET))
	// FIXME: now, response is deleted, but is still registered on
	// GLIB as callback!!
	throw std::runtime_error("Device is not responding!");

    // std::cout << " - response received" << std::endl;
    // std::cout.flush();
    return response.received();
}

void
GATTRequester::check_channel() {
    time_t ts = time(NULL);
    while (_channel == NULL || _attrib == NULL) {
	usleep(1000);
	if (time(NULL) - ts > MAX_WAIT_FOR_PACKET)
	    throw std::runtime_error("Channel or attrib not ready");

	// // Set Link Supervision Timeout
	// {
	// 	int dev_id = 0; // hci0
	// 	int timeout = 700;
	// 	struct hci_conn_info_req cr;
	// 	bdaddr_t bdaddr;
	// 	int dd;

	// 	str2ba(_address.c_str(), &bdaddr);
	// 	dd = hci_open_dev(dev_id);
	// 	if (dd < 0)
	// 		throw std::runtime_error("HCI device open failed");

	// 	bacpy(&(cr.bdaddr), &bdaddr);
	// 	cr.type = ACL_LINK;

	// 	if (ioctl(dd, HCIGETCONNINFO, (unsigned long)&cr) < 0) {
	// 		perror("Get connection info failed");
	// 		throw std::runtime_error("Get connection info failed: ");
	// 	}

	// 	if (hci_write_link_supervision_timeout(dd, htobs(cr.conn_info->handle),
	// 										   htobs(timeout), 1000) < 0)
	// 		throw std::runtime_error("HCI write_link_supervision_timeout request failed");

	// 	hci_close_dev(dd);
	// }
    }
}
