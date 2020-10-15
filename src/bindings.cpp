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
    void on_response(const std::string data) {
        try {
            call_method<void>(self, "on_response", data);
        } catch(error_already_set const&) {
            PyErr_Print();
        }
    }

    // to be called from python side
    static void default_on_response(GATTResponse& self_, const std::string data) {
        self_.GATTResponse::on_response(data);
    }
};

class GATTRequesterCb : public GATTRequester {
public:
    GATTRequesterCb(PyObject* p, std::string address,
            bool do_connect=true, std::string device="hci0") :
        GATTRequester(p, address, do_connect, device) {
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

    register_ptr_to_python<GATTRequester*>();

    class_<GATTRequester, boost::noncopyable, GATTRequesterCb> ("GATTRequester",
            init<std::string, optional<bool, std::string> >())

        .def("connect", boost::python::raw_function(GATTRequester::connect_kwarg,1))
        .def("is_connected", &GATTRequester::is_connected)
        .def("disconnect", &GATTRequester::disconnect)
        .def("read_by_handle", &GATTRequester::read_by_handle)
        .def("read_by_handle_async", &GATTRequester::read_by_handle_async)
        .def("read_by_uuid", &GATTRequester::read_by_uuid)
        .def("read_by_uuid_async", &GATTRequester::read_by_uuid_async)
        .def("write_by_handle", &GATTRequester::write_by_handle)
        .def("write_by_handle_async", &GATTRequester::write_by_handle_async)
        .def("write_cmd", &GATTRequester::write_cmd)
        .def("on_notification", &GATTRequesterCb::default_on_notification)
        .def("on_indication", &GATTRequesterCb::default_on_indication)
        .def("discover_primary", &GATTRequester::discover_primary,
                "returns a list with of primary services,"
                " with their handles and UUIDs.")
        .def("discover_primary_async", &GATTRequester::discover_primary_async)
        .def("discover_characteristics",
                &GATTRequester::discover_characteristics,
                GATTRequester_discover_characteristics_overloads())
        .def("discover_characteristics_async",
                &GATTRequester::discover_characteristics_async,
                GATTRequester_discover_characteristics_async_overloads());

    register_ptr_to_python<GATTResponse*>();

    pyGATTResponse = class_<GATTResponse, boost::noncopyable, GATTResponseCb>("GATTResponse")
            .def("received", &GATTResponse::received)
            .def("on_response", &GATTResponseCb::default_on_response);

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
