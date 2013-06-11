/*
 *  gstvaapivideobuffer.c - Gstreamer/VA video buffer
 *
 *  Copyright (C) 2010-2011 Splitted-Desktop Systems
 *  Copyright (C) 2011-2013 Intel Corporation
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
 * SECTION:gstvaapivideobuffer
 * @short_description: VA video buffer for GStreamer
 */

#include "gst/vaapi/sysdeps.h"
#include "gstvaapivideobuffer.h"
#if USE_GLX && !GST_CHECK_VERSION(1,1,0)
# include "gstvaapivideoconverter_glx.h"
#endif

#if GST_CHECK_VERSION(1,1,0)

#if USE_GLX
#include <GL/gl.h>
#include <gst/vaapi/gstvaapitexture.h>

static GstVaapiTexture *texture = NULL;

static void
gst_vaapi_texure_upload_free(gpointer data)
{
    GstVaapiTexture *texture = data;

    if (texture)
        gst_vaapi_texture_replace(&texture, NULL);
}

static gboolean
gst_vaapi_texture_upload(GstVideoGLTextureUploadMeta *meta, guint texture_id[4])
{
    GstVaapiVideoMeta * const vvmeta =
        gst_buffer_get_vaapi_video_meta(meta->buffer);
    GstVaapiTexture *texture = (GstVaapiTexture *) meta->user_data;

    GstVaapiSurface * const surface = gst_vaapi_video_meta_get_surface(vvmeta);
    GstVaapiDisplay *dpy =
        gst_vaapi_object_get_display(GST_VAAPI_OBJECT(surface));

    if (gst_vaapi_display_get_display_type(dpy) != GST_VAAPI_DISPLAY_TYPE_GLX)
        return FALSE;

    if (texture) {
        GstVaapiDisplay *tex_dpy =
            gst_vaapi_object_get_display(GST_VAAPI_OBJECT(texture));
        if (tex_dpy != dpy)
            gst_vaapi_texture_replace(&texture, NULL);
    }

    if (!texture) {
        /* FIXME Should we assume target and format ? */
        texture = gst_vaapi_texture_new_with_texture(dpy, texture_id[0],
            GL_TEXTURE_2D, GL_RGBA);
        meta->user_data = texture;
    }

    if (!gst_vaapi_apply_composition(surface, meta->buffer))
        GST_WARNING("could not update buffer");

    return gst_vaapi_texture_put_surface(texture, surface,
        gst_vaapi_video_meta_get_render_flags(vvmeta));
}
#endif

static GstBuffer *
gst_surface_buffer_new(void)
{
    GstBuffer * const buffer = gst_buffer_new();

#if USE_GLX
    if (buffer) {
        GstVideoGLTextureType tex_type[] = { GST_VIDEO_GL_TEXTURE_TYPE_RGBA };

        gst_buffer_add_video_gl_texture_upload_meta (buffer,
            GST_VIDEO_GL_TEXTURE_ORIENTATION_X_NORMAL_Y_NORMAL,
            1, tex_type, gst_vaapi_texture_upload,
            texture, NULL, gst_vaapi_texure_upload_free);
    }
#endif

    return buffer;
}
#elif GST_CHECK_VERSION(1,0,0)
#include <gst/video/gstsurfacemeta.h>

#define GST_VAAPI_SURFACE_META_CAST(obj) \
    ((GstVaapiSurfaceMeta *)(obj))

typedef struct _GstVaapiSurfaceMeta GstVaapiSurfaceMeta;
struct _GstVaapiSurfaceMeta {
    GstSurfaceMeta      base;
    GstBuffer          *buffer;
};

#define GST_VAAPI_SURFACE_META_INFO gst_vaapi_surface_meta_get_info()
static const GstMetaInfo *
gst_vaapi_surface_meta_get_info(void);

typedef GstSurfaceConverter *(*GstSurfaceConverterCreateFunc)(
    GstSurfaceMeta *meta, const gchar *type, GValue *dest);

#if USE_GLX
static GstSurfaceConverter *
gst_vaapi_surface_create_converter_glx(GstSurfaceMeta *base_meta,
    const gchar *type, GValue *dest)
{
    GstVaapiSurfaceMeta * const meta = GST_VAAPI_SURFACE_META_CAST(base_meta);

    return gst_vaapi_video_converter_glx_new(meta->buffer, type, dest);
}

#undef gst_vaapi_video_converter_glx_new
#define gst_vaapi_video_converter_glx_new \
    gst_vaapi_surface_create_converter_glx
#endif

static GstSurfaceConverter *
gst_vaapi_surface_create_converter(GstSurfaceMeta *base_meta,
    const gchar *type, GValue *dest)
{
    GstVaapiSurfaceMeta * const meta = GST_VAAPI_SURFACE_META_CAST(base_meta);
    GstVaapiVideoMeta * const vmeta =
        gst_buffer_get_vaapi_video_meta(meta->buffer);
    GstSurfaceConverterCreateFunc func;

    if (G_UNLIKELY(!vmeta))
        return NULL;

    func = (GstSurfaceConverterCreateFunc)
        gst_vaapi_video_meta_get_surface_converter(vmeta);

    return func ? func(base_meta, type, dest) : NULL;
}

static gboolean
gst_vaapi_surface_meta_init(GstVaapiSurfaceMeta *meta, gpointer params,
    GstBuffer *buffer)
{
    meta->base.create_converter = gst_vaapi_surface_create_converter;
    meta->buffer = buffer;
    return TRUE;
}

static void
gst_vaapi_surface_meta_free(GstVaapiSurfaceMeta *meta, GstBuffer *buffer)
{
}

static gboolean
gst_vaapi_surface_meta_transform(GstBuffer *dst_buffer, GstMeta *meta,
    GstBuffer *src_buffer, GQuark type, gpointer data)
{
    GstVaapiVideoMeta * const src_vmeta =
        gst_buffer_get_vaapi_video_meta(src_buffer);

    if (GST_META_TRANSFORM_IS_COPY(type)) {
        GstVaapiSurfaceMeta * const dst_smeta = GST_VAAPI_SURFACE_META_CAST(
            gst_buffer_add_meta(dst_buffer, GST_VAAPI_SURFACE_META_INFO, NULL));

        /* Note: avoid meta lookups in gst_vaapi_surface_create_converter()
           by directly calling the GstVaapiVideoMeta::surface_converter hook */
        dst_smeta->base.create_converter = (GstSurfaceConverterCreateFunc)
            gst_vaapi_video_meta_get_surface_converter(src_vmeta);
        return TRUE;
    }
    return FALSE;
}

const GstMetaInfo *
gst_vaapi_surface_meta_get_info(void)
{
    static gsize g_meta_info;

    if (g_once_init_enter(&g_meta_info)) {
        gsize meta_info = GPOINTER_TO_SIZE(gst_meta_register(
            GST_SURFACE_META_API_TYPE,
            "GstVaapiSurfaceMeta", sizeof(GstVaapiSurfaceMeta),
            (GstMetaInitFunction)gst_vaapi_surface_meta_init,
            (GstMetaFreeFunction)gst_vaapi_surface_meta_free,
            (GstMetaTransformFunction)gst_vaapi_surface_meta_transform));
        g_once_init_leave(&g_meta_info, meta_info);
    }
    return GSIZE_TO_POINTER(g_meta_info);
}

static GstBuffer *
gst_surface_buffer_new(void)
{
    GstBuffer * const buffer = gst_buffer_new();

    if (buffer)
        gst_buffer_add_meta(buffer, GST_VAAPI_SURFACE_META_INFO, NULL);
    return buffer;
}
#else /* GStreamer 0.10 */
#include <gst/video/gstsurfacebuffer.h>

#define GST_VAAPI_TYPE_VIDEO_BUFFER \
    (gst_vaapi_video_buffer_get_type())

#define GST_VAAPI_VIDEO_BUFFER(obj)             \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),          \
        GST_VAAPI_TYPE_VIDEO_BUFFER,            \
        GstVaapiVideoBuffer))

#define GST_VAAPI_VIDEO_BUFFER_CLASS(klass)     \
    (G_TYPE_CHECK_CLASS_CAST((klass),           \
        GST_VAAPI_TYPE_VIDEO_BUFFER,            \
        GstVaapiVideoBufferClass))

#define GST_VAAPI_IS_VIDEO_BUFFER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_VAAPI_TYPE_VIDEO_BUFFER))

#define GST_VAAPI_IS_VIDEO_BUFFER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GST_VAAPI_TYPE_VIDEO_BUFFER))

#define GST_VAAPI_VIDEO_BUFFER_GET_CLASS(obj)   \
    (G_TYPE_INSTANCE_GET_CLASS((obj),           \
        GST_VAAPI_TYPE_VIDEO_BUFFER,            \
        GstVaapiVideoBufferClass))

typedef struct _GstVaapiVideoBufferClass        GstVaapiVideoBufferClass;

/**
 * GstVaapiVideoBuffer:
 *
 * A #GstBuffer holding video objects (#GstVaapiSurface and #GstVaapiImage).
 */
struct _GstVaapiVideoBuffer {
    /*< private >*/
    GstSurfaceBuffer parent_instance;

    GstVaapiVideoMeta *meta;
};

/**
 * GstVaapiVideoBufferClass:
 *
 * A #GstBuffer holding video objects
 */
struct _GstVaapiVideoBufferClass {
    /*< private >*/
    GstSurfaceBufferClass parent_class;
};

GType
gst_vaapi_video_buffer_get_type(void) G_GNUC_CONST;

G_DEFINE_TYPE(GstVaapiVideoBuffer,
              gst_vaapi_video_buffer,
              GST_TYPE_SURFACE_BUFFER)

typedef GstSurfaceConverter *(*GstSurfaceConverterCreateFunc)(
    GstSurfaceBuffer *surface, const gchar *type, GValue *dest);

static GstSurfaceConverter *
gst_vaapi_video_buffer_create_converter(GstSurfaceBuffer *surface,
    const gchar *type, GValue *dest)
{
    GstVaapiVideoBuffer * const vbuffer = GST_VAAPI_VIDEO_BUFFER(surface);
    GstSurfaceConverterCreateFunc func;

    func = (GstSurfaceConverterCreateFunc)
        gst_vaapi_video_meta_get_surface_converter(vbuffer->meta);

    return func ? func(surface, type, dest) : NULL;
}

static void
gst_vaapi_video_buffer_class_init(GstVaapiVideoBufferClass *klass)
{
    GstSurfaceBufferClass * const surface_class =
        GST_SURFACE_BUFFER_CLASS(klass);

    surface_class->create_converter = gst_vaapi_video_buffer_create_converter;
}

static void
gst_vaapi_video_buffer_init(GstVaapiVideoBuffer *buffer)
{
}

static inline GstBuffer *
gst_surface_buffer_new(void)
{
    return GST_BUFFER_CAST(gst_mini_object_new(GST_TYPE_SURFACE_BUFFER));
}
#endif /* GStreamer API  */

static GFunc
get_surface_converter(GstVaapiDisplay *display)
{
    GFunc func;

    switch (gst_vaapi_display_get_display_type(display)) {
#if USE_GLX && !GST_CHECK_VERSION(1,1,0)
    case GST_VAAPI_DISPLAY_TYPE_GLX:
        func = (GFunc)gst_vaapi_video_converter_glx_new;
        break;
#endif
    default:
        func = NULL;
        break;
    }
    return func;
}

static GstBuffer *
new_vbuffer(GstVaapiVideoMeta *meta)
{
    GstBuffer *buffer;

    g_return_val_if_fail(meta != NULL, NULL);

    gst_vaapi_video_meta_set_surface_converter(meta,
        get_surface_converter(gst_vaapi_video_meta_get_display(meta)));

    buffer = gst_surface_buffer_new();
    if (buffer)
        gst_buffer_set_vaapi_video_meta(buffer, meta);
    gst_vaapi_video_meta_unref(meta);
    return buffer;
}

GstBuffer *
gst_vaapi_video_buffer_new(GstVaapiVideoMeta *meta)
{
    g_return_val_if_fail(meta != NULL, NULL);

    return new_vbuffer(gst_vaapi_video_meta_ref(meta));
}

GstBuffer *
gst_vaapi_video_buffer_new_from_pool(GstVaapiVideoPool *pool)
{
    return new_vbuffer(gst_vaapi_video_meta_new_from_pool(pool));
}

GstBuffer *
gst_vaapi_video_buffer_new_from_buffer(GstBuffer *buffer)
{
    GstVaapiVideoMeta * const meta = gst_buffer_get_vaapi_video_meta(buffer);

    return meta ? new_vbuffer(gst_vaapi_video_meta_ref(meta)) : NULL;
}

GstBuffer *
gst_vaapi_video_buffer_new_with_image(GstVaapiImage *image)
{
    return new_vbuffer(gst_vaapi_video_meta_new_with_image(image));
}

GstBuffer *
gst_vaapi_video_buffer_new_with_surface(GstVaapiSurface *surface)
{
    return new_vbuffer(gst_vaapi_video_meta_new_with_surface(surface));
}

GstBuffer *
gst_vaapi_video_buffer_new_with_surface_proxy(GstVaapiSurfaceProxy *proxy)
{
    return new_vbuffer(gst_vaapi_video_meta_new_with_surface_proxy(proxy));
}
