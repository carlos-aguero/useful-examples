# Note: plugindir is set in configure

plugin_LTLIBRARIES = libgstbayercrop.la

# sources used to compile this plug-in
libgstbayercrop_la_SOURCES = gstbayercrop.c gstbayercrop.h

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstbayercrop_la_CFLAGS = $(GST_CFLAGS)
libgstbayercrop_la_LIBADD = $(GST_LIBS)
libgstbayercrop_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstbayercrop_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstbayercrop.h
