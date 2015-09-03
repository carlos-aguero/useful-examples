#L$
# Copyright (C) 2011 Ridgerun (http://www.ridgerun.com). 
##$L$

# We don't have to fetch the source code, is local
FETCHER_NO_DOWNLOAD=yes
DESTDIR=$(FSDEVROOT)
include ../../bsp/classes/rrsdk.class

AUTOTOOLS_PARAMS = --prefix=/usr/  LDFLAGS="-Wl,--rpath-link -Wl,$(FSDEVROOT)/usr/lib:$(FSDEVROOT)/lib" --sysconfdir=$(FSDEVROOT)/etc

# Name of the binary to install on the final fs

include $(CLASSES)/autotools.class

LIBRARIES=/usr/lib/{libtest-1.0.1.so,libtest.so}

