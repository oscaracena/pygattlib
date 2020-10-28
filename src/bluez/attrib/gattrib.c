/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2010  Nokia Corporation
 *  Copyright (C) 2010  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <glib.h>

#include <stdio.h>

#include <bluetooth/bluetooth.h>

#include "btio/btio.h"
#include "lib/uuid.h"
#include "src/shared/util.h"
#include "src/log.h"
#include "attrib/att.h"
#include "attrib/gattrib.h"

#define GATT_TIMEOUT 30

struct _GAttrib {
	GIOChannel *io;
	int refs;
	struct _GAttribLock *lk;
	uint8_t *buf;
	size_t buflen;
	guint read_watch;
	guint write_watch;
	guint timeout_watch;
	GQueue *requests;
	GQueue *responses;
	GSList *events;
	guint next_cmd_id;
	GDestroyNotify destroy;
	gpointer destroy_user_data;
	bool stale;
};

struct command {
	guint id;
	guint8 opcode;
	guint8 *pdu;
	guint16 len;
	guint8 expected;
	bool sent;
	GAttribResultFunc func;
	gpointer user_data;
	GDestroyNotify notify;
};

struct event {
	guint id;
	guint8 expected;
	guint16 handle;
	GAttribNotifyFunc func;
	gpointer user_data;
	GDestroyNotify notify;
};

static guint8 opcode2expected(guint8 opcode)
{
	switch (opcode) {
	case ATT_OP_MTU_REQ:
		return ATT_OP_MTU_RESP;

	case ATT_OP_FIND_INFO_REQ:
		return ATT_OP_FIND_INFO_RESP;

	case ATT_OP_FIND_BY_TYPE_REQ:
		return ATT_OP_FIND_BY_TYPE_RESP;

	case ATT_OP_READ_BY_TYPE_REQ:
		return ATT_OP_READ_BY_TYPE_RESP;

	case ATT_OP_READ_REQ:
		return ATT_OP_READ_RESP;

	case ATT_OP_READ_BLOB_REQ:
		return ATT_OP_READ_BLOB_RESP;

	case ATT_OP_READ_MULTI_REQ:
		return ATT_OP_READ_MULTI_RESP;

	case ATT_OP_READ_BY_GROUP_REQ:
		return ATT_OP_READ_BY_GROUP_RESP;

	case ATT_OP_WRITE_REQ:
		return ATT_OP_WRITE_RESP;

	case ATT_OP_PREP_WRITE_REQ:
		return ATT_OP_PREP_WRITE_RESP;

	case ATT_OP_EXEC_WRITE_REQ:
		return ATT_OP_EXEC_WRITE_RESP;

	case ATT_OP_HANDLE_IND:
		return ATT_OP_HANDLE_CNF;
	}

	return 0;
}

static bool is_response(guint8 opcode)
{
	switch (opcode) {
	case ATT_OP_ERROR:
	case ATT_OP_MTU_RESP:
	case ATT_OP_FIND_INFO_RESP:
	case ATT_OP_FIND_BY_TYPE_RESP:
	case ATT_OP_READ_BY_TYPE_RESP:
	case ATT_OP_READ_RESP:
	case ATT_OP_READ_BLOB_RESP:
	case ATT_OP_READ_MULTI_RESP:
	case ATT_OP_READ_BY_GROUP_RESP:
	case ATT_OP_WRITE_RESP:
	case ATT_OP_PREP_WRITE_RESP:
	case ATT_OP_EXEC_WRITE_RESP:
	case ATT_OP_HANDLE_CNF:
		return true;
	}

	return false;
}

static bool is_request(guint8 opcode)
{
	switch (opcode) {
	case ATT_OP_MTU_REQ:
	case ATT_OP_FIND_INFO_REQ:
	case ATT_OP_FIND_BY_TYPE_REQ:
	case ATT_OP_READ_BY_TYPE_REQ:
	case ATT_OP_READ_REQ:
	case ATT_OP_READ_BLOB_REQ:
	case ATT_OP_READ_MULTI_REQ:
	case ATT_OP_READ_BY_GROUP_REQ:
	case ATT_OP_WRITE_REQ:
	case ATT_OP_WRITE_CMD:
	case ATT_OP_PREP_WRITE_REQ:
	case ATT_OP_EXEC_WRITE_REQ:
		return true;
	}

	return false;
}

#define ALOCK(attrib) do { if (attrib->lk) (attrib->lk->lockfn)(attrib->lk); } while (0)
#define AUNLOCK(attrib) do { if (attrib->lk) (attrib->lk->unlockfn)(attrib->lk); } while (0)

GAttrib *g_attrib_ref(GAttrib *attrib)
{
	int refs;

	if (!attrib)
		return NULL;

	refs = __sync_add_and_fetch(&attrib->refs, 1);

	DBG("%p: ref=%d", attrib, refs);

	return attrib;
}

static void command_destroy(struct command *cmd)
{
	if (cmd->notify)
		cmd->notify(cmd->user_data);

	g_free(cmd->pdu);
	g_free(cmd);
}

static void event_destroy(struct event *evt)
{
	if (evt->notify)
		evt->notify(evt->user_data);

	g_free(evt);
}

static void attrib_destroy(GAttrib *attrib)
{
	GSList *l;
	struct command *c;

	while ((c = g_queue_pop_head(attrib->requests)))
		command_destroy(c);

	while ((c = g_queue_pop_head(attrib->responses)))
		command_destroy(c);

	g_queue_free(attrib->requests);
	attrib->requests = NULL;

	g_queue_free(attrib->responses);
	attrib->responses = NULL;

	for (l = attrib->events; l; l = l->next)
		event_destroy(l->data);

	g_slist_free(attrib->events);
	attrib->events = NULL;

	if (attrib->timeout_watch > 0)
		x_g_source_remove(attrib->timeout_watch);

	if (attrib->write_watch > 0)
		x_g_source_remove(attrib->write_watch);

	if (attrib->read_watch > 0)
		x_g_source_remove(attrib->read_watch);

	if (attrib->io)
		g_io_channel_unref(attrib->io);

	g_free(attrib->buf);

	if (attrib->destroy)
		attrib->destroy(attrib->destroy_user_data);

	g_free(attrib);
}

void g_attrib_unref(GAttrib *attrib)
{
	int refs;

	if (!attrib)
		return;

	refs = __sync_sub_and_fetch(&attrib->refs, 1);

	DBG("%p: ref=%d", attrib, refs);

	if (refs > 0)
		return;

	attrib_destroy(attrib);
}

GIOChannel *g_attrib_get_channel(GAttrib *attrib)
{
	if (!attrib)
		return NULL;

	return attrib->io;
}

gboolean g_attrib_set_destroy_function(GAttrib *attrib,
		GDestroyNotify destroy, gpointer user_data)
{
	if (attrib == NULL)
		return FALSE;

	attrib->destroy = destroy;
	attrib->destroy_user_data = user_data;

	return TRUE;
}

static gboolean disconnect_timeout(gpointer data)
{
	struct _GAttrib *attrib = data;
	struct command *c;

	g_attrib_ref(attrib);

        ALOCK(attrib);

	c = g_queue_pop_head(attrib->requests);
	if (c == NULL)
		goto done;

        AUNLOCK(attrib);

	if (c->func)
		c->func(ATT_ECODE_TIMEOUT, NULL, 0, c->user_data);

	command_destroy(c);

        ALOCK(attrib);

	while ((c = g_queue_pop_head(attrib->requests))) {
                AUNLOCK(attrib);
		if (c->func)
			c->func(ATT_ECODE_ABORTED, NULL, 0, c->user_data);
		command_destroy(c);
                ALOCK(attrib);
	}

done:
	attrib->stale = true;
        AUNLOCK(attrib);

	g_attrib_unref(attrib);

	return FALSE;
}

static gboolean can_write_data(GIOChannel *io, GIOCondition cond,
								gpointer data)
{
	struct _GAttrib *attrib = data;
	struct command *cmd;
	GError *gerr = NULL;
	gsize len;
	GIOStatus iostat;
	GQueue *queue;

        ALOCK(attrib);

	if (attrib->stale)
		goto done;

	if (cond & (G_IO_HUP | G_IO_ERR | G_IO_NVAL))
		goto done;

	queue = attrib->responses;
	cmd = g_queue_peek_head(queue);
	if (cmd == NULL) {
		queue = attrib->requests;
		cmd = g_queue_peek_head(queue);
	}
	if (cmd == NULL)
		goto done;

	/*
	 * Verify that we didn't already send this command. This can only
	 * happen with elementes from attrib->requests.
	 */
	if (cmd->sent)
		goto done;

	iostat = g_io_channel_write_chars(io, (char *) cmd->pdu, cmd->len,
								&len, &gerr);
	if (iostat != G_IO_STATUS_NORMAL) {
		if (gerr) {
			error("%s", gerr->message);
			g_error_free(gerr);
		}

		goto done;
	}

	if (cmd->expected == 0) {
		g_queue_pop_head(queue);
                AUNLOCK(attrib);
		command_destroy(cmd);

		return TRUE;
	}

	cmd->sent = true;

	if (attrib->timeout_watch == 0)
		attrib->timeout_watch = x_g_timeout_add_seconds(GATT_TIMEOUT,
						disconnect_timeout, attrib);

done:
        AUNLOCK(attrib);
	return FALSE;
}

static void destroy_sender(gpointer data)
{
	struct _GAttrib *attrib = data;

        ALOCK(attrib);
	attrib->write_watch = 0;
        AUNLOCK(attrib);
	g_attrib_unref(attrib);
}

static void wake_up_sender(struct _GAttrib *attrib)
{
        ALOCK(attrib);
	if (attrib->write_watch > 0)
		goto done;

	attrib = g_attrib_ref(attrib);
	attrib->write_watch = x_g_io_add_watch_full(attrib->io,
				G_PRIORITY_DEFAULT, G_IO_OUT,
				can_write_data, attrib, destroy_sender);
done:
        AUNLOCK(attrib);
}

static bool match_event(struct event *evt, const uint8_t *pdu, gsize len)
{
	guint16 handle;

	if (is_request(pdu[0]) && evt->expected == GATTRIB_ALL_REQS)
		return true;

	if (evt->expected == pdu[0] && evt->handle == GATTRIB_ALL_HANDLES)
		return true;

	if (len < 3)
		return false;

	handle = get_le16(&pdu[1]);

	if (evt->expected == pdu[0] && evt->handle == handle)
		return true;

	return false;
}

static gboolean received_data(GIOChannel *io, GIOCondition cond, gpointer data)
{
	struct _GAttrib *attrib = data;
	struct command *cmd = NULL;
	GSList *l;
	uint8_t buf[512], status;
	gsize len;
	GIOStatus iostat;
        gboolean rv = FALSE;
        gboolean needwake = FALSE;

        ALOCK(attrib);

	if (attrib->stale)
		goto notdone;

	if (cond & (G_IO_HUP | G_IO_ERR | G_IO_NVAL)) {
		struct command *c;

		while ((c = g_queue_pop_head(attrib->requests))) {
                        AUNLOCK(attrib);
			if (c->func)
				c->func(ATT_ECODE_IO, NULL, 0, c->user_data);
			command_destroy(c);
                        ALOCK(attrib);
		}

		attrib->read_watch = 0;

		goto notdone;
	}

	memset(buf, 0, sizeof(buf));

	iostat = g_io_channel_read_chars(io, (char *) buf, sizeof(buf),
								&len, NULL);
	if (iostat != G_IO_STATUS_NORMAL) {
		status = ATT_ECODE_IO;
		goto done;
	}

	for (l = attrib->events; l; l = l->next) {
		struct event *evt = l->data;

		if (match_event(evt, buf, len))
			evt->func(buf, len, evt->user_data);
	}

	if (!is_response(buf[0])) {
                rv = TRUE;
                goto notdone;
        }

	if (attrib->timeout_watch > 0) {
		x_g_source_remove(attrib->timeout_watch);
		attrib->timeout_watch = 0;
	}

	cmd = g_queue_pop_head(attrib->requests);
	if (cmd == NULL) {
		/* Keep the watch if we have events to report */
		rv = attrib->events != NULL;
                goto notdone;
	}

	if (buf[0] == ATT_OP_ERROR) {
		status = buf[4];
		goto done;
	}

	if (cmd->expected != buf[0]) {
		status = ATT_ECODE_IO;
		goto done;
	}

	status = 0;

done:
        needwake = !g_queue_is_empty(attrib->requests) ||
                !g_queue_is_empty(attrib->responses);

        AUNLOCK(attrib);

        if (needwake)
		wake_up_sender(attrib);

	if (cmd) {
		if (cmd->func)
			cmd->func(status, buf, len, cmd->user_data);

		command_destroy(cmd);
	}

	return TRUE;

notdone:
        AUNLOCK(attrib);
        return rv;
}

GAttrib *g_attrib_new(GIOChannel *io, guint16 mtu)
{
  return g_attrib_withlock_new(io, mtu, NULL);
}

GAttrib *g_attrib_withlock_new(GIOChannel *io, guint16 mtu, struct _GAttribLock *lk)
{
	struct _GAttrib *attrib;

	g_io_channel_set_encoding(io, NULL, NULL);
	g_io_channel_set_buffered(io, FALSE);

	attrib = g_try_new0(struct _GAttrib, 1);
	if (attrib == NULL)
		return NULL;

        attrib->lk = lk;
	attrib->buf = g_malloc0(mtu);
	attrib->buflen = mtu;

	attrib->io = g_io_channel_ref(io);
	attrib->requests = g_queue_new();
	attrib->responses = g_queue_new();

        ALOCK(attrib);

	attrib->read_watch = x_g_io_add_watch(attrib->io,
			G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
			received_data, attrib);

        AUNLOCK(attrib);

	return g_attrib_ref(attrib);
}

guint g_attrib_send(GAttrib *attrib, guint id, const guint8 *pdu, guint16 len,
			GAttribResultFunc func, gpointer user_data,
			GDestroyNotify notify)
{
	struct command *c;
	GQueue *queue;
	uint8_t opcode;
        guint rv = 0;
        gboolean needwake = FALSE;

        ALOCK(attrib);

	if (attrib->stale)
		goto done;

	c = g_try_new0(struct command, 1);
	if (c == NULL)
		goto done;

	opcode = pdu[0];

	c->opcode = opcode;
	c->expected = opcode2expected(opcode);
	c->pdu = g_malloc(len);
	memcpy(c->pdu, pdu, len);
	c->len = len;
	c->func = func;
	c->user_data = user_data;
	c->notify = notify;

	if (is_response(opcode))
		queue = attrib->responses;
	else
		queue = attrib->requests;

	if (id) {
		c->id = id;
		if (!is_response(opcode))
			g_queue_push_head(queue, c);
		else
			/* Don't re-order responses even if an ID is given */
			g_queue_push_tail(queue, c);
	} else {
		c->id = ++attrib->next_cmd_id;
		g_queue_push_tail(queue, c);
	}

	/*
	 * If a command was added to the queue and it was empty before, wake up
	 * the sender. If the sender was already woken up by the second queue,
	 * wake_up_sender will just return.
	 */
	needwake = (g_queue_get_length(queue) == 1);

	rv = c->id;

done:
        AUNLOCK(attrib);

        if (needwake)
                wake_up_sender(attrib);

        return rv;
}

static int command_cmp_by_id(gconstpointer a, gconstpointer b)
{
	const struct command *cmd = a;
	guint id = GPOINTER_TO_UINT(b);

	return cmd->id - id;
}

gboolean g_attrib_cancel(GAttrib *attrib, guint id)
{
	GList *l = NULL;
	struct command *cmd;
	GQueue *queue;

	if (attrib == NULL)
		return FALSE;

        ALOCK(attrib);

	queue = attrib->requests;
	if (queue)
		l = g_queue_find_custom(queue, GUINT_TO_POINTER(id),
					command_cmp_by_id);
	if (l == NULL) {
		queue = attrib->responses;
		if (!queue)
			goto done;
		l = g_queue_find_custom(queue, GUINT_TO_POINTER(id),
					command_cmp_by_id);
	}

	if (l == NULL)
		goto done;

	cmd = l->data;

	if (cmd == g_queue_peek_head(queue) && cmd->sent) {
		cmd->func = NULL;
                AUNLOCK(attrib);
	} else {
		g_queue_remove(queue, cmd);
                AUNLOCK(attrib);
		command_destroy(cmd);
	}

	return TRUE;

done:
        AUNLOCK(attrib);
        return FALSE;
}

static gboolean cancel_all_per_queue(GAttrib *attrib, GQueue *queue)
{
	struct command *c, *head = NULL;
	gboolean first = TRUE;

	if (queue == NULL)
		return FALSE;

	while ((c = g_queue_pop_head(queue))) {
		if (first && c->sent) {
			/* If the command was sent ignore its callback ... */
			c->func = NULL;
			head = c;
			continue;
		}

		first = FALSE;
                AUNLOCK(attrib);
		command_destroy(c);
                ALOCK(attrib);
	}

	if (head) {
		/* ... and put it back in the queue */
		g_queue_push_head(queue, head);
	}

	return TRUE;
}

gboolean g_attrib_cancel_all(GAttrib *attrib)
{
	gboolean ret;

	if (attrib == NULL)
		return FALSE;

        ALOCK(attrib);
	ret = cancel_all_per_queue(attrib, attrib->requests);
	ret = cancel_all_per_queue(attrib, attrib->responses) && ret;
        AUNLOCK(attrib);

	return ret;
}

uint8_t *g_attrib_get_buffer(GAttrib *attrib, size_t *len)
{
	if (len == NULL)
		return NULL;

	*len = attrib->buflen;

	return attrib->buf;
}

gboolean g_attrib_set_mtu(GAttrib *attrib, int mtu)
{
	if (mtu < ATT_DEFAULT_LE_MTU)
		return FALSE;

	attrib->buf = g_realloc(attrib->buf, mtu);

	attrib->buflen = mtu;

	return TRUE;
}

guint g_attrib_register(GAttrib *attrib, guint8 opcode, guint16 handle,
				GAttribNotifyFunc func, gpointer user_data,
				GDestroyNotify notify)
{
	static guint next_evt_id = 0;
	struct event *event;

	event = g_try_new0(struct event, 1);
	if (event == NULL)
		return 0;

	event->expected = opcode;
	event->handle = handle;
	event->func = func;
	event->user_data = user_data;
	event->notify = notify;
	event->id = ++next_evt_id;

	attrib->events = g_slist_append(attrib->events, event);

	return event->id;
}

static int event_cmp_by_id(gconstpointer a, gconstpointer b)
{
	const struct event *evt = a;
	guint id = GPOINTER_TO_UINT(b);

	return evt->id - id;
}

gboolean g_attrib_unregister(GAttrib *attrib, guint id)
{
	struct event *evt;
	GSList *l;

	if (id == 0) {
		warn("%s: invalid id", __func__);
		return FALSE;
	}

	l = g_slist_find_custom(attrib->events, GUINT_TO_POINTER(id),
							event_cmp_by_id);
	if (l == NULL)
		return FALSE;

	evt = l->data;

	attrib->events = g_slist_remove(attrib->events, evt);

	if (evt->notify)
		evt->notify(evt->user_data);

	g_free(evt);

	return TRUE;
}

gboolean g_attrib_unregister_all(GAttrib *attrib)
{
	GSList *l;

	if (attrib->events == NULL)
		return FALSE;

	for (l = attrib->events; l; l = l->next) {
		struct event *evt = l->data;

		if (evt->notify)
			evt->notify(evt->user_data);

		g_free(evt);
	}

	g_slist_free(attrib->events);
	attrib->events = NULL;

	return TRUE;
}
