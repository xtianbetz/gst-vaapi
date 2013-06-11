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

#ifndef __GST_VIDEO_CONTEXT_H__
#define __GST_VIDEO_CONTEXT_H__

#if GST_CHECK_VERSION(1,1,0)

#include <gst/gst.h>
#include "gstvaapidisplay.h"

G_BEGIN_DECLS

#define GST_VIDEO_CONTEXT(obj) (GST_ELEMENT(obj))
#define GST_IS_VIDEO_CONTEXT(obj) (GST_IS_ELEMENT(obj))
#define GstVideoContext GstElement
#define gst_video_context_prepare gst_vaapi_video_context_prepare

void
gst_vaapi_video_context_prepare(GstElement *element, const gchar **types);

void
gst_vaapi_video_context_propagate(GstElement *element, GstVaapiDisplay *display);

G_END_DECLS

#endif /* GST_CHECK_VERSION(1,1,0) */

#endif /* __GST_VIDEO_CONTEXT_H__ */