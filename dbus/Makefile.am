#
# Copyright (c) 2016, 2021 Thorsten Kukuk <kukuk@suse.de>
#

busconfigdir = $(datadir)/dbus-1/system.d
introspectiondir = $(datadir)/dbus-1/interfaces
systemservicedir = $(datadir)/dbus-1/system-services

busconfig_DATA = org.opensuse.RebootMgr.conf
introspection_DATA = org.opensuse.RebootMgr.xml
systemservice_DATA = org.opensuse.RebootMgr.service

xmllint:
	for F in $(introspection_DATA) $(busconfig_DATA) ; do \
	  xmllint --noout --valid $$F ; \
	done

EXTRA_DIST = $(DATA) introspect.dtd introspect.xsl busconfig.dtd

