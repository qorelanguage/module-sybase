AUTOMAKE_OPTIONS = foreign

SUBDIRS = src

noinst_HEADERS = src/command.h \
	src/common_constants.h \
	src/connection.h \
	src/conversions.h \
	src/encoding_helpers.h \
	src/row_output_buffers.h \
	src/sybase.h \
	src/sybase_query.h \
	src/utils.h \
	src/dbmodulewrap.h \
	src/emptystatement.h \
	src/error.h \
	src/resultfactory.h \
	src/row_output_buffers.h \
	src/minitest.hpp

EXTRA_DIST = COPYING.MIT COPYING.LGPL AUTHORS README \
	RELEASE-NOTES \
	test/sybase-statement.qtest \
	test/sybase-types.qtest \
	qore-sybase-modules.spec

ACLOCAL_AMFLAGS=-I m4

if COND_DOXYGEN
DOX = sybase
DOXYGEN_OUTPUT = docs/sybase/html
DOXYFILES = docs/doxyfile docs/mainpage.dox
DOXYFILES_SRC = docs/doxyfile.tmpl docs/footer_template.html
QDX = qdx
DX_CLEANFILES = ${DOXYFILES}

docs/sybase/html: $(DOXYFILES)
	cd docs; $(DOXYGEN_CMD) doxyfile;
	$(QDX) --post docs/sybase/html/*html
	$(QDX) --post docs/sybase/html/search/*html

docs/mainpage.dox: docs/mainpage.dox.tmpl
	$(QDX) -d $< $@

docs/doxyfile: docs/doxyfile.tmpl
	sed s/__VERSION__/${VERSION}/ $< > $@

doxygen-doc: $(DOXYGEN_OUTPUT)

EXTRA_DIST += $(DOXYGEN_OUTPUT)

html-local: $(DOXYGEN_OUTPUT)

install-html-local:
	for dox in $(DOX); do $(MKDIR_P) $(DESTDIR)$(datadir)/$(PACKAGE)/docs/$$dox/html/search; done
	for dir in $(DOXYGEN_OUTPUT); do \
	   for f in $$dir/*; do if [ -f $$f ]; then $(INSTALL_DATA) $$f $(DESTDIR)$(datadir)/$(PACKAGE)/$$dir; fi; done; \
	   for f in $$dir/search/*; do $(INSTALL_DATA) $$f $(DESTDIR)$(datadir)/$(PACKAGE)/$$dir/search; done; \
	done

clean-local:
	$(RM) -rf $(DOXYGEN_OUTPUT)

uninstall-local:
	$(RM) -rf $(DESTDIR)$(datadir)/$(PACKAGE)/docs

mostlyclean-local:
	rm -rf ${DOXYGEN_OUTPUT} ${DX_CLEANFILES}
else
doxygen-doc:
	echo no doxygen support
endif

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck
