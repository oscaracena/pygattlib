// -*- mode: c++; coding: utf-8 -*-

// Copyright (C) 2014,2020 Oscar Acena <oscaracena@gmail.com>
// This software is under the terms of Apache License v2 or later.

#include <boost/thread/thread.hpp>
#include <boost/python/dict.hpp>
#include <boost/python/extract.hpp>
#include <boost/python/str.hpp>
#include <sys/ioctl.h>
#include <iostream>

#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "gattlib.h"


// This class uses RAII to ensure that the GIL is released while the object exists
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

class PyGILGuard {
public:
    PyGILGuard() { _state = PyGILState_Ensure(); }
    ~PyGILGuard() { PyGILState_Release(_state); }

private:
    PyGILState_STATE _state;
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
IOService::stop() {
    g_main_loop_quit(event_loop);
}

void
IOService::operator()() {
    event_loop = g_main_loop_new(NULL, FALSE);

    g_main_loop_run(event_loop);
    g_main_loop_unref(event_loop);
}

static volatile IOService _instance(true);

GATTResponse::GATTResponse(PyObject* p) :
    GATTPyBase(p),
    _status(0) {
}

void
GATTResponse::on_response(boost::python::object data) {
    _data.append(data);
}

void
GATTResponse::notify(uint8_t status) {
    _status = status;
    _event.set();
}

bool
GATTResponse::wait(uint16_t timeout) {
    if (not _event.wait(timeout)) {
        // timeout expired!
        return false;
    }

    if (_status != 0) {
        std::string msg = "Characteristic value/descriptor operation failed: ";
        msg += att_ecode2str(_status);
        throw GATTException(_status, msg);
    }

    return true;
}

boost::python::list
GATTResponse::received() {
    return _data;
}


GATTRequester::GATTRequester(PyObject* p, std::string address, bool do_connect,
        std::string device) :
    GATTPyBase(p),
    _state(STATE_DISCONNECTED),
    _device(device),
    _address(address),
    _hci_socket(-1),
    _channel(NULL),
    _attrib(NULL) {

    int dev_id = hci_devid(_device.c_str());
    if (dev_id < 0)
        throw BTIOException(EINVAL, "Invalid device!");

    _hci_socket = hci_open_dev(dev_id);
    if (_hci_socket < 0) {
        std::string msg = std::string("Could not open HCI device: ") +
            std::string(strerror(errno));
        throw BTIOException(errno, msg);
    }

    if (do_connect)
        connect();
}

GATTRequester::~GATTRequester() {
    if (_channel != NULL) {
        g_io_channel_shutdown(_channel, TRUE, NULL);
        g_io_channel_unref(_channel);
    }

    if (_hci_socket > -1)
        hci_close_dev(_hci_socket);

    if (_attrib != NULL) {
        g_attrib_unref(_attrib);
    }
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
events_handler(const uint8_t* data, uint16_t size, gpointer userp) {
    GATTRequester* request = (GATTRequester*)userp;
    uint16_t handle = htobs(bt_get_le16(&data[1]));

    switch(data[0]) {
    case ATT_OP_HANDLE_NOTIFY:
        {
            PyGILGuard guard;
            request->on_notification(handle, std::string((const char*)data, size));
        }
        return;
    case ATT_OP_HANDLE_IND:
        {
            PyGILGuard guard;
            request->on_indication(handle, std::string((const char*)data, size));
        }
        break;
    default:
        return;
    }

    size_t plen;
    uint8_t* output = g_attrib_get_buffer(request->_attrib, &plen);
    uint16_t olen = enc_confirmation(output, plen);

    if (olen > 0)
        g_attrib_send(request->_attrib, 0, output, olen, NULL, NULL, NULL);
}

void
events_destroy(gpointer userp)
{
    PyGILGuard guard;
    GATTRequester* request = (GATTRequester*)userp;
    // This is called for each of the two events we register for, so release
    // each one's reference individually.
    request->decref();
}

void
connect_cb(GIOChannel* channel, GError* err, gpointer userp) {
    PyGILGuard guard;
    GATTRequester* request = (GATTRequester*)userp;

    if (err) {
        std::cout << "PyGattLib ERROR: " << std::string(err->message) << std::endl;

        request->_state = GATTRequester::STATE_ERROR_CONNECTING;
        g_error_free(err);
        request->decref();
        return;
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
    else if (cid == ATT_CID)
        mtu = ATT_DEFAULT_LE_MTU;

    request->_attrib = g_attrib_new(channel, mtu);

    request->incref();
    g_attrib_register(request->_attrib, ATT_OP_HANDLE_NOTIFY,
            GATTRIB_ALL_HANDLES, events_handler, userp, events_destroy);
    request->incref();
    g_attrib_register(request->_attrib, ATT_OP_HANDLE_IND, GATTRIB_ALL_HANDLES,
            events_handler, userp, events_destroy);

    request->_state = GATTRequester::STATE_CONNECTED;
    request->_ready.set();
    request->decref();
}

gboolean
disconnect_cb(GIOChannel* channel, GIOCondition cond, gpointer userp) {
    PyGILGuard guard;
    GATTRequester* request = (GATTRequester*)userp;
    if (channel == request->get_channel())
        request->disconnect();
    return false;
}

void
GATTRequester::connect(
        bool wait, std::string channel_type,
        std::string security_level, int psm, int mtu) {

    if (_state != STATE_DISCONNECTED)
        throw BTIOException(EISCONN, "Already connecting or connected");

    _state = STATE_CONNECTING;

    GError *gerr = NULL;
    incref();
    {
        PyThreadsGuard guard;

        _channel = gatt_connect
            (_device.c_str(),          // 'hciX'
             _address.c_str(),         // 'mac address'
             channel_type.c_str(),     // 'public' '[public | random]'
             security_level.c_str(),   // sec_level, '[low | medium | high]'
             psm,                      // 0, psm
             mtu,                      // 0, mtu
             connect_cb,
             &gerr,
             (gpointer)this);
    }

    if (_channel == NULL) {
        _state = STATE_DISCONNECTED;

        std::string msg(gerr->message);
        int ecode = gerr->code;
        g_error_free(gerr);
        throw BTIOException(ecode, msg);
    }

    incref();
    g_io_add_watch(_channel, G_IO_HUP, disconnect_cb, (gpointer)this);
    if (wait) {
        PyThreadsGuard guard;
        check_channel();
    }
}

boost::python::object
GATTRequester::connect_kwarg(boost::python::tuple args, boost::python::dict kwargs)
{
	// Static method wrapper around connect. First obtain self/this
	GATTRequester& self = boost::python::extract<GATTRequester&>(args[0]);

	// Argument default values.
	bool wait=false;
	std::string channel_type="public";
	std::string security_level="low";
	int psm=0;
	int mtu=0;
	int kwargsused = 0;

	// Extract each argument either positionally or from the keyword arguments
	if (boost::python::len(args) > 1) {
		wait = boost::python::extract<bool>(args[1]);
	} else if (kwargs.has_key("wait")) {
		wait = boost::python::extract<bool>(kwargs.get("wait"));
		kwargsused++;
	}
	if (boost::python::len(args) > 2) {
		channel_type = boost::python::extract<std::string>(args[2]);
	} else if (kwargs.has_key("channel_type")) {
		channel_type = boost::python::extract<std::string>(kwargs.get("channel_type"));
		kwargsused++;
	}
	if (boost::python::len(args) > 3) {
		security_level = boost::python::extract<std::string>(args[3]);
	} else if (kwargs.has_key("security_level")) {
		security_level = boost::python::extract<std::string>(kwargs.get("security_level"));
		kwargsused++;
	}
	if (boost::python::len(args) > 4) {
		psm = boost::python::extract<int>(args[4]);
	} else if (kwargs.has_key("psm")) {
		psm = boost::python::extract<int>(kwargs.get("psm"));
		kwargsused++;
	}
	if (boost::python::len(args) > 5) {
		mtu = boost::python::extract<int>(args[5]);
	} else if (kwargs.has_key("mtu")) {
		mtu = boost::python::extract<int>(kwargs.get("mtu"));
		kwargsused++;
	}

	// Check that we have used all keyword arguments
	if (kwargsused != boost::python::len(kwargs))
            throw BTIOException(EINVAL, "Error in keyword arguments");

	// Call the real method
	self.connect(wait, channel_type, security_level, psm, mtu);

	return boost::python::object(); // boost-ism for "None"
}

bool
GATTRequester::is_connected() {
    return _state == STATE_CONNECTED;
}

void
GATTRequester::disconnect() {
    if (_state == STATE_DISCONNECTED)
        return;

    g_attrib_unref(_attrib);
    _attrib = NULL;

    g_io_channel_shutdown(_channel, false, NULL);
    g_io_channel_unref(_channel);
    _channel = NULL;

    _state = STATE_DISCONNECTED;
    decref();
}

static void
read_by_handle_cb(guint8 status, const guint8* data,
        guint16 size, gpointer userp) {
    // Note: first byte is the opcode (ATT_READ_RSP 0x0b), remove it
    PyGILGuard guard;
    GATTResponse* response = (GATTResponse*)userp;
    if (!status && data) {
        PyObject* bytes = PyBytes_FromStringAndSize((const char*)data + 1, size - 1);
        response->on_response(boost::python::object(boost::python::handle<>(bytes)));
    }
    response->notify(status);
    response->decref();
}

void
GATTRequester::read_by_handle_async(uint16_t handle, GATTResponse* response) {
    check_channel();
    response->incref();
    gatt_read_char(_attrib, handle, read_by_handle_cb, (gpointer)response);
}

boost::python::list
GATTRequester::read_by_handle(uint16_t handle) {
    boost::python::object pyresponse = pyGATTResponse();
    GATTResponse &response = boost::python::extract<GATTResponse&>(pyresponse)();

    {
        PyThreadsGuard guard;

        read_by_handle_async(handle, &response);

        if (not response.wait(MAX_WAIT_FOR_PACKET))
            throw GATTException(ATT_ECODE_TIMEOUT, "Device is not responding!");
    }

    return response.received();
}

static void
read_by_uuid_cb(guint8 status, const guint8* data,
        guint16 size, gpointer userp) {

    PyGILGuard guard;
    GATTResponse* response = (GATTResponse*)userp;
    if (status || !data) {
        response->notify(status);
        response->decref();
        return;
    }

    struct att_data_list* list = dec_read_by_type_resp(data, size);
    if (list == NULL) {
        response->notify(ATT_ECODE_ABORTED);
        return;
    }

    for (int i=0; i<list->num; i++) {
        uint8_t* item = list->data[i];

        // Remove handle addr
        item += 2;

        PyObject* bytes = PyBytes_FromStringAndSize((const char*)item, list->len - 2);
        response->on_response(boost::python::object(boost::python::handle<>(bytes)));
    }

    att_data_list_free(list);
    response->notify(status);
    response->decref();
}

void
GATTRequester::read_by_uuid_async(std::string uuid, GATTResponse* response) {
    uint16_t start = 0x0001;
    uint16_t end = 0xffff;
    bt_uuid_t btuuid;

    check_channel();
    if (bt_string_to_uuid(&btuuid, uuid.c_str()) < 0)
        throw BTIOException(EINVAL, "Invalid UUID\n");

    response->incref();
    gatt_read_char_by_uuid(_attrib, start, end, &btuuid, read_by_uuid_cb,
                           (gpointer)response);

}

boost::python::list
GATTRequester::read_by_uuid(std::string uuid) {
    boost::python::object pyresponse = pyGATTResponse();
    GATTResponse &response = boost::python::extract<GATTResponse&>(pyresponse)();

    {
        PyThreadsGuard guard;

        read_by_uuid_async(uuid, &response);

        if (not response.wait(MAX_WAIT_FOR_PACKET))
            throw GATTException(ATT_ECODE_TIMEOUT, "Device is not responding!");
    }

    return response.received();
}

static void
write_by_handle_cb(guint8 status, const guint8* data,
        guint16 size, gpointer userp) {

    PyGILGuard guard;
    GATTResponse* response = (GATTResponse*)userp;
    if (status == 0 && data) {
        PyObject* bytes = PyBytes_FromStringAndSize((const char*)data, size);
        response->on_response(boost::python::object(boost::python::handle<>(bytes)));
    }
    response->notify(status);
    response->decref();
}

void
GATTRequester::write_by_handle_async(uint16_t handle, std::string data,
                                     GATTResponse* response) {

    check_channel();
    response->incref();
    gatt_write_char(_attrib, handle, (const uint8_t*)data.data(), data.size(),
                    write_by_handle_cb, (gpointer)response);
}

boost::python::list
GATTRequester::write_by_handle(uint16_t handle, std::string data) {
    boost::python::object pyresponse = pyGATTResponse();
    GATTResponse &response = boost::python::extract<GATTResponse&>(pyresponse)();

    {
        PyThreadsGuard guard;

        write_by_handle_async(handle, data, &response);

        if (not response.wait(MAX_WAIT_FOR_PACKET))
            throw GATTException(ATT_ECODE_TIMEOUT, "Device is not responding!");
    }

    return response.received();
}

void
GATTRequester::write_cmd(uint16_t handle, std::string data) {
    check_channel();
    gatt_write_cmd(_attrib, handle, (const uint8_t*)data.data(), data.size(),
                   NULL, NULL);
}

void
GATTRequester::check_channel() {

    for (int c=MAX_WAIT_FOR_PACKET; c>0; c--) {
        if (_state == STATE_CONNECTED)
            return;

        if (_state != STATE_CONNECTING)
            throw BTIOException(ECONNRESET, "Channel or attrib disconnected");

        if (_ready.wait(1))
            return;
    }

    throw BTIOException(ETIMEDOUT, "Channel or attrib not ready");
}

static void
discover_primary_cb(guint8 status, GSList *services, void *userp) {

    PyGILGuard guard;
    GATTResponse* response = (GATTResponse*)userp;
    if (status || !services) {
        response->notify(status);
        response->decref();
        return;
    }

    for (GSList * l = services; l; l = l->next) {
        struct gatt_primary *prim = (gatt_primary*) l->data;
        boost::python::dict sdescr;
        sdescr["uuid"] = prim->uuid;
        sdescr["start"] = prim->range.start;
        sdescr["end"] = prim->range.end;
        response->on_response(sdescr);
    }

    response->notify(status);
    response->decref();
}


void
GATTRequester::discover_primary_async(GATTResponse* response) {
    check_connected();
    response->incref();
    if( not gatt_discover_primary(
            _attrib, NULL, discover_primary_cb, (gpointer)response)) {
        response->decref();
        throw BTIOException(ENOMEM, "Discover primary failed");
    }
}

boost::python::list GATTRequester::discover_primary() {
    boost::python::object pyresponse = pyGATTResponse();
    GATTResponse &response = boost::python::extract<GATTResponse&>(pyresponse)();

    {
        PyThreadsGuard guard;

	discover_primary_async(&response);

	if (not response.wait(5 * MAX_WAIT_FOR_PACKET))
            throw GATTException(ATT_ECODE_TIMEOUT, "Device is not responding!");
    }

    return response.received();
}

/* Characteristics Discovery

 */
static void discover_char_cb(guint8 status, GSList *characteristics,
        void *user_data) {
    PyGILGuard guard;
    GATTResponse* response = (GATTResponse*) user_data;
    if (status || !characteristics) {
        response->notify(status);
        response->decref();
        return;
    }

    for (GSList * l = characteristics; l; l = l->next) {
        struct gatt_char *chars = (gatt_char*) l->data;
        boost::python::dict adescr;
        adescr["uuid"] = chars->uuid;
        adescr["handle"] = chars->handle;
        adescr["properties"] = chars->properties;
        adescr["value_handle"] = chars->value_handle;
        response->on_response(adescr);
    }

    response->notify(status);
    response->decref();
}

void GATTRequester::discover_characteristics_async(GATTResponse* response,
        int start, int end, std::string uuid_str) {
    check_connected();

    if (uuid_str.size() == 0) {
        //TODO handle error
        response->incref();
        gatt_discover_char(_attrib, start, end, NULL, discover_char_cb,
                (gpointer) response);
    } else {
        bt_uuid_t uuid;
        if (bt_string_to_uuid(&uuid, uuid_str.c_str()) < 0) {
            throw BTIOException(EINVAL, "Invalid UUID");
        }
        //TODO handle error
        response->incref();
        gatt_discover_char(_attrib, start, end, &uuid, discover_char_cb,
                (gpointer) response);
    }
}

boost::python::list GATTRequester::discover_characteristics(int start, int end,
        std::string uuid_str) {
    boost::python::object pyresponse = pyGATTResponse();
    GATTResponse &response = boost::python::extract<GATTResponse&>(pyresponse)();

    {
        PyThreadsGuard guard;

        discover_characteristics_async(&response, start, end, uuid_str);

        if (not response.wait(5 * MAX_WAIT_FOR_PACKET))
            throw GATTException(ATT_ECODE_TIMEOUT, "Device is not responding!");
    }

    return response.received();
}

void GATTRequester::check_connected() {
    if (_state != STATE_CONNECTED)
        throw BTIOException(ENOTCONN, "Not connected");
}
