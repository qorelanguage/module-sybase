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
	src/minitest.hpp

EXTRA_DIST = COPYING ChangeLog AUTHORS README \
	RELEASE-NOTES \
	test/db-test.q \
	docs/sybase-module-doc.html \
	qore-sybase-modules.spec

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck
