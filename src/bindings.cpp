// -*- mode: c++; coding: utf-8; tab-width: 4 -*-

// Copyright (C) 2014, Oscar Acena <oscaracena@gmail.com>
// This software is under the terms of GPLv3 or later.

#include <boost/python.hpp>
#include <boost/python/suite/indexing/map_indexing_suite.hpp>

#include "gattlib.h"
#include "gattservices.h"

using namespace boost::python;

class GATTResponseCb : public GATTResponse {
public:
	GATTResponseCb(PyObject* p) : self(p) {
	}

	// to be called from c++ side
	void on_response(const std::string data) {
		try {
			PyGILState_STATE state = PyGILState_Ensure();
			call_method<void>(self, "on_response", data);
			PyGILState_Release(state);
		} catch(error_already_set const&) {
			PyErr_Print();
		}
	}

	// to be called from python side
	static void default_on_response(GATTResponse& self_, const std::string data) {
		self_.GATTResponse::on_response(data);
	}

private:
	PyObject* self;
};

class GATTRequesterCb : public GATTRequester {
public:
	GATTRequesterCb(PyObject* p, std::string address, bool do_connect=true) :
		GATTRequester(address, do_connect),
		self(p) {
	}

	// to be called from c++ side
	void on_notification(const uint16_t handle, const std::string data) {
		try {
			PyGILState_STATE state = PyGILState_Ensure();
			call_method<void>(self, "on_notification", handle, data);
			PyGILState_Release(state);
		} catch(error_already_set const&) {
			PyErr_Print();
		}
	}

	// to be called from python side
	static void default_on_notification(GATTRequester& self_,
										const uint16_t handle,
										const std::string data) {
		self_.GATTRequester::on_notification(handle, data);
	}

	// to be called from c++ side
	void on_indication(const uint16_t handle, const std::string data) {
		try {
			PyGILState_STATE state = PyGILState_Ensure();
			call_method<void>(self, "on_indication", handle, data);
			PyGILState_Release(state);
		} catch(error_already_set const&) {
			PyErr_Print();
		}
	}

	// to be called from python side
	static void default_on_indication(GATTRequester& self_,
										const uint16_t handle,
										const std::string data) {
		self_.GATTRequester::on_indication(handle, data);
	}

private:
	PyObject* self;
};

BOOST_PYTHON_MODULE(gattlib) {

	register_ptr_to_python<GATTRequester*>();

	class_<GATTRequester, boost::noncopyable, GATTRequesterCb>
		("GATTRequester", init<std::string, optional<bool> >())

		.def("connect", &GATTRequester::connect)
		.def("is_connected", &GATTRequester::is_connected)
		.def("disconnect", &GATTRequester::disconnect)
		.def("read_by_handle", &GATTRequester::read_by_handle)
		.def("read_by_handle_async", &GATTRequester::read_by_handle_async)
		.def("read_by_uuid", &GATTRequester::read_by_uuid)
		.def("read_by_uuid_async", &GATTRequester::read_by_uuid_async)
		.def("write_by_handle", &GATTRequester::write_by_handle)
		.def("write_by_handle_async", &GATTRequester::write_by_handle_async)
		.def("on_notification", &GATTRequesterCb::default_on_notification)
		.def("on_indication", &GATTRequesterCb::default_on_indication)
		;

	register_ptr_to_python<GATTResponse*>();

	class_<GATTResponse, boost::noncopyable, GATTResponseCb>("GATTResponse")
		.def("received", &GATTResponse::received)
		.def("on_response", &GATTResponseCb::default_on_response);
		;

	class_<DiscoveryService>("DiscoveryService", init<std::string>())
		.def("discover", &DiscoveryService::discover)
		;
}
