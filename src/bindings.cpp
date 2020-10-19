// -*- mode: c++; coding: utf-8 -*-

// Copyright (C) 2014,2020 Oscar Acena <oscaracena@gmail.com>
// This software is under the terms of Apache License v2 or later.

#include <boost/python.hpp>
#include <boost/python/suite/indexing/map_indexing_suite.hpp>
#include <boost/python/overloads.hpp>
#include <boost/python/raw_function.hpp>
#include <boost/python/to_python_converter.hpp>

#include "gattlib.h"
#include "gattservices.h"
#include "beacon.h"

using namespace boost::python;

boost::python::object pyGATTResponse;
boost::python::object pyBaseException;
PyObject* pyBaseExceptionPtr = NULL;
boost::python::object pyBTIOException;
PyObject* pyBTIOExceptionPtr = NULL;
boost::python::object pyGATTException;
PyObject* pyGATTExceptionPtr = NULL;

/** to-python convert for std::vector<char> */
struct bytes_vector_to_python_bytes
{
    static PyObject* convert(std::vector<char> const& s)
    {
        return PyBytes_FromStringAndSize(s.data(), s.size());
    }
};

class GATTResponseCb : public GATTResponse {
public:
    GATTResponseCb(PyObject* p) : GATTResponse(p) {
    }

    // to be called from c++ side
    void on_response(boost::python::object data) {
        try {
            call_method<void>(self, "on_response", data);
        } catch(error_already_set const&) {
            PyErr_Print();
        }
    }

    // to be called from python side
    static void default_on_response(GATTResponse& self_, boost::python::object data) {
        self_.GATTResponse::on_response(data);
    }

    // to be called from c++ side
    void on_response_complete() {
        try {
            call_method<void>(self, "on_response_complete");
        } catch(error_already_set const&) {
            PyErr_Print();
        }
    }

    // to be called from python side
    static void default_on_response_complete(GATTResponse& self_) {
        self_.GATTResponse::on_response_complete();
    }

    // to be called from c++ side
    void on_response_failed(int status) {
        try {
            call_method<void>(self, "on_response_failed", status);
        } catch(error_already_set const&) {
            PyErr_Print();
        }
    }

    // to be called from python side
    static void default_on_response_failed(GATTResponse& self_, int status) {
        self_.GATTResponse::on_response_failed(status);
    }
};

class GATTRequesterCb : public GATTRequester {
public:
    GATTRequesterCb(PyObject* p, std::string address,
            bool do_connect=true, std::string device="hci0") :
        GATTRequester(p, address, do_connect, device) {
    }

    // to be called from c++ side
    void on_connect(int mtu) {
        try {
          call_method<void>(self, "on_connect", mtu);
        } catch(error_already_set const&) {
            PyErr_Print();
        }
    }

    // to be called from python side
    static void default_on_connect(GATTRequester& self_, int mtu)
    {
        self_.GATTRequester::on_connect(mtu);
    }

    // to be called from c++ side
    void on_connect_failed(int code) {
        try {
          call_method<void>(self, "on_connect_failed", code);
        } catch(error_already_set const&) {
            PyErr_Print();
        }
    }

    // to be called from python side
    static void default_on_connect_failed(GATTRequester& self_, int code)
    {
        self_.GATTRequester::on_connect_failed(code);
    }

    // to be called from c++ side
    void on_disconnect() {
        try {
            call_method<void>(self, "on_disconnect");
        } catch(error_already_set const&) {
            PyErr_Print();
        }
    }

    // to be called from python side
    static void default_on_disconnect(GATTRequester& self_)
    {
        self_.GATTRequester::on_disconnect();
    }

    // to be called from c++ side
    void on_notification(const uint16_t handle, const std::string data) {
        try {
            const std::vector<char> vecdata(data.begin(), data.end());
            call_method<void>(self, "on_notification", handle, vecdata);
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
            const std::vector<char> vecdata(data.begin(), data.end());
            call_method<void>(self, "on_indication", handle, vecdata);
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
};

BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(
        start_advertising, BeaconService::start_advertising, 0, 5)

BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(
        GATTRequester_discover_characteristics_overloads,
        GATTRequester::discover_characteristics, 0, 3)

BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(
        GATTRequester_discover_characteristics_async_overloads,
        GATTRequester::discover_characteristics_async, 1, 4)

BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(
        GATTRequester_discover_descriptors_overloads,
        GATTRequester::discover_descriptors, 0, 3)

BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(
        GATTRequester_discover_descriptors_async_overloads,
        GATTRequester::discover_descriptors_async, 1, 4)

BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(
        GATTResponse_wait_overloads,
        GATTResponse::wait_locked, 0, 1)

static PyObject*
createExceptionClass(const char* name, PyObject* baseType, boost::python::object &pyrv, const char *docstr=NULL)
{
    boost::python::object scope = boost::python::scope();
    std::string scopename = boost::python::extract<std::string>(scope.attr("__name__"));
    std::string qname = scopename + "." + name;
    PyObject* rv = PyErr_NewExceptionWithDoc(qname.c_str(), docstr, baseType, 0);
    boost::python::object o(boost::python::handle<>(boost::python::borrowed(rv)));
    pyrv = o;
    scope.attr(name) = pyrv;
    return rv;
}

void
translate_BTIOException(const BTIOException& e)
{
    boost::python::object pye(pyBTIOException(e.what()));
    pye.attr("code") = e.status;
    PyErr_SetObject(pyBTIOExceptionPtr, pye.ptr());
}

void
translate_GATTException(const GATTException& e)
{
    boost::python::object pye(pyGATTException(e.what()));
    pye.attr("status") = e.status;
    PyErr_SetObject(pyGATTExceptionPtr, pye.ptr());
}

#define BOOST_PYTHON_CONSTANT(x) boost::python::scope().attr(#x) = x

BOOST_PYTHON_MODULE(gattlib) {

    BOOST_PYTHON_CONSTANT(ATT_ECODE_INVALID_HANDLE);
    BOOST_PYTHON_CONSTANT(ATT_ECODE_READ_NOT_PERM);
    BOOST_PYTHON_CONSTANT(ATT_ECODE_WRITE_NOT_PERM);
    BOOST_PYTHON_CONSTANT(ATT_ECODE_INVALID_PDU);
    BOOST_PYTHON_CONSTANT(ATT_ECODE_AUTHENTICATION);
    BOOST_PYTHON_CONSTANT(ATT_ECODE_REQ_NOT_SUPP);
    BOOST_PYTHON_CONSTANT(ATT_ECODE_INVALID_OFFSET);
    BOOST_PYTHON_CONSTANT(ATT_ECODE_AUTHORIZATION);
    BOOST_PYTHON_CONSTANT(ATT_ECODE_PREP_QUEUE_FULL);
    BOOST_PYTHON_CONSTANT(ATT_ECODE_ATTR_NOT_FOUND);
    BOOST_PYTHON_CONSTANT(ATT_ECODE_ATTR_NOT_LONG);
    BOOST_PYTHON_CONSTANT(ATT_ECODE_INSUFF_ENCR_KEY_SIZE);
    BOOST_PYTHON_CONSTANT(ATT_ECODE_INVAL_ATTR_VALUE_LEN);
    BOOST_PYTHON_CONSTANT(ATT_ECODE_UNLIKELY);
    BOOST_PYTHON_CONSTANT(ATT_ECODE_INSUFF_ENC);
    BOOST_PYTHON_CONSTANT(ATT_ECODE_UNSUPP_GRP_TYPE);
    BOOST_PYTHON_CONSTANT(ATT_ECODE_INSUFF_RESOURCES);
    BOOST_PYTHON_CONSTANT(ATT_ECODE_IO);
    BOOST_PYTHON_CONSTANT(ATT_ECODE_TIMEOUT);
    BOOST_PYTHON_CONSTANT(ATT_ECODE_ABORTED);

    BOOST_PYTHON_CONSTANT(GATT_CHR_PROP_BROADCAST);
    BOOST_PYTHON_CONSTANT(GATT_CHR_PROP_READ);
    BOOST_PYTHON_CONSTANT(GATT_CHR_PROP_WRITE_WITHOUT_RESP);
    BOOST_PYTHON_CONSTANT(GATT_CHR_PROP_WRITE);
    BOOST_PYTHON_CONSTANT(GATT_CHR_PROP_NOTIFY);
    BOOST_PYTHON_CONSTANT(GATT_CHR_PROP_INDICATE);
    BOOST_PYTHON_CONSTANT(GATT_CHR_PROP_AUTH);
    BOOST_PYTHON_CONSTANT(GATT_CHR_PROP_EXT_PROP);

    BOOST_PYTHON_CONSTANT(GATT_CLIENT_CHARAC_CFG_NOTIF_BIT);
    BOOST_PYTHON_CONSTANT(GATT_CLIENT_CHARAC_CFG_IND_BIT);

    to_python_converter<std::vector<char>, bytes_vector_to_python_bytes>();

    pyBaseExceptionPtr = createExceptionClass("BTBaseException", PyExc_RuntimeError, pyBaseException,
        "Base for custom gattlib exceptions.");

    pyBTIOExceptionPtr = createExceptionClass("BTIOException", pyBaseExceptionPtr, pyBTIOException,
        "Parameter, state and BT protocol level errors. code is an errno value.");
    register_exception_translator<BTIOException>(&translate_BTIOException);

    pyGATTExceptionPtr = createExceptionClass("GATTException", pyBaseExceptionPtr, pyGATTException,
        "GATT level errors. status is an ATT_ECODE_* value.");
    register_exception_translator<GATTException>(&translate_GATTException);

    register_ptr_to_python<GATTRequester*>();

    class_<GATTRequester, boost::noncopyable, GATTRequesterCb> ("GATTRequester",
            init<std::string, optional<bool, std::string> >())

        .def("connect", boost::python::raw_function(GATTRequester::connect_kwarg,1))
        .def("is_connected", &GATTRequester::is_connected)
        .def("on_connect", &GATTRequesterCb::default_on_connect)
        .def("on_connect_failed", &GATTRequesterCb::default_on_connect_failed)
        .def("disconnect", &GATTRequester::disconnect)
        .def("on_disconnect", &GATTRequesterCb::default_on_disconnect)
        .def("update_connection_parameters", boost::python::raw_function(GATTRequester::update_connection_parameters_kwarg,1))
        .def("exchange_mtu", &GATTRequester::exchange_mtu)
        .def("exchange_mtu_async", &GATTRequester::exchange_mtu_async)
        .def("set_mtu", &GATTRequester::set_mtu)
        .def("read_by_handle", &GATTRequester::read_by_handle)
        .def("read_by_handle_async", &GATTRequester::read_by_handle_async)
        .def("read_by_uuid", &GATTRequester::read_by_uuid)
        .def("read_by_uuid_async", &GATTRequester::read_by_uuid_async)
        .def("write_by_handle", &GATTRequester::write_by_handle)
        .def("write_by_handle_async", &GATTRequester::write_by_handle_async)
        .def("write_cmd", &GATTRequester::write_cmd)
        .def("enable_notifications", &GATTRequester::enable_notifications)
        .def("enable_notifications_async", &GATTRequester::enable_notifications_async)
        .def("on_notification", &GATTRequesterCb::default_on_notification)
        .def("on_indication", &GATTRequesterCb::default_on_indication)
        .def("discover_primary", &GATTRequester::discover_primary,
                "returns a list with of primary services,"
                " with their handles and UUIDs.")
        .def("discover_primary_async", &GATTRequester::discover_primary_async)
        .def("find_included", &GATTRequester::find_included)
        .def("find_included_async", &GATTRequester::find_included_async)
        .def("discover_characteristics",
                &GATTRequester::discover_characteristics,
                GATTRequester_discover_characteristics_overloads())
        .def("discover_characteristics_async",
                &GATTRequester::discover_characteristics_async,
                GATTRequester_discover_characteristics_async_overloads())
        .def("discover_descriptors",
                &GATTRequester::discover_descriptors,
                GATTRequester_discover_descriptors_overloads())
        .def("discover_descriptors_async",
                &GATTRequester::discover_descriptors_async,
                GATTRequester_discover_descriptors_async_overloads());

    register_ptr_to_python<GATTResponse*>();

    pyGATTResponse = class_<GATTResponse, boost::noncopyable, GATTResponseCb>("GATTResponse")
            .def("complete", &GATTResponse::complete)
            .def("status", &GATTResponse::status)
            .def("wait", &GATTResponse::wait_locked, GATTResponse_wait_overloads())
            .def("received", &GATTResponse::received)
            .def("on_response", &GATTResponseCb::default_on_response)
            .def("on_response_complete", &GATTResponseCb::default_on_response_complete)
            .def("on_response_failed", &GATTResponseCb::default_on_response_failed);

    class_<DiscoveryService>("DiscoveryService", init<optional<std::string> >())
            .def("discover", &DiscoveryService::discover);

    class_<BeaconService>("BeaconService", init<optional<std::string> >())
            .def("scan", &BeaconService::scan)
            .def("start_advertising", &BeaconService::start_advertising,
                    start_advertising(
                        args("uuid", "major", "minor", "txpower", "interval"),
                        "starts advertising beacon packets"))
            .def("stop_advertising", &BeaconService::stop_advertising);
}
