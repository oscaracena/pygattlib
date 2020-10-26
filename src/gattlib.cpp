// -*- mode: c++; coding: utf-8 -*-

// Copyright (C) 2014,2020 Oscar Acena <oscaracena@gmail.com>
// This software is under the terms of Apache License v2 or later.

#include <boost/thread/thread.hpp>
#include <boost/python/dict.hpp>
#include <boost/python/extract.hpp>
#include <boost/python/str.hpp>
#include <boost/python/long.hpp>
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

class PyKwargsExtracter
{
public:
    PyKwargsExtracter(boost::python::tuple &args_, boost::python::dict &kwargs_, int start_)
        : args(args_), kwargs(kwargs_), curarg(start_-1),
          kwargsused(0)
    {
    }

    template<typename T>
    bool extract(T &dst, const char *name)
    {
        curarg++;
        if (boost::python::len(args) > curarg) {
            dst = boost::python::extract<T>(args[curarg]);
            return true;
        } else if (kwargs.has_key(name)) {
            kwargsused++;
            dst = boost::python::extract<T>(kwargs.get(name));
            return true;
        }
        return false;
    }

    template<typename T>
    T extract(const char *name, const T& defval)
    {
        curarg++;
        if (boost::python::len(args) > curarg) {
            return boost::python::extract<T>(args[curarg]);
        } else if (kwargs.has_key(name)) {
            kwargsused++;
            return boost::python::extract<T>(kwargs.get(name));
        } else {
            return defval;
        }
    }

    bool used_all_kwargs() { return kwargsused==boost::python::len(kwargs); }

    boost::python::tuple &args;
    boost::python::dict &kwargs;
    int curarg;
    int kwargsused;
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

    boost::thread iothread(std::bind(&IOService::operator(), this));
    start_event.wait(10);
}

void
IOService::stop() {
    g_main_loop_quit(event_loop);
}

void
IOService::operator()() {
    context = g_main_context_new();
    g_main_context_push_thread_default(context);
    event_loop = g_main_loop_new(context, FALSE);
    bt_io_set_context(context);
    start_event.set();

    g_main_loop_run(event_loop);
    g_main_loop_unref(event_loop);
    bt_io_set_context(NULL);
    g_main_context_pop_thread_default(context);
    g_main_context_unref(context);
}

static volatile IOService _instance(true);

GATTResponse::GATTResponse(PyObject* p) :
    GATTPyBase(p),
    _complete(false), _status(0), _list(false) {
}

void
GATTResponse::on_response(boost::python::object data) {
    if (_list) {
        boost::python::list list = boost::python::extract<boost::python::list>(_data);
        list.append(data);
    } else {
        _data = data;
    }
}

void
GATTResponse::notify(uint8_t status) {
    _status = status;
    _complete = true;
    if (!status) {
      on_response_complete();
    } else {
      on_response_failed(status);
    }
    _event.set();
}

void
GATTResponse::expect_list()
{
    _list = true;
    _data = boost::python::list();
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

bool
GATTResponse::wait_locked(uint16_t timeout) {
    PyThreadsGuard guard;
    return wait(timeout);
}

bool
GATTResponse::complete() {
    return _complete;
}

int
GATTResponse::status() {
    return _status;
}

boost::python::object
GATTResponse::received() {
    return _data;
}


GATTRequester::GATTRequester(PyObject* p, std::string address, bool do_connect,
        std::string device) :
    GATTPyBase(p),
    _state(STATE_DISCONNECTED),
    _device(device),
    _address(address),
    _conn_interval_min(24),
    _conn_interval_max(40),
    _slave_latency(0),
    _supervision_timeout(700),
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
        request->on_connect_failed(err->code);
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

    request->_attrib = g_attrib_withlock_new(channel, mtu, &request->attriblocker);

    request->incref();
    g_attrib_register(request->_attrib, ATT_OP_HANDLE_NOTIFY,
            GATTRIB_ALL_HANDLES, events_handler, userp, events_destroy);
    request->incref();
    g_attrib_register(request->_attrib, ATT_OP_HANDLE_IND, GATTRIB_ALL_HANDLES,
            events_handler, userp, events_destroy);

    request->_state = GATTRequester::STATE_CONNECTED;
    request->on_connect(mtu);
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
        decref();

        std::string msg(gerr->message);
        int ecode = gerr->code;
        g_error_free(gerr);
        throw BTIOException(ecode, msg);
    }

    incref();
    x_g_io_add_watch(_channel, G_IO_HUP, disconnect_cb, (gpointer)this);
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

        PyKwargsExtracter e(args, kwargs, 1);
        bool wait = e.extract<bool>("wait", false);
        std::string channel_type = e.extract<std::string>("channel_type", "public");
        std::string security_level = e.extract<std::string>("security_level", "low");
        int psm = e.extract<int>("psm", 0);
        int mtu = e.extract<int>("mtu", 0);

        self.extract_connection_parameters(e);

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
    on_disconnect();
    decref();
}

static void exchange_mtu_cb(guint8 status, const guint8 *pdu, guint16 len, gpointer user_data)
{
    PyGILGuard guard;
    GATTResponse* response = (GATTResponse*) user_data;

    if (!status && pdu && len>=3) {
        uint16_t mtu = bt_get_le16(&pdu[1]);
        response->on_response(boost::python::long_(mtu));
    }

    response->notify(status);
    response->decref();
}

void
GATTRequester::exchange_mtu_async(uint16_t mtu, GATTResponse* response)
{
    check_channel();
    response->incref();
    if ( not gatt_exchange_mtu(_attrib, mtu, exchange_mtu_cb, (gpointer)response)) {
      response->decref();
      throw BTIOException(ENOMEM, "Exchange MTU failed");
    }
}

boost::python::object
GATTRequester::exchange_mtu(uint16_t mtu)
{
    boost::python::object pyresponse = pyGATTResponse();
    GATTResponse &response = boost::python::extract<GATTResponse&>(pyresponse)();

    {
        PyThreadsGuard guard;

        exchange_mtu_async(mtu, &response);

        if (not response.wait(MAX_WAIT_FOR_PACKET))
            throw GATTException(ATT_ECODE_TIMEOUT, "Device is not responding!");
    }

    return response.received();
}

void
GATTRequester::set_mtu(uint16_t mtu)
{
  if (mtu < ATT_DEFAULT_LE_MTU ||
      mtu > ATT_MAX_VALUE_LEN) {
    throw BTIOException(EINVAL, "MTU must be between 23 and 512");
  }
  g_attrib_set_mtu(_attrib, mtu);
}

static void
read_by_handle_cb(guint8 status, const guint8* data,
        guint16 size, gpointer userp) {
    // Note: first byte is the opcode (ATT_READ_RSP 0x0b), remove it
    PyGILGuard guard;
    GATTResponse* response = (GATTResponse*)userp;
    if (!status && data) {
        PyObject* bytes = PyBytes_FromStringAndSize((const char*)data + 1, size - 1);
        response->expect_list();
        response->on_response(boost::python::object(boost::python::handle<>(bytes)));
    }
    response->notify(status);
    response->decref();
}

void
GATTRequester::read_by_handle_async(uint16_t handle, GATTResponse* response) {
    check_channel();
    response->incref();
    if ( not gatt_read_char(_attrib, handle, read_by_handle_cb, (gpointer)response)) {
      response->decref();
      throw BTIOException(ENOMEM, "Read characteristic failed");
    }
}

boost::python::object
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

    response->expect_list();

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
    if ( not gatt_read_char_by_uuid(_attrib, start, end, &btuuid, read_by_uuid_cb,
                                    (gpointer)response)) {
      response->decref();
      throw BTIOException(ENOMEM, "Read characteristic failed");
    }

}

boost::python::object
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
        response->expect_list();
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
    if ( not gatt_write_char(_attrib, handle, (const uint8_t*)data.data(), data.size(),
                             write_by_handle_cb, (gpointer)response)) {
      response->decref();
      throw BTIOException(ENOMEM, "Write characteristic failed");
    }
}

boost::python::object
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
    if ( not gatt_write_cmd(_attrib, handle, (const uint8_t*)data.data(), data.size(),
                            NULL, NULL)) {
      throw BTIOException(ENOMEM, "Write command failed");
    }

}

void
GATTRequester::enable_notifications_async(uint16_t handle, bool notifications, bool indications,
                                          GATTResponse* response) {
    check_channel();
    uint8_t val[2] = { 0, 0 };
    if (notifications) val[0] |= GATT_CLIENT_CHARAC_CFG_NOTIF_BIT;
    if (indications) val[0] |= GATT_CLIENT_CHARAC_CFG_IND_BIT;
    response->incref();
    if ( not gatt_write_char(_attrib, handle, val, 2,
                             write_by_handle_cb, (gpointer)response)) {
      response->decref();
      throw BTIOException(ENOMEM, "Write characteristic failed");
    }

}

void
GATTRequester::enable_notifications(uint16_t handle, bool notifications, bool indications) {
    boost::python::object pyresponse = pyGATTResponse();
    GATTResponse &response = boost::python::extract<GATTResponse&>(pyresponse)();

    {
        PyThreadsGuard guard;

        enable_notifications_async(handle, notifications, indications, &response);

        if (not response.wait(MAX_WAIT_FOR_PACKET))
            throw GATTException(ATT_ECODE_TIMEOUT, "Device is not responding!");
    }
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

boost::python::object
GATTRequester::update_connection_parameters_kwarg(boost::python::tuple args, boost::python::dict kwargs)
{
    GATTRequester& self = boost::python::extract<GATTRequester&>(args[0]);
    PyKwargsExtracter e(args, kwargs, 1);
    self.extract_connection_parameters(e);
    self.update_connection_parameters();
    return boost::python::object();
}

void
GATTRequester::extract_connection_parameters(PyKwargsExtracter &e)
{
    uint16_t conn_interval_min = _conn_interval_min;
    uint16_t conn_interval_max = _conn_interval_max;
    uint16_t slave_latency = _slave_latency;
    uint16_t supervision_timeout = _supervision_timeout;

    if (e.extract<uint16_t>(conn_interval_min, "conn_interval_min")) {
        if ((conn_interval_min<0x0006 || conn_interval_min>0x0c80) &&
            conn_interval_min!=0xffff)
            throw BTIOException(EINVAL, "conn_interval_min must be between 6 and 0xc80, or 0xffff");
    }
    if (e.extract<uint16_t>(conn_interval_max, "conn_interval_max")) {
        if ((conn_interval_max<0x0006 || conn_interval_max>0x0c80) &&
            conn_interval_max!=0xffff)
            throw BTIOException(EINVAL, "conn_interval_max must be between 6 and 0xc80, or 0xffff");
    }
    if (conn_interval_min!=0xffff && conn_interval_min>conn_interval_max)
        throw BTIOException(EINVAL, "conn_interval_max must be greater then or equal to conn_interval_min");
    if (e.extract<uint16_t>(slave_latency, "slave_latency")) {
        if (slave_latency>0x01f3)
            throw BTIOException(EINVAL, "slave_latency must be between 0 and 0x1f3");
    }
    if (e.extract<uint16_t>(supervision_timeout, "supervision_timeout")) {
        if ((supervision_timeout<0x000a || supervision_timeout>0x0c80) &&
            supervision_timeout!=0xffff)
            throw BTIOException(EINVAL, "supervision_timeout must be between 0xa and 0xc80, or 0xffff");
    }

    // Check that we have used all keyword arguments
    if (!e.used_all_kwargs())
        throw BTIOException(EINVAL, "Error in keyword arguments");

    _conn_interval_min = conn_interval_min;
    _conn_interval_max = conn_interval_max;
    _slave_latency = slave_latency;
    _supervision_timeout = supervision_timeout;
}

void
GATTRequester::update_connection_parameters()
{
    // Update connection settings (supervisor timeut > 0.42 s)
    int l2cap_sock = g_io_channel_unix_get_fd(_channel);
    struct l2cap_conninfo info;
    socklen_t info_size = sizeof(info);

    getsockopt(l2cap_sock, SOL_L2CAP, L2CAP_CONNINFO, &info, &info_size);
    int handle = info.hci_handle;

    int retval = hci_le_conn_update(
            _hci_socket, handle,
            _conn_interval_min, _conn_interval_max,
            _slave_latency, _supervision_timeout, 25000);
    if (retval < 0) {
        std::string msg = "Could not update HCI connection: ";
        msg += strerror(errno);
        throw BTIOException(errno, msg);
    }
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

    response->expect_list();

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

boost::python::object GATTRequester::discover_primary() {
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

/* Included Service Discovery

 */
static void
find_included_cb(guint8 status, GSList *includes, void *userp) {

    PyGILGuard guard;
    GATTResponse* response = (GATTResponse*)userp;
    if (status || !includes) {
        response->notify(status);
        response->decref();
        return;
    }

    response->expect_list();

    for (GSList * l = includes; l; l = l->next) {
        struct gatt_included *incl = (gatt_included*) l->data;
        boost::python::dict sdescr;
        sdescr["uuid"] = incl->uuid;
        sdescr["handle"] = incl->handle;
        sdescr["start"] = incl->range.start;
        sdescr["end"] = incl->range.end;
        response->on_response(sdescr);
    }

    response->notify(status);
    response->decref();
}

void
GATTRequester::find_included_async(GATTResponse* response, int start, int end) {
    check_connected();
    response->incref();
    if( not gatt_find_included(
            _attrib, start, end, find_included_cb, (gpointer)response)) {
        response->decref();
        throw BTIOException(ENOMEM, "Find included failed");
    }
}

boost::python::object GATTRequester::find_included(int start, int end) {
    boost::python::object pyresponse = pyGATTResponse();
    GATTResponse &response = boost::python::extract<GATTResponse&>(pyresponse)();

    {
        PyThreadsGuard guard;

        find_included_async(&response, start, end);

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

    response->expect_list();

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
        response->incref();
        if ( not gatt_discover_char(_attrib, start, end, NULL, discover_char_cb,
                                    (gpointer) response)) {
          response->decref();
          throw BTIOException(ENOMEM, "Discover characteristics failed");
        }
    } else {
        bt_uuid_t uuid;
        if (bt_string_to_uuid(&uuid, uuid_str.c_str()) < 0) {
            throw BTIOException(EINVAL, "Invalid UUID");
        }
        response->incref();
        if ( not gatt_discover_char(_attrib, start, end, &uuid, discover_char_cb,
                                    (gpointer) response)) {
          response->decref();
          throw BTIOException(ENOMEM, "Discover characteristics failed");
        }
    }
}

boost::python::object GATTRequester::discover_characteristics(int start, int end,
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

/* Descriptors Discovery

 */
static void discover_desc_cb(guint8 status, GSList *descriptors,
        void *user_data) {
    PyGILGuard guard;
    GATTResponse* response = (GATTResponse*) user_data;
    if (status || !descriptors) {
        response->notify(status);
        response->decref();
        return;
    }

    response->expect_list();

    for (GSList * l = descriptors; l; l = l->next) {
        struct gatt_desc *descs = (gatt_desc*) l->data;
        boost::python::dict adescr;
        adescr["uuid"] = descs->uuid;
        adescr["handle"] = descs->handle;
        response->on_response(adescr);
    }

    response->notify(status);
    response->decref();
}

void GATTRequester::discover_descriptors_async(GATTResponse* response,
        int start, int end, std::string uuid_str) {
    check_connected();

    if (uuid_str.size() == 0) {
        response->incref();
        if ( not gatt_discover_desc(_attrib, start, end, NULL, discover_desc_cb,
                                    (gpointer) response)) {
          response->decref();
          throw BTIOException(ENOMEM, "Discover descriptors failed");
        }
    } else {
        bt_uuid_t uuid;
        if (bt_string_to_uuid(&uuid, uuid_str.c_str()) < 0) {
            throw BTIOException(EINVAL, "Invalid UUID");
        }
        response->incref();
        if ( not gatt_discover_desc(_attrib, start, end, &uuid, discover_char_cb,
                                    (gpointer) response)) {
          response->decref();
          throw BTIOException(ENOMEM, "Discover descriptors failed");
        }
    }
}

boost::python::object GATTRequester::discover_descriptors(int start, int end,
        std::string uuid_str) {
    boost::python::object pyresponse = pyGATTResponse();
    GATTResponse &response = boost::python::extract<GATTResponse&>(pyresponse)();

    {
        PyThreadsGuard guard;

        discover_descriptors_async(&response, start, end, uuid_str);

        if (not response.wait(5 * MAX_WAIT_FOR_PACKET))
            throw GATTException(ATT_ECODE_TIMEOUT, "Device is not responding!");
    }

    return response.received();
}

void GATTRequester::check_connected() {
    if (_state != STATE_CONNECTED)
        throw BTIOException(ENOTCONN, "Not connected");
}
