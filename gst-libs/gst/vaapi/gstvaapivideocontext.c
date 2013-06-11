/*
 * gstvaapivideocontext.h - Compatibility glue for GstVideoContext
 *
 * Copyright (C) 2011 Collabora Ltd.
 *   Author: Nicolas Dufresne <nicolas.dufresne@collabora.co.uk>
 * Copyright (C) 2013 Igalia
 *   Author: Víctor Jáquez <vjaquez@igalia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "sysdeps.h"
#include "gstvaapivideocontext.h"

#if GST_CHECK_VERSION(1,1,0)

#include "gstvaapidisplay.h"

#define DEBUG 1
#include "gstvaapidebug.h"

static gboolean
context_pad_query(
    const GValue *item,
    GValue *value,
    gpointer user_data)
{
    GstPad *pad = g_value_get_object(item);
    GstQuery *query = user_data;
    gboolean res;

    res = gst_pad_peer_query(pad, query);

    if (res) {
        g_value_set_boolean(value, TRUE);
        return FALSE;
    }

    return TRUE;
}

static gboolean
run_context_query(
    GstElement *element,
    GstQuery *query)
{
    GstIterator *it;
    GstIteratorFoldFunction func = context_pad_query;
    GValue res = { 0 };

    g_value_init(&res, G_TYPE_BOOLEAN);
    g_value_set_boolean(&res, FALSE);

    /* Ask downstream neighbor */
    it = gst_element_iterate_src_pads(element);

    while (gst_iterator_fold(it, func, &res, query) == GST_ITERATOR_RESYNC)
        gst_iterator_resync(it);

    gst_iterator_free(it);

    /* ignore upstream neighbors */
    return g_value_get_boolean(&res);
}

void
gst_vaapi_video_context_prepare(
    GstElement *element,
    const gchar **types)
{
    GstContext *ctxt;
    GstQuery *query;

    /*  1) Check if the element already has a context of the specific
     *     type, i.e. by checking the context returned by
     *     gst_element_get_context()
     */
    if ((ctxt = gst_element_get_context(element))) {
        char *name = gst_element_get_name(element);
        g_print("%s: already have a context: %p\n", name, ctxt);
        g_free(name);
        gst_context_unref(ctxt);
        return;
    }

    /*  2) Query downstream with GST_QUERY_CONTEXT for the context and
     *     check if downstream already has a context of the specific
     *     type
     */
    ctxt = NULL;
    query = gst_query_new_context();
    gst_query_add_context_type(query, GST_VAAPI_DISPLAY_CONTEXT_TYPE);
    if (run_context_query(element, query)) {
        char *name = gst_element_get_name(element);
        g_print("%s: found context in query: %p\n", name, ctxt);
        g_free(name);
        gst_query_parse_context(query, &ctxt);
        gst_element_set_context(element, ctxt);
    } else {
        /* 3) Post a GST_MESSAGE_NEED_CONTEXT message on the bus with
         *    the required context types and afterwards check if a
         *    usable context was set now as in 1). The message could
         *    be handled by the parent bins of the element and the
         *    application.
         */
        GstMessage *msg;

        char *name = gst_element_get_name(element);
        g_print("%s: posting context need\n", name);
        g_free(name);
        msg = gst_message_new_need_context(GST_OBJECT_CAST(element));
        gst_message_add_context_type(msg, GST_VAAPI_DISPLAY_CONTEXT_TYPE);
        gst_element_post_message(element, msg);
    }

    gst_query_unref(query);
}

void context_event(
    const GValue *item,
    gpointer user_data)
{
    GstPad *pad = gst_pad_get_peer(g_value_get_object(item));
    if (!pad) {
        GST_INFO_OBJECT (pad, "pad has no peer");
        return;
    }

    GstEvent *event = gst_event_ref(user_data);
    gst_pad_send_event(pad, event);

    gst_object_unref(pad);
}

/*  4) Create a context by itself and post a GST_MESSAGE_HAVE_CONTEXT
 *     message and send a GST_EVENT_CONTEXT event downstream,
 *     containing the complete context information at this time.
 */
void
gst_vaapi_video_context_propagate(
    GstElement *element,
    GstVaapiDisplay *display)
{
    GstContext *context;
    GstMessage *msg;

    if (!display) {
        GST_ERROR_OBJECT(element, "Could not get VAAPI display connection");
        return;
    }

    context = gst_context_new();
    gst_context_set_vaapi_display(context, display);

    msg = gst_message_new_have_context(GST_OBJECT_CAST(element), context);
    gst_element_post_message(GST_ELEMENT_CAST(element), msg);

    context = gst_element_get_context(GST_ELEMENT_CAST(element));
    if (!context)
        context = gst_context_new();
    context = gst_context_make_writable (context);
    gst_context_set_vaapi_display(context, display);
    gst_element_set_context(GST_ELEMENT_CAST(element), context);

    GstEvent *event = gst_event_new_context(context);
    GstIterator *it = gst_element_iterate_src_pads(element);
    gst_iterator_foreach(it, context_event, event);
    gst_iterator_free(it);
    gst_event_unref(event);
}

#endif
