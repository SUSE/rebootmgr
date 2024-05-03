#
# Copyright (c) 2016, 2017 Thorsten Kukuk, Germany
#
# Author: Thorsten Kukuk <kukuk@suse.de>
#

CLEANFILES = $(MANS) $(DATA)

EXTRA_DIST = $(MANS) $(XMLS) $(DATA)

man_MANS = rebootmgrd.8 rebootmgr.service.8 org.opensuse.RebootMgr.conf.8 \
	rebootmgrctl.1 rebootmgr.conf.5

XMLS = rebootmgrd.8.xml rebootmgrctl.1.xml rebootmgr.conf.5.xml

if ENABLE_REGENERATE_MAN

rebootmgr.service.8: rebootmgrd.8
org.opensuse.RebootMgr.conf.8: rebootmgrd.8

rebootmgrd.8: rebootmgrd.8.xml
	$(XSLTPROC) -o $(srcdir)/$@ --path $(srcdir) --xinclude --nonet http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

rebootmgrctl.1: rebootmgrctl.1.xml
	$(XSLTPROC) -o $(srcdir)/$@ --path $(srcdir) --xinclude --nonet http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

rebootmgr.conf.5: rebootmgr.conf.5.xml
	$(XSLTPROC) -o $(srcdir)/$@ --path $(srcdir) --xinclude --nonet http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

endif
