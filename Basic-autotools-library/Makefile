#$L$
# Copyright (C) 2011 Ridgerun (http://www.ridgerun.com). 
##$L$

# We don't have to fetch the source code, is local
FETCHER_NO_DOWNLOAD=yes

# Vala code needs to build on his own src dir
AUTOTOOLS_BUILD_DIR=src

AUTOTOOLSi_PARAMS = --prefix=/usr/lib/  LDFLAGS="-Wl,--rpath-link -Wl,$(FSDEVROOT)/usr/lib:$(FSDEVROOT)/lib" --sysconfdir=$(FSDEVROOT)/etc

# Name of the binary to install on the final fs
LIBRARIES=/usr/lib/{libtest.so,libtest.so.0,libtest.so.0.0.0}

include ../../bsp/classes/rrsdk.class
include $(CLASSES)/autotools.class


