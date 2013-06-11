/*
 *  gstvaapipostproc.c - VA-API video postprocessing
 *
 *  Copyright (C) 2012-2013 Intel Corporation
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

/**
 * SECTION:gstvaapipostproc
 * @short_description: A video postprocessing filter
 *
 * vaapipostproc consists in various postprocessing algorithms to be
 * applied to VA surfaces. So far, only basic bob deinterlacing is
 * implemented.
 */

#include "gst/vaapi/sysdeps.h"
#include <gst/video/video.h>
#if !GST_CHECK_VERSION(1,1,0)
#include <gst/video/videocontext.h>
#endif

#include "gstvaapipostproc.h"
#include "gstvaapipluginutil.h"
#include "gstvaapivideobuffer.h"

#define GST_PLUGIN_NAME "vaapipostproc"
#define GST_PLUGIN_DESC "A video postprocessing filter"

GST_DEBUG_CATEGORY_STATIC(gst_debug_vaapipostproc);
#define GST_CAT_DEFAULT gst_debug_vaapipostproc

/* Default templates */
static const char gst_vaapipostproc_sink_caps_str[] =
    GST_VAAPI_SURFACE_CAPS ", "
    GST_CAPS_INTERLACED_MODES;

static const char gst_vaapipostproc_src_caps_str[] =
    GST_VAAPI_SURFACE_CAPS ", "
    GST_CAPS_INTERLACED_FALSE;

static GstStaticPadTemplate gst_vaapipostproc_sink_factory =
    GST_STATIC_PAD_TEMPLATE(
        "sink",
        GST_PAD_SINK,
        GST_PAD_ALWAYS,
        GST_STATIC_CAPS(gst_vaapipostproc_sink_caps_str));

static GstStaticPadTemplate gst_vaapipostproc_src_factory =
    GST_STATIC_PAD_TEMPLATE(
        "src",
        GST_PAD_SRC,
        GST_PAD_ALWAYS,
        GST_STATIC_CAPS(gst_vaapipostproc_src_caps_str));

/* GstImplementsInterface interface */
#if !GST_CHECK_VERSION(1,0,0)
static gboolean
gst_vaapipostproc_implements_interface_supported(
    GstImplementsInterface *iface,
    GType                   type
)
{
    return (type == GST_TYPE_VIDEO_CONTEXT);
}

static void
gst_vaapipostproc_implements_iface_init(GstImplementsInterfaceClass *iface)
{
    iface->supported = gst_vaapipostproc_implements_interface_supported;
}
#endif

/* GstVideoContext interface */
#if !GST_CHECK_VERSION(1,1,0)
static void
gst_vaapipostproc_set_video_context(
    GstVideoContext *context,
    const gchar     *type,
    const GValue    *value
)
{
    GstVaapiPostproc * const postproc = GST_VAAPIPOSTPROC(context);

    gst_vaapi_set_display(type, value, &postproc->display);
}

static void
gst_video_context_interface_init(GstVideoContextInterface *iface)
{
    iface->set_context = gst_vaapipostproc_set_video_context;
}

#define GstVideoContextClass GstVideoContextInterface
#endif
G_DEFINE_TYPE_WITH_CODE(
    GstVaapiPostproc,
    gst_vaapipostproc,
    GST_TYPE_ELEMENT,
#if !GST_CHECK_VERSION(1,0,0)
    G_IMPLEMENT_INTERFACE(GST_TYPE_IMPLEMENTS_INTERFACE,
                          gst_vaapipostproc_implements_iface_init);
#endif
#if !GST_CHECK_VERSION(1,1,0)
    G_IMPLEMENT_INTERFACE(GST_TYPE_VIDEO_CONTEXT,
                          gst_video_context_interface_init)
#endif
    )

enum {
    PROP_0,

    PROP_DEINTERLACE_MODE,
    PROP_DEINTERLACE_METHOD,
};

#define DEFAULT_DEINTERLACE_MODE        GST_VAAPI_DEINTERLACE_MODE_AUTO
#define DEFAULT_DEINTERLACE_METHOD      GST_VAAPI_DEINTERLACE_METHOD_BOB

#define GST_VAAPI_TYPE_DEINTERLACE_MODE \
    gst_vaapi_deinterlace_mode_get_type()

static GType
gst_vaapi_deinterlace_mode_get_type(void)
{
    static GType deinterlace_mode_type = 0;

    static const GEnumValue mode_types[] = {
        { GST_VAAPI_DEINTERLACE_MODE_AUTO,
          "Auto detection", "auto" },
        { GST_VAAPI_DEINTERLACE_MODE_INTERLACED,
          "Force deinterlacing", "interlaced" },
        { GST_VAAPI_DEINTERLACE_MODE_DISABLED,
          "Never deinterlace", "disabled" },
        { 0, NULL, NULL },
    };

    if (!deinterlace_mode_type) {
        deinterlace_mode_type =
            g_enum_register_static("GstVaapiDeinterlaceMode", mode_types);
    }
    return deinterlace_mode_type;
}

#define GST_VAAPI_TYPE_DEINTERLACE_METHOD \
    gst_vaapi_deinterlace_method_get_type()

static GType
gst_vaapi_deinterlace_method_get_type(void)
{
    static GType deinterlace_method_type = 0;

    static const GEnumValue method_types[] = {
        { GST_VAAPI_DEINTERLACE_METHOD_BOB,
          "Bob deinterlacing", "bob" },
#if 0
        /* VA/VPP */
        { GST_VAAPI_DEINTERLACE_METHOD_WEAVE,
          "Weave deinterlacing", "weave" },
        { GST_VAAPI_DEINTERLACE_METHOD_MOTION_ADAPTIVE,
          "Motion adaptive deinterlacing", "motion-adaptive" },
        { GST_VAAPI_DEINTERLACE_METHOD_MOTION_COMPENSATED,
          "Motion compensated deinterlacing", "motion-compensated" },
#endif
        { 0, NULL, NULL },
    };

    if (!deinterlace_method_type) {
        deinterlace_method_type =
            g_enum_register_static("GstVaapiDeinterlaceMethod", method_types);
    }
    return deinterlace_method_type;
}

static inline GstVaapiPostproc *
get_vaapipostproc_from_pad(GstPad *pad)
{
    return GST_VAAPIPOSTPROC(gst_pad_get_parent_element(pad));
}

static inline gboolean
gst_vaapipostproc_ensure_display(GstVaapiPostproc *postproc)
{
#if GST_CHECK_VERSION(1,1,0)
    GST_OBJECT_LOCK(postproc);
    if (postproc->set_display) {
        GstContext *context;

        postproc->display = gst_vaapi_display_ref(postproc->set_display);
        GST_OBJECT_UNLOCK(postproc);
        context = gst_element_get_context(GST_ELEMENT_CAST(postproc));
        if (!context)
            context = gst_context_new();
        context = gst_context_make_writable(context);
        gst_context_set_vaapi_display(context, postproc->display);
        gst_element_set_context(GST_ELEMENT_CAST(postproc), context);
        gst_context_unref(context);
        return TRUE;
    }
    GST_OBJECT_UNLOCK(postproc);
#endif

    return gst_vaapi_ensure_display(postproc, GST_VAAPI_DISPLAY_TYPE_ANY,
        &postproc->display);
}

static gboolean
gst_vaapipostproc_create(GstVaapiPostproc *postproc, GstCaps *caps)
{
    if (!gst_vaapipostproc_ensure_display(postproc))
        return FALSE;

    gst_caps_replace(&postproc->postproc_caps, caps);
    return TRUE;
}

static void
gst_vaapipostproc_destroy(GstVaapiPostproc *postproc)
{
    gst_caps_replace(&postproc->postproc_caps, NULL);

    gst_vaapi_display_replace(&postproc->display, NULL);
}

static gboolean
gst_vaapipostproc_reset(GstVaapiPostproc *postproc, GstCaps *caps)
{
    if (postproc->postproc_caps &&
        gst_caps_is_always_compatible(caps, postproc->postproc_caps))
        return TRUE;

    gst_vaapipostproc_destroy(postproc);
    return gst_vaapipostproc_create(postproc, caps);
}

static gboolean
gst_vaapipostproc_start(GstVaapiPostproc *postproc)
{
    if (!gst_vaapipostproc_ensure_display(postproc))
        return FALSE;
    return TRUE;
}

static gboolean
gst_vaapipostproc_stop(GstVaapiPostproc *postproc)
{
    gst_vaapi_display_replace(&postproc->display, NULL);
    return TRUE;
}

static GstFlowReturn
gst_vaapipostproc_process(GstVaapiPostproc *postproc, GstBuffer *buf)
{
    GstVaapiVideoMeta *meta;
    GstVaapiSurfaceProxy *proxy;
    GstClockTime timestamp;
    GstFlowReturn ret;
    GstBuffer *outbuf;
    guint outbuf_flags, flags;
    gboolean tff;

    meta = gst_buffer_get_vaapi_video_meta(buf);
    if (!meta)
        goto error_invalid_buffer;

    /* Deinterlacing disabled, push frame */
    if (!postproc->deinterlace) {
        outbuf = gst_buffer_ref(buf);
        if (!outbuf)
            goto error_create_buffer;
        ret = gst_pad_push(postproc->srcpad, outbuf);
        if (ret != GST_FLOW_OK)
            goto error_push_buffer;
        gst_buffer_unref(buf);
        return GST_FLOW_OK;
    }

    timestamp  = GST_BUFFER_TIMESTAMP(buf);
    proxy      = gst_vaapi_video_meta_get_surface_proxy(meta);
    tff        = GST_BUFFER_FLAG_IS_SET(buf, GST_VIDEO_BUFFER_FLAG_TFF);

    flags = gst_vaapi_video_meta_get_render_flags(meta) &
        ~(GST_VAAPI_PICTURE_STRUCTURE_TOP_FIELD|
          GST_VAAPI_PICTURE_STRUCTURE_BOTTOM_FIELD);

    /* First field */
    outbuf = gst_vaapi_video_buffer_new_with_surface_proxy(proxy);
    if (!outbuf)
        goto error_create_buffer;

    meta = gst_buffer_get_vaapi_video_meta(outbuf);
    outbuf_flags = flags;
    outbuf_flags |= postproc->deinterlace ? (
        tff ?
        GST_VAAPI_PICTURE_STRUCTURE_TOP_FIELD :
        GST_VAAPI_PICTURE_STRUCTURE_BOTTOM_FIELD) :
        GST_VAAPI_PICTURE_STRUCTURE_FRAME;
    gst_vaapi_video_meta_set_render_flags(meta, outbuf_flags);

    GST_BUFFER_TIMESTAMP(outbuf) = timestamp;
    GST_BUFFER_DURATION(outbuf)  = postproc->field_duration;
#if !GST_CHECK_VERSION(1,0,0)
    gst_buffer_set_caps(outbuf, postproc->srcpad_caps);
#endif
    ret = gst_pad_push(postproc->srcpad, outbuf);
    if (ret != GST_FLOW_OK)
        goto error_push_buffer;

    /* Second field */
    outbuf = gst_vaapi_video_buffer_new_with_surface_proxy(proxy);
    if (!outbuf)
        goto error_create_buffer;

    meta = gst_buffer_get_vaapi_video_meta(outbuf);
    outbuf_flags = flags;
    outbuf_flags |= postproc->deinterlace ? (
        tff ?
        GST_VAAPI_PICTURE_STRUCTURE_BOTTOM_FIELD :
        GST_VAAPI_PICTURE_STRUCTURE_TOP_FIELD) :
        GST_VAAPI_PICTURE_STRUCTURE_FRAME;
    gst_vaapi_video_meta_set_render_flags(meta, outbuf_flags);

    GST_BUFFER_TIMESTAMP(outbuf) = timestamp + postproc->field_duration;
    GST_BUFFER_DURATION(outbuf)  = postproc->field_duration;
#if !GST_CHECK_VERSION(1,0,0)
    gst_buffer_set_caps(outbuf, postproc->srcpad_caps);
#endif
    ret = gst_pad_push(postproc->srcpad, outbuf);
    if (ret != GST_FLOW_OK)
        goto error_push_buffer;

    gst_buffer_unref(buf);
    return GST_FLOW_OK;

    /* ERRORS */
error_invalid_buffer:
    {
        GST_ERROR("failed to receive a valid video buffer");
        gst_buffer_unref(buf);
        return GST_FLOW_EOS;
    }
error_create_buffer:
    {
        GST_ERROR("failed to create output buffer");
        gst_buffer_unref(buf);
        return GST_FLOW_EOS;
    }
error_push_buffer:
    {
        if (ret != GST_FLOW_FLUSHING)
            GST_ERROR("failed to push output buffer to video sink");
        gst_buffer_unref(buf);
        return GST_FLOW_EOS;
    }
}

static gboolean
gst_vaapipostproc_update_sink_caps(GstVaapiPostproc *postproc, GstCaps *caps)
{
    GstVideoInfo vi;

    if (!gst_video_info_from_caps(&vi, caps))
        return FALSE;
    postproc->fps_n = GST_VIDEO_INFO_FPS_N(&vi);
    postproc->fps_d = GST_VIDEO_INFO_FPS_D(&vi);

    switch (postproc->deinterlace_mode) {
    case GST_VAAPI_DEINTERLACE_MODE_AUTO:
        postproc->deinterlace = GST_VIDEO_INFO_IS_INTERLACED(&vi);
        break;
    case GST_VAAPI_DEINTERLACE_MODE_INTERLACED:
        postproc->deinterlace = TRUE;
        break;
    case GST_VAAPI_DEINTERLACE_MODE_DISABLED:
        postproc->deinterlace = FALSE;
        break;
    }

    postproc->field_duration = gst_util_uint64_scale(
        GST_SECOND,
        postproc->fps_d,
        (1 + postproc->deinterlace) * postproc->fps_n
    );

    gst_caps_replace(&postproc->sinkpad_caps, caps);
    return TRUE;
}

static gboolean
gst_vaapipostproc_update_src_caps(GstVaapiPostproc *postproc, GstCaps *caps)
{
    GstCaps *src_caps;
    GstStructure *structure;
    const GValue *v_width, *v_height, *v_par;
    gint fps_n, fps_d;

    if (postproc->srcpad_caps)
        src_caps = gst_caps_make_writable(postproc->srcpad_caps);
    else
        src_caps = gst_caps_from_string(GST_VAAPI_SURFACE_CAPS_NAME);
    if (!src_caps)
        return FALSE;
    postproc->srcpad_caps = src_caps;

    structure    = gst_caps_get_structure(caps, 0);
    v_width      = gst_structure_get_value(structure, "width");
    v_height     = gst_structure_get_value(structure, "height");
    v_par        = gst_structure_get_value(structure, "pixel-aspect-ratio");

    structure = gst_caps_get_structure(src_caps, 0);
    if (v_width && v_height) {
        gst_structure_set_value(structure, "width", v_width);
        gst_structure_set_value(structure, "height", v_height);
    }
    if (v_par)
        gst_structure_set_value(structure, "pixel-aspect-ratio", v_par);

    gst_structure_set(structure, "type", G_TYPE_STRING, "vaapi", NULL);
    gst_structure_set(structure, "opengl", G_TYPE_BOOLEAN, USE_GLX, NULL);

    if (!postproc->deinterlace)
        gst_structure_remove_interlaced_field(structure);
    else {
        /* Set double framerate in interlaced mode */
        if (!gst_util_fraction_multiply(postproc->fps_n, postproc->fps_d,
                                        2, 1,
                                        &fps_n, &fps_d))
            return FALSE;

        gst_structure_set(structure, "framerate",
            GST_TYPE_FRACTION, fps_n, fps_d, NULL);
        gst_structure_set_interlaced(structure, FALSE);
    }
    return gst_pad_set_caps(postproc->srcpad, src_caps);
}

static gboolean
gst_vaapipostproc_ensure_allowed_caps(GstVaapiPostproc *postproc)
{
    if (postproc->allowed_caps)
        return TRUE;

    postproc->allowed_caps =
        gst_caps_from_string(gst_vaapipostproc_sink_caps_str);
    if (!postproc->allowed_caps)
        return FALSE;

    /* XXX: append VA/VPP filters */
    return TRUE;
}

static GstCaps *
gst_vaapipostproc_get_caps(GstPad *pad)
{
    GstVaapiPostproc * const postproc = get_vaapipostproc_from_pad(pad);
    GstCaps *out_caps;

    if (gst_vaapipostproc_ensure_allowed_caps(postproc))
        out_caps = gst_caps_ref(postproc->allowed_caps);
    else
        out_caps = gst_caps_new_empty();

    gst_object_unref(postproc);
    return out_caps;
}

static gboolean
gst_vaapipostproc_set_caps(GstPad *pad, GstCaps *caps)
{
    GstVaapiPostproc * const postproc = get_vaapipostproc_from_pad(pad);
    gboolean success = FALSE;

    g_return_val_if_fail(pad == postproc->sinkpad, FALSE);

    do {
        if (!gst_vaapipostproc_update_sink_caps(postproc, caps))
            break;
        if (!gst_vaapipostproc_update_src_caps(postproc, caps))
            break;
        if (!gst_vaapipostproc_reset(postproc, postproc->sinkpad_caps))
            break;
        success = TRUE;
    } while (0);
    gst_object_unref(postproc);
    return success;
}

static GstFlowReturn
gst_vaapipostproc_chain(GST_PAD_CHAIN_FUNCTION_ARGS)
{
    GstVaapiPostproc * const postproc = get_vaapipostproc_from_pad(pad);
    GstFlowReturn ret;

    ret = gst_vaapipostproc_process(postproc, buffer);
    gst_object_unref(postproc);
    return ret;
}

static gboolean
gst_vaapipostproc_sink_event(GST_PAD_EVENT_FUNCTION_ARGS)
{
    GstVaapiPostproc * const postproc = get_vaapipostproc_from_pad(pad);
    gboolean success;

    GST_DEBUG("handle sink event '%s'", GST_EVENT_TYPE_NAME(event));

    switch (GST_EVENT_TYPE(event)) {
#if GST_CHECK_VERSION(1,0,0)
    case GST_EVENT_CAPS: {
        GstCaps *caps;
        gst_event_parse_caps(event, &caps);
        success = gst_vaapipostproc_set_caps(pad, caps);
        break;
    }
#endif
    default:
        /* Propagate event downstream */
        success = gst_pad_push_event(postproc->srcpad, event);
        break;
    }
    gst_object_unref(postproc);
    return success;
}

static gboolean
gst_vaapipostproc_src_event(GST_PAD_EVENT_FUNCTION_ARGS)
{
    GstVaapiPostproc * const postproc = get_vaapipostproc_from_pad(pad);
    gboolean success;

    GST_DEBUG("handle src event '%s'", GST_EVENT_TYPE_NAME(event));

    /* Propagate event upstream */
    success = gst_pad_push_event(postproc->sinkpad, event);
    gst_object_unref(postproc);
    return success;
}

#if GST_CHECK_VERSION(1,1,0)
static void
gst_vaapipostproc_set_context(GstElement *element, GstContext *context)
{
    GstVaapiPostproc * const postproc = GST_VAAPIPOSTPROC(element);
    GstVaapiDisplay *display = NULL;

    if (gst_context_get_vaapi_display(context, &display)) {
        GST_OBJECT_LOCK(postproc);
        if (postproc->set_display)
            gst_vaapi_display_unref(postproc->set_display);
        postproc->set_display = display;
        GST_OBJECT_UNLOCK(postproc);
    }

    GST_OBJECT_LOCK(postproc);
    context = gst_context_copy(context);
    gst_context_set_vaapi_display(context, postproc->display);
    GST_OBJECT_UNLOCK(postproc);

    GST_ELEMENT_CLASS(gst_vaapipostproc_parent_class)->set_context(element,
        context);
    gst_context_unref (context);
}
#endif

static gboolean
gst_vaapipostproc_query(GST_PAD_QUERY_FUNCTION_ARGS)
{
    GstVaapiPostproc * const postproc = get_vaapipostproc_from_pad(pad);
    gboolean success;

    GST_DEBUG("sharing display %p", postproc->display);

    if (gst_vaapi_reply_to_query(query, postproc->display))
        success = TRUE;
#if GST_CHECK_VERSION(1,0,0)
    else if (GST_PAD_IS_SINK(pad) && GST_QUERY_TYPE(query) == GST_QUERY_CAPS) {
        GstCaps * const caps = gst_vaapipostproc_get_caps(pad);
        gst_query_set_caps_result(query, caps);
        gst_caps_unref(caps);
        success = TRUE;
    }
#endif
    else
        success = GST_PAD_QUERY_FUNCTION_CALL(gst_pad_query_default, pad,
            parent, query);

    gst_object_unref(postproc);
    return success;
}

static void
gst_vaapipostproc_finalize(GObject *object)
{
    GstVaapiPostproc * const postproc = GST_VAAPIPOSTPROC(object);

    gst_vaapipostproc_destroy(postproc);

    gst_caps_replace(&postproc->sinkpad_caps, NULL);
    gst_caps_replace(&postproc->srcpad_caps,  NULL);
    gst_caps_replace(&postproc->allowed_caps, NULL);

    G_OBJECT_CLASS(gst_vaapipostproc_parent_class)->finalize(object);
}

static void
gst_vaapipostproc_set_property(
    GObject      *object,
    guint         prop_id,
    const GValue *value,
    GParamSpec   *pspec
)
{
    GstVaapiPostproc * const postproc = GST_VAAPIPOSTPROC(object);

    switch (prop_id) {
    case PROP_DEINTERLACE_MODE:
        postproc->deinterlace_mode = g_value_get_enum(value);
        break;
    case PROP_DEINTERLACE_METHOD:
        postproc->deinterlace_method = g_value_get_enum(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gst_vaapipostproc_get_property(
    GObject    *object,
    guint       prop_id,
    GValue     *value,
    GParamSpec *pspec
)
{
    GstVaapiPostproc * const postproc = GST_VAAPIPOSTPROC(object);

    switch (prop_id) {
    case PROP_DEINTERLACE_MODE:
        g_value_set_enum(value, postproc->deinterlace_mode);
        break;
    case PROP_DEINTERLACE_METHOD:
        g_value_set_enum(value, postproc->deinterlace_method);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static GstStateChangeReturn
gst_vaapipostproc_change_state(GstElement *element, GstStateChange transition)
{
    GstVaapiPostproc * const postproc = GST_VAAPIPOSTPROC(element);
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
        if (!gst_vaapipostproc_start(postproc))
            return GST_STATE_CHANGE_FAILURE;
        break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
        break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
        break;
    default:
        break;
    }

    ret = GST_ELEMENT_CLASS(gst_vaapipostproc_parent_class)->change_state(element, transition);
    if (ret != GST_STATE_CHANGE_SUCCESS)
        return ret;

    switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
        break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
        break;
    case GST_STATE_CHANGE_READY_TO_NULL:
        if (!gst_vaapipostproc_stop(postproc))
            return GST_STATE_CHANGE_FAILURE;
        break;
    default:
        break;
    }
    return GST_STATE_CHANGE_SUCCESS;
}

static void
gst_vaapipostproc_class_init(GstVaapiPostprocClass *klass)
{
    GObjectClass * const object_class = G_OBJECT_CLASS(klass);
    GstElementClass * const element_class = GST_ELEMENT_CLASS(klass);
    GstPadTemplate *pad_template;

    GST_DEBUG_CATEGORY_INIT(gst_debug_vaapipostproc,
                            GST_PLUGIN_NAME, 0, GST_PLUGIN_DESC);

    object_class->finalize      = gst_vaapipostproc_finalize;
    object_class->set_property  = gst_vaapipostproc_set_property;
    object_class->get_property  = gst_vaapipostproc_get_property;

    element_class->change_state = gst_vaapipostproc_change_state;

#if GST_CHECK_VERSION(1,1,0)
    element_class->set_context  = gst_vaapipostproc_set_context;
#endif

    gst_element_class_set_static_metadata(element_class,
        "VA-API video postprocessing",
        "Filter/Converter/Video",
        GST_PLUGIN_DESC,
        "Gwenole Beauchesne <gwenole.beauchesne@intel.com>");

    /* sink pad */
    pad_template = gst_static_pad_template_get(&gst_vaapipostproc_sink_factory);
    gst_element_class_add_pad_template(element_class, pad_template);

    /* src pad */
    pad_template = gst_static_pad_template_get(&gst_vaapipostproc_src_factory);
    gst_element_class_add_pad_template(element_class, pad_template);

    /**
     * GstVaapiPostproc:deinterlace-mode:
     *
     * This selects whether the deinterlacing should always be applied or if
     * they should only be applied on content that has the "interlaced" flag
     * on the caps.
     */
    g_object_class_install_property
        (object_class,
         PROP_DEINTERLACE_MODE,
         g_param_spec_enum("deinterlace",
                           "Deinterlace",
                           "Deinterlace mode to use",
                           GST_VAAPI_TYPE_DEINTERLACE_MODE,
                           DEFAULT_DEINTERLACE_MODE,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    /**
     * GstVaapiPostproc:deinterlace-method:
     *
     * This selects the deinterlacing method to apply.
     */
    g_object_class_install_property
        (object_class,
         PROP_DEINTERLACE_METHOD,
         g_param_spec_enum("deinterlace-method",
                           "Deinterlace method",
                           "Deinterlace method to use",
                           GST_VAAPI_TYPE_DEINTERLACE_METHOD,
                           DEFAULT_DEINTERLACE_METHOD,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_vaapipostproc_init(GstVaapiPostproc *postproc)
{
    GstVaapiPostprocClass *klass = GST_VAAPIPOSTPROC_GET_CLASS(postproc);
    GstElementClass * const element_class = GST_ELEMENT_CLASS(klass);

    postproc->allowed_caps              = NULL;
    postproc->postproc_caps             = NULL;
    postproc->display                   = NULL;
    postproc->surface_width             = 0;
    postproc->surface_height            = 0;
    postproc->deinterlace               = FALSE;
    postproc->deinterlace_mode          = DEFAULT_DEINTERLACE_MODE;
    postproc->deinterlace_method        = DEFAULT_DEINTERLACE_METHOD;
    postproc->field_duration            = GST_CLOCK_TIME_NONE;
    postproc->fps_n                     = 0;
    postproc->fps_d                     = 0;

    /* Pad through which data comes in to the element */
    postproc->sinkpad = gst_pad_new_from_template(
        gst_element_class_get_pad_template(element_class, "sink"),
        "sink"
    );
    postproc->sinkpad_caps = NULL;

#if !GST_CHECK_VERSION(1,0,0)
    gst_pad_set_getcaps_function(postproc->sinkpad, gst_vaapipostproc_get_caps);
    gst_pad_set_setcaps_function(postproc->sinkpad, gst_vaapipostproc_set_caps);
#endif
    gst_pad_set_chain_function(postproc->sinkpad, gst_vaapipostproc_chain);
    gst_pad_set_event_function(postproc->sinkpad, gst_vaapipostproc_sink_event);
    gst_pad_set_query_function(postproc->sinkpad, gst_vaapipostproc_query);
    gst_element_add_pad(GST_ELEMENT(postproc), postproc->sinkpad);

    /* Pad through which data goes out of the element */
    postproc->srcpad = gst_pad_new_from_template(
        gst_element_class_get_pad_template(element_class, "src"),
        "src"
    );
    postproc->srcpad_caps = NULL;

    gst_pad_set_event_function(postproc->srcpad, gst_vaapipostproc_src_event);
    gst_pad_set_query_function(postproc->srcpad, gst_vaapipostproc_query);
    gst_element_add_pad(GST_ELEMENT(postproc), postproc->srcpad);
}
