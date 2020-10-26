// -*- mode: c++; coding: utf-8; tab-width: 4 -*-

// Copyright (C) 2014, Oscar Acena <oscaracena@gmail.com>
// This software is under the terms of Apache License v2 or later.

#ifndef _MIBANDA_GATTLIB_H_
#define _MIBANDA_GATTLIB_H_

#define MAX_WAIT_FOR_PACKET 15 // seconds

#include <boost/python/list.hpp>
#include <boost/python/tuple.hpp>
#include <boost/python/dict.hpp>
#include <string>
#include <stdint.h>
#include <glib.h>

extern "C" {
#include "lib/uuid.h"
#include "attrib/att.h"
#include "attrib/gattrib.h"
#include "attrib/gatt.h"
#include "attrib/utils.h"
}

#include "event.hpp"

class IOService {
public:
	IOService(bool run);
	void start();
	void stop();
	void operator()();

private:
	GMainContext* context;
	GMainLoop* event_loop;
	Event start_event;
};

class BTIOException : public std::runtime_error
{
public:
    BTIOException(int status_, const std::string &msg)
        : std::runtime_error(msg), status(status_)
    {
    }
    BTIOException(int status_, const char *msg)
        : std::runtime_error(msg), status(status_)
    {
    }

    int status;
};

class GATTException : public std::runtime_error
{
public:
    GATTException(int status_, const std::string &msg)
        : std::runtime_error(msg), status(status_)
    {
    }
    GATTException(int status_, const char *msg)
        : std::runtime_error(msg), status(status_)
    {
    }

    int status;
};

class GATTPyBase {
public:
    GATTPyBase(PyObject* p) : self(p) { }
    void incref() { Py_INCREF(self); }
    void decref() { Py_DECREF(self); }
protected:
    PyObject* self;
};

class GATTResponse : public GATTPyBase {
public:
	GATTResponse(PyObject* p);
	virtual ~GATTResponse() {};

	virtual void on_response(boost::python::object data);
	virtual void on_response_complete() { }
	virtual void on_response_failed(int status) { }
	bool complete();
	int status();
	boost::python::object received();
	bool wait(uint16_t timeout);
	bool wait_locked(uint16_t timeout=5 * MAX_WAIT_FOR_PACKET);
	void expect_list();
	void notify(uint8_t status);

private:
	bool _complete;
	uint8_t _status;
	boost::python::object _data;
	bool _list;
	Event _event;
};

extern boost::python::object pyGATTResponse;

void connect_cb(GIOChannel* channel, GError* err, gpointer user_data);

class PyKwargsExtracter;

class GATTRequester : public GATTPyBase {
public:
    GATTRequester(PyObject* p, std::string address,
			bool do_connect=true, std::string device="hci0");
	virtual ~GATTRequester();

	virtual void on_connect(int mtu) { }
	virtual void on_connect_failed(int code) { }
	virtual void on_disconnect() { }
	virtual void on_notification(const uint16_t handle, const std::string data);
	virtual void on_indication(const uint16_t handle, const std::string data);

	void connect(bool wait=false, std::string channel_type="public",
			std::string security_level="low", int psm=0, int mtu=0);
	static boost::python::object connect_kwarg(boost::python::tuple args, boost::python::dict kwargs);
	bool is_connected();
	void disconnect();
	static boost::python::object update_connection_parameters_kwarg(boost::python::tuple args, boost::python::dict kwargs);
	void extract_connection_parameters(PyKwargsExtracter &e);
	void update_connection_parameters();
	void exchange_mtu_async(uint16_t mtu, GATTResponse* response);
	boost::python::object exchange_mtu(uint16_t mtu);
	void set_mtu(uint16_t mtu);
	void read_by_handle_async(uint16_t handle, GATTResponse* response);
	boost::python::object read_by_handle(uint16_t handle);
	void read_by_uuid_async(std::string uuid, GATTResponse* response);
	boost::python::object read_by_uuid(std::string uuid);
	void write_by_handle_async(uint16_t handle, std::string data, GATTResponse* response);
    boost::python::object write_by_handle(uint16_t handle, std::string data);
	void write_cmd(uint16_t handle, std::string data);
	void enable_notifications_async(uint16_t handle, bool notifications, bool indications, GATTResponse* response);
	void enable_notifications(uint16_t handle, bool notifications, bool indications);

	friend void connect_cb(GIOChannel*, GError*, gpointer);
	friend gboolean disconnect_cb(GIOChannel* channel, GIOCondition cond, gpointer userp);
	friend void events_handler(const uint8_t* data, uint16_t size, gpointer userp);

	boost::python::object discover_primary();
	void discover_primary_async(GATTResponse* response);
	boost::python::object find_included(int start = 0x0001, int end = 0xffff);
	void find_included_async(GATTResponse* response, int start = 0x0001, int end = 0xffff);
	boost::python::object discover_characteristics(int start = 0x0001, int end = 0xffff, std::string uuid = "");
	void discover_characteristics_async(GATTResponse* response, int start = 0x0001, int end = 0xffff, std::string uuid = "");
	boost::python::object discover_descriptors(int start = 0x0001, int end = 0xffff, std::string uuid = "");
	void discover_descriptors_async(GATTResponse* response, int start = 0x0001, int end = 0xffff, std::string uuid = "");

	GIOChannel* get_channel() { return _channel; }

private:
	void check_channel();
	void check_connected();

    enum State {
        STATE_DISCONNECTED,
        STATE_CONNECTING,
        STATE_CONNECTED,
        STATE_ERROR_CONNECTING
    } _state;

	std::string _device;
	std::string _address;
	uint16_t _conn_interval_min; // 24, 40, 0 700 25000
	uint16_t _conn_interval_max;
	uint16_t _slave_latency;
	uint16_t _supervision_timeout;
	int _hci_socket;
	GIOChannel* _channel;
	GAttrib* _attrib;
	class AttribLocker : public _GAttribLock
	{
	public:
		AttribLocker()
		{
			lockfn = &slock;
			unlockfn = &sunlock;
		}
		static void slock(struct _GAttribLock *lk)
		{
			((AttribLocker*)lk)->_mutex.lock();
		}
		static void sunlock(struct _GAttribLock *lk)
		{
			((AttribLocker*)lk)->_mutex.unlock();
		}
		boost::mutex _mutex;
	} attriblocker;
	Event _ready;
};

#endif // _MIBANDA_GATTLIB_H_
