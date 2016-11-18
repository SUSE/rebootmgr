#
# Copyright (c) 2016 Thorsten Kukuk, Germany
#
# Author: Thorsten Kukuk <kukuk@suse.de>
#

CLEANFILES = transactional-update.8 transactional-update.timer.8 \
	transactional-update.service.8

EXTRA_DIST = $(MANS) $(XMLS)

man_MANS = rebootmgrd.8
XMLS = rebootmgrd.8.xml

if ENABLE_REGENERATE_MAN
rebootmgrd.8: rebootmgrd.8.xml
	$(XMLLINT) --nonet --xinclude --postvalid --noout $<
	$(XSLTPROC) -o $(srcdir)/$@ --path $(srcdir) --xinclude --nonet http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<
endif
