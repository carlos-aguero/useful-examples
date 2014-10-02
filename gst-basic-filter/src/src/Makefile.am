
plugin_LTLIBRARIES = libgstbufferpartitioner.la

# sources used to compile this plug-in
libgstbufferpartitioner_la_SOURCES = gstbufferpartitioner.c gstbufferpartitioner.h

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstbufferpartitioner_la_CFLAGS = $(GST_CFLAGS)
libgstbufferpartitioner_la_LIBADD = $(GST_LIBS)
libgstbufferpartitioner_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstbufferpartitioner_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstplugin.h
