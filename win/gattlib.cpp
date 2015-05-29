// -*- mode: c++; coding: utf-8 -*-

// Copyright (C) 2014, Oscar Acena <oscaracena@gmail.com>
// This software is under the terms of GPLv3 or later.
#define BOOST_PYTHON_STATIC_LIB 

#include <boost/thread/thread.hpp>
#include <iostream>

#include "gattlib.h"
#include <winerror.h>
//#include <bluetoothleapis.h>

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

GATTResponse::GATTResponse() {
    throw std::runtime_error("Not implemented!");
}

void
GATTResponse::on_response(const std::string data) {
    throw std::runtime_error("Not implemented!");
}


boost::python::list
GATTResponse::received() {
    throw std::runtime_error("Not implemented!");
    return boost::python::list();
}

GATTRequester::GATTRequester(std::string address, bool do_connect) :
    _state(STATE_DISCONNECTED),
    _device("hci0"),
    _address(address),
    _hci_socket(-1)
{
    throw std::runtime_error("Not implemented!");
}

GATTRequester::~GATTRequester() {
}

void
GATTRequester::on_notification(const uint16_t handle, const std::string data) {
    throw std::runtime_error("Not implemented!");
}

void
GATTRequester::on_indication(const uint16_t handle, const std::string data) {
    throw std::runtime_error("Not implemented!");
}

void
GATTRequester::connect(bool wait) {
    throw std::runtime_error("Not implemented!");
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
    throw std::runtime_error("Not implemented!");
    if (_state == STATE_DISCONNECTED)
        return;

    _state = STATE_DISCONNECTED;
}

void
GATTRequester::read_by_handle_async(uint16_t handle, GATTResponse* response) {
    throw std::runtime_error("Not implemented!");
}

boost::python::list
GATTRequester::read_by_handle(uint16_t handle) {
    // Determine Characteristic Buffer Size
    /*USHORT charBufferSize;
    HRESULT hr = BluetoothGATTGetCharacteristics(
            _device_handle,
            NULL,
            0,
            NULL,
            &charBufferSize,
            BLUETOOTH_GATT_FLAG_NONE);
    
    if (HRESULT_FROM_WIN32(ERROR_MORE_DATA) != hr) {
        throw std::runtime_error("Problem determining characteristics buffer size");
    }
    
    if (charBufferSize > 0) {
        pCharBuffer = (PBTH_LE_GATT_CHARACTERISTIC)
                malloc(charBufferSize * sizeof(BTH_LE_GATT_CHARACTERISTIC));
    
        if (NULL == pCharBuffer) {
            throw std::bad_alloc("Problem determining characteristics buffer size");
        } else {
            RtlZeroMemory(pCharBuffer, 
                    charBufferSize * sizeof(BTH_LE_GATT_CHARACTERISTIC));
        }

        // Retrieve Characteristics
        hr = BluetoothGATTGetCharacteristics(
                hCurrService,
                currGattService,
                charBufferSize,
                pCharBuffer,
                &numChars,
                BLUETOOTH_GATT_FLAG_NONE);

        if (S_OK != hr) {
            throw std::bad_alloc("Problem getting characteristics data");
        }

        if (numChars != charBufferSize) {
            throw std::bad_alloc("Characteristics buffer size mismatch");
        }
    }*/
    GATTResponse response;
    return response.received();
}

void
GATTRequester::read_by_uuid_async(std::string uuid, GATTResponse* response) {
    throw std::runtime_error("Not implemented!");
}

boost::python::list
GATTRequester::read_by_uuid(std::string uuid) {
    throw std::runtime_error("Not implemented!");
    PyThreadsGuard guard;
    GATTResponse response;
    return response.received();
}

void
GATTRequester::write_by_handle_async(uint16_t handle, std::string data,
                                     GATTResponse* response) {
    throw std::runtime_error("Not implemented!");
}

boost::python::list
GATTRequester::write_by_handle(uint16_t handle, std::string data) {
    throw std::runtime_error("Not implemented!");
    PyThreadsGuard guard;
    GATTResponse response;
    return response.received();
}

