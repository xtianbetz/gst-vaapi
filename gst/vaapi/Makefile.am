plugin_LTLIBRARIES = libgstvaapi.la

libgstvaapi_CFLAGS = \
	$(LIBVA_CFLAGS)		\
	-DGST_USE_UNSTABLE_API	\
	-I$(top_srcdir)/gst-libs

if USE_VAAPI_GLX
libgstvaapi_LIBS = \
	$(top_builddir)/gst-libs/gst/vaapi/libgstvaapi-glx-$(GST_MAJORMINOR).la
else
libgstvaapi_LIBS = \
	$(top_builddir)/gst-libs/gst/vaapi/libgstvaapi-x11-$(GST_MAJORMINOR).la
endif

libgstvaapi_la_SOURCES =	\
	gstvaapi.c		\
	gstvaapiconvert.c	\
	gstvaapidecode.c	\
	gstvaapipluginutil.c	\
	gstvaapisink.c		\
	$(NULL)

noinst_HEADERS =		\
	gstvaapiconvert.h	\
	gstvaapidecode.h	\
	gstvaapipluginutil.h	\
	gstvaapisink.h		\
	$(NULL)

libgstvaapi_la_CFLAGS =	\
	$(libgstvaapi_CFLAGS)	\
	$(GST_CFLAGS)		\
	$(GST_BASE_CFLAGS)	\
	$(GST_VIDEO_CFLAGS)	\
	$(GST_INTERFACES_CFLAGS) \
	$(GST_BASEVIDEO_CFLAGS)	\
	$(GST_PLUGINS_BASE_CFLAGS)

libgstvaapi_la_LIBADD =	\
	$(libgstvaapi_LIBS)	\
	$(GST_LIBS)		\
	$(GST_BASE_LIBS)	\
	$(GST_VIDEO_LIBS)	\
	$(GST_INTERFACES_LIBS)	\
	$(GST_BASEVIDEO_LIBS)	\
	$(GST_PLUGINS_BASE_LIBS)

libgstvaapi_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstvaapi_la_LIBTOOLFLAGS = --tag=disable-static

# Extra clean files so that maintainer-clean removes *everything*
MAINTAINERCLEANFILES = Makefile.in