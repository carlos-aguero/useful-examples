#$L$
# Copyright (C) 2014 Ridgerun (http://www.ridgerun.com).
##$L$

PKG_URL=https://github.com/zeux/pugixml/releases/download/v1.4/
PKG_TARBALL=pugixml-1.4.tar.gz
PKG_SHA1SUM=76d16b3be36390b1f17da8b80f6e064287d64686

AUTOTOOLS_AUTOGEN_GENERATION_RULE=mod_autogen_permissions

include ../../../bsp/classes/rrsdk.class

DESTDIR=$(FSDEVROOT)

include $(CLASSES)/autotools.class

mod_autogen_permissions:
	$(V) chmod 775 src/autogen.sh
	$(V) touch $@

distclean:
	$(V) rm -rf src/ mod_autogen_permissions
 
LIBRARIES=/usr/lib/{libpugixml-1.4.0.so,libpugixml.so}
