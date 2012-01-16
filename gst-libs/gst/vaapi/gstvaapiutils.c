/*
 *  gstvaapiutils.c - VA-API utilities
 *
 *  Copyright (C) 2010-2011 Splitted-Desktop Systems
 *  Copyright (C) 2011-2012 Intel Corporation
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

#include "config.h"
#include "gstvaapicompat.h"
#include "gstvaapiutils.h"
#include "gstvaapisurface.h"
#include <stdio.h>
#include <stdarg.h>

#define DEBUG 1
#include "gstvaapidebug.h"

#define CONCAT(a, b)    CONCAT_(a, b)
#define CONCAT_(a, b)   a##b
#define STRINGIFY(x)    STRINGIFY_(x)
#define STRINGIFY_(x)   #x
#define STRCASEP(p, x)  STRCASE(CONCAT(p, x))
#define STRCASE(x)      case x: return STRINGIFY(x)

/* Check VA status for success or print out an error */
gboolean
vaapi_check_status(VAStatus status, const char *msg)
{
    if (status != VA_STATUS_SUCCESS) {
        GST_DEBUG("%s: %s", msg, vaErrorStr(status));
        return FALSE;
    }
    return TRUE;
}

/* Maps VA buffer */
void *
vaapi_map_buffer(VADisplay dpy, VABufferID buf_id)
{
    VAStatus status;
    void *data = NULL;

    status = vaMapBuffer(dpy, buf_id, &data);
    if (!vaapi_check_status(status, "vaMapBuffer()"))
        return NULL;
    return data;
}

/* Unmaps VA buffer */
void
vaapi_unmap_buffer(VADisplay dpy, VABufferID buf_id, void **pbuf)
{
    VAStatus status;

    if (pbuf)
        *pbuf = NULL;

    status = vaUnmapBuffer(dpy, buf_id);
    if (!vaapi_check_status(status, "vaUnmapBuffer()"))
        return;
}

/* Creates and maps VA buffer */
void *
vaapi_create_buffer(
    VADisplay    dpy,
    VAContextID  ctx,
    int          type,
    unsigned int size,
    VABufferID  *buf_id_ptr
)
{
    VABufferID buf_id;
    VAStatus status;
    void *data;

    status = vaCreateBuffer(dpy, ctx, type, size, 1, NULL, &buf_id);
    if (!vaapi_check_status(status, "vaCreateBuffer()"))
        return NULL;

    data = vaapi_map_buffer(dpy, buf_id);
    if (!data)
        goto error;

    *buf_id_ptr = buf_id;
    return data;

error:
    vaapi_destroy_buffer(dpy, &buf_id);
    return NULL;
}

/* Destroy VA buffer */
void
vaapi_destroy_buffer(VADisplay dpy, VABufferID *buf_id_ptr)
{
    if (!buf_id_ptr || *buf_id_ptr == VA_INVALID_ID)
        return;

    vaDestroyBuffer(dpy, *buf_id_ptr);
    *buf_id_ptr = VA_INVALID_ID;
}

/* Return a string representation of a VAProfile */
const char *string_of_VAProfile(VAProfile profile)
{
    switch (profile) {
#define MAP(profile) \
        STRCASEP(VAProfile, profile)
        MAP(MPEG2Simple);
        MAP(MPEG2Main);
        MAP(MPEG4Simple);
        MAP(MPEG4AdvancedSimple);
        MAP(MPEG4Main);
#if VA_CHECK_VERSION(0,32,0)
        MAP(JPEGBaseline);
        MAP(H263Baseline);
        MAP(H264ConstrainedBaseline);
#endif
        MAP(H264Baseline);
        MAP(H264Main);
        MAP(H264High);
        MAP(VC1Simple);
        MAP(VC1Main);
        MAP(VC1Advanced);
#undef MAP
    default: break;
    }
    return "<unknown>";
}

/* Return a string representation of a VAEntrypoint */
const char *string_of_VAEntrypoint(VAEntrypoint entrypoint)
{
    switch (entrypoint) {
#define MAP(entrypoint) \
        STRCASEP(VAEntrypoint, entrypoint)
        MAP(VLD);
        MAP(IZZ);
        MAP(IDCT);
        MAP(MoComp);
        MAP(Deblocking);
#undef MAP
    default: break;
    }
    return "<unknown>";
}

/**
 * from_GstVaapiSurfaceRenderFlags:
 * @flags: the #GstVaapiSurfaceRenderFlags
 *
 * Converts #GstVaapiSurfaceRenderFlags to flags suitable for
 * vaPutSurface().
 */
guint
from_GstVaapiSurfaceRenderFlags(guint flags)
{
    guint va_fields = 0, va_csc = 0;

    if (flags & GST_VAAPI_PICTURE_STRUCTURE_TOP_FIELD)
        va_fields |= VA_TOP_FIELD;
    if (flags & GST_VAAPI_PICTURE_STRUCTURE_BOTTOM_FIELD)
        va_fields |= VA_BOTTOM_FIELD;
    if ((va_fields ^ (VA_TOP_FIELD|VA_BOTTOM_FIELD)) == 0)
        va_fields  = VA_FRAME_PICTURE;

#ifdef VA_SRC_BT601
    if (flags & GST_VAAPI_COLOR_STANDARD_ITUR_BT_601)
        va_csc = VA_SRC_BT601;
#endif
#ifdef VA_SRC_BT709
    if (flags & GST_VAAPI_COLOR_STANDARD_ITUR_BT_709)
        va_csc = VA_SRC_BT709;
#endif

    return va_fields|va_csc;
}

/**
 * to_GstVaapiSurfaceStatus:
 * @flags: the #GstVaapiSurfaceStatus flags to translate
 *
 * Converts vaQuerySurfaceStatus() @flags to #GstVaapiSurfaceStatus
 * flags.
 *
 * Return value: the #GstVaapiSurfaceStatus flags
 */
guint
to_GstVaapiSurfaceStatus(guint va_flags)
{
    guint flags;
    const guint va_flags_mask = (VASurfaceReady|
                                 VASurfaceRendering|
                                 VASurfaceDisplaying);

    /* Check for core status */
    switch (va_flags & va_flags_mask) {
    case VASurfaceReady:
        flags = GST_VAAPI_SURFACE_STATUS_IDLE;
        break;
    case VASurfaceRendering:
        flags = GST_VAAPI_SURFACE_STATUS_RENDERING;
        break;
    case VASurfaceDisplaying:
        flags = GST_VAAPI_SURFACE_STATUS_DISPLAYING;
        break;
    default:
        flags = 0;
        break;
    }

    /* Check for encoder status */
#if VA_CHECK_VERSION(0,30,0)
    if (va_flags & VASurfaceSkipped)
        flags |= GST_VAAPI_SURFACE_STATUS_SKIPPED;
#endif
    return flags;
}
