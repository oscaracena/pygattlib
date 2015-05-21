// -*- mode: c++; coding: utf-8 -*-

// Copyright (C) 2014, Oscar Acena <oscaracena@gmail.com>
// This software is under the terms of GPLv3 or later.
#define BOOST_PYTHON_STATIC_LIB 

#include <boost/thread/thread.hpp>
#include <iostream>

#include "gattlib.h"

class PyThreadsGuard {
public:
    PyThreadsGuard() : _save(NULL) {
        _save = PyEval_SaveThread();
    };

    ~PyThreadsGuard() {
        PyEval_RestoreThread(_save);
    };

private:
    PyThreadState* _save;
};


IOService::IOService(bool run) {
    if (run)
        start();
}

void
IOService::start() {
    if (!PyEval_ThreadsInitialized()) {
        PyEval_InitThreads();
    }

    boost::thread iothread(*this);
}

void
IOService::operator()() {
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
    if (!_event.wait(timeout))
        return false;

    if (_status != 0) {
        std::string msg = "Characteristic value/descriptor operation failed: ";
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
    _device("hci0"),
    _address(address),
    _hci_socket(-1)
{
    if (_hci_socket < 0) {
        std::string msg = std::string("Could not open HCI device: ") +
            std::string(strerror(errno));
        throw std::runtime_error(msg);
    }

    if (do_connect)
        connect();
}

GATTRequester::~GATTRequester() {
}

void
GATTRequester::on_notification(const uint16_t handle, const std::string data) {
    std::cout << "on notification, handle: 0x" << std::hex << handle << " -> ";

    for (std::string::const_iterator i=data.begin() + 2; i!=data.end(); i++) {
        printf("%02x:", int(*i));
    }
    printf("\n");
}

void
GATTRequester::on_indication(const uint16_t handle, const std::string data) {
    std::cout << "on indication, handle: 0x" << std::hex << handle << " -> ";

    for (std::string::const_iterator i=data.begin() + 2; i!=data.end(); i++) {
        printf("%02x:", int(*i));
    }
    printf("\n");
}

void
GATTRequester::connect(bool wait) {
    if (_state != STATE_DISCONNECTED)
        throw std::runtime_error("Already connecting or connected");

    _state = STATE_CONNECTING;
}

bool
GATTRequester::is_connected() {
    return _state == STATE_CONNECTED;
}

void
GATTRequester::disconnect() {
    if (_state == STATE_DISCONNECTED)
        return;

    _state = STATE_DISCONNECTED;
}

void
GATTRequester::read_by_handle_async(uint16_t handle, GATTResponse* response) {
    check_channel();
}

boost::python::list
GATTRequester::read_by_handle(uint16_t handle) {
    GATTResponse response;
    read_by_handle_async(handle, &response);

    if (!response.wait(MAX_WAIT_FOR_PACKET))
        // FIXME: now, response is deleted, but is still registered on
        // GLIB as callback!!
        throw std::runtime_error("Device is not responding!");

    return response.received();
}

void
GATTRequester::read_by_uuid_async(std::string uuid, GATTResponse* response) {
}

boost::python::list
GATTRequester::read_by_uuid(std::string uuid) {
    PyThreadsGuard guard;
    GATTResponse response;

    read_by_uuid_async(uuid, &response);

    if (!response.wait(MAX_WAIT_FOR_PACKET))
        // FIXME: now, response is deleted, but is still registered on
        // GLIB as callback!!
        throw std::runtime_error("Device is not responding!");

    return response.received();
}

void
GATTRequester::write_by_handle_async(uint16_t handle, std::string data,
                                     GATTResponse* response) {
    check_channel();
}

boost::python::list
GATTRequester::write_by_handle(uint16_t handle, std::string data) {
    PyThreadsGuard guard;
    GATTResponse response;

    write_by_handle_async(handle, data, &response);

    if (!response.wait(MAX_WAIT_FOR_PACKET))
        // FIXME: now, response is deleted, but is still registered on
        // GLIB as callback!!
        throw std::runtime_error("Device is not responding!");

    return response.received();
}

void
GATTRequester::check_channel() {
}
