#
# Copyright (c) 2016 Thorsten Kukuk <kukuk@suse.de>
#

busconfigdir = $(sysconfdir)/dbus-1/system.d
introspectiondir = $(datadir)/dbus-1/interfaces

busconfig_DATA = org.opensuse.RebootMgr.conf
introspection_DATA = org.opensuse.RebootMgr.xml

xmllint:
	for F in $(introspection_DATA) $(busconfig_DATA) ; do \
	  xmllint --noout --valid $$F ; \
	done

EXTRA_DIST = $(DATA) introspect.dtd introspect.xsl busconfig.dtd

