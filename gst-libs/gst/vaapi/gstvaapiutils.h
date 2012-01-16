/*
 *  gstvaapiutils.h - VA-API utilities
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

#ifndef GST_VAAPI_UTILS_H
#define GST_VAAPI_UTILS_H

#include "config.h"
#include <glib/gtypes.h>

#ifdef GST_VAAPI_USE_OLD_VAAPI_0_29
# include <va.h>
#else
# include <va/va.h>
#endif

/** Check VA status for success or print out an error */
gboolean
vaapi_check_status(VAStatus status, const char *msg)
    attribute_hidden;

/** Maps VA buffer */
void *
vaapi_map_buffer(VADisplay dpy, VABufferID buf_id)
    attribute_hidden;

/** Unmaps VA buffer */
void
vaapi_unmap_buffer(VADisplay dpy, VABufferID buf_id, void **pbuf)
    attribute_hidden;

/** Creates and maps VA buffer */
void *
vaapi_create_buffer(
    VADisplay    dpy,
    VAContextID  ctx,
    int          type,
    unsigned int size,
    VABufferID  *buf_id
) attribute_hidden;

/** Destroy VA buffer */
void
vaapi_destroy_buffer(VADisplay dpy, VABufferID *buf_id)
    attribute_hidden;

/** Return a string representation of a VAProfile */
const char *string_of_VAProfile(VAProfile profile)
    attribute_hidden;

/** Return a string representation of a VAEntrypoint */
const char *string_of_VAEntrypoint(VAEntrypoint entrypoint)
    attribute_hidden;

guint
from_GstVaapiSurfaceRenderFlags(guint flags)
    attribute_hidden;

guint
to_GstVaapiSurfaceStatus(guint va_flags)
    attribute_hidden;

#endif /* GST_VAAPI_UTILS_H */
