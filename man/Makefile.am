#
# Copyright (c) 2016, 2017 Thorsten Kukuk, Germany
#
# Author: Thorsten Kukuk <kukuk@suse.de>
#

CLEANFILES = $(MANS) $(DATA) rebootmgr.index.xml

EXTRA_DIST = $(MANS) $(XMLS) $(DATA) custom-html.xsl \
	make-man-index.py xml_helper.py rebootmgr.index.xml

man_MANS = rebootmgrd.8 rebootmgr.service.8 org.opensuse.RebootMgr.conf.8 \
	rebootmgrctl.1
noinst_DATA = rebootmgrd.8.html rebootmgr.service.8.html \
	org.opensuse.RebootMgr.conf.8.html rebootmgrctl.1.html \
	rebootmgr.index.html

XMLS = rebootmgrd.8.xml rebootmgrctl.1.xml

XSLTPROC_FLAGS_HTML = \
	--nonet \
	--xinclude \
        --stringparam man.authors.section.enabled 0 \
        --stringparam man.copyright.section.enabled 0 \
	--stringparam rebootmgr.version $(VERSION) \
	--stringparam chunker.output.encoding UTF-8 \
	$(srcdir)/custom-html.xsl

if ENABLE_REGENERATE_MAN

define html-alias
        $(LN_S) -f $(notdir $<) $@
endef

rebootmgr.index.xml: make-man-index.py $(XMLS)
	python $(srcdir)/make-man-index.py $(srcdir)/$@ $(XMLS)

rebootmgrd.8: rebootmgrd.8.xml
	$(XSLTPROC) -o $(srcdir)/$@ --path $(srcdir) --xinclude --nonet http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

rebootmgrd.8.html: rebootmgrd.8.xml custom-html.xsl
	$(XSLTPROC) -o $(srcdir)/$@ --path $(srcdir) $(XSLTPROC_FLAGS_HTML) $<

org.opensuse.RebootMgr.conf.8.html: rebootmgrd.8.xml
	$(html-alias)
rebootmgr.service.8.html: rebootmgrd.8.xml
	$(html-alias)

rebootmgrctl.1: rebootmgrctl.1.xml
	$(XSLTPROC) -o $(srcdir)/$@ --path $(srcdir) --xinclude --nonet http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

rebootmgrctl.1.html: rebootmgrctl.1.xml custom-html.xsl
	$(XSLTPROC) -o $(srcdir)/$@ --path $(srcdir) $(XSLTPROC_FLAGS_HTML) $<

rebootmgr.index.html: rebootmgr.index.xml
	$(XSLTPROC) -o $(srcdir)/$@ --path $(srcdir) $(XSLTPROC_FLAGS_HTML) $<

endif
