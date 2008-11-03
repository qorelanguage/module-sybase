if COND_SINGLE_COMPILATION_UNIT
SYBASE_SOURCES = single-compilation-unit.cc
else
SYBASE_SOURCES = sybase.cc connection.cc conversions.cc command.cc encoding_helpers.cc sybase_query.cc row_output_buffers.cc
endif

lib_LTLIBRARIES =
if COND_SYBASE
lib_LTLIBRARIES += sybase.la
sybase_la_SOURCES = $(SYBASE_SOURCES)
sybase_la_LDFLAGS = -module -avoid-version ${SYBASE_LIBS}
sybase_la_CPPFLAGS = $(QORE_CPPFLAGS) $(SYBASE_INCLUDES) -DSYBASE=1
endif
if COND_FREETDS
lib_LTLIBRARIES += freetds.la
freetds_la_SOURCES = $(SYBASE_SOURCES)
freetds_la_LDFLAGS = -module -avoid-version ${FREETDS_LIBS}
freetds_la_CPPFLAGS = $(QORE_CPPFLAGS) $(FREETDS_INCLUDES) -DFREETDS=1
endif

INCLUDES = -I$(top_srcdir)/include 

if COND_PROFILE
AM_CXXFLAGS=-g -pg
else
if COND_DEBUG
AM_CXXFLAGS=-g
endif
endif

install-exec-hook:
if COND_SYBASE
	mv $(DESTDIR)$(libdir)/sybase.$(MODULE_SUFFIX) $(DESTDIR)$(libdir)/sybase-api-$(MODULE_API).qmod
	rm -f $(DESTDIR)$(libdir)/sybase.la
endif
if COND_FREETDS
	mv $(DESTDIR)$(libdir)/freetds.$(MODULE_SUFFIX) $(DESTDIR)$(libdir)/freetds-api-$(MODULE_API).qmod
	rm -f $(DESTDIR)$(libdir)/freetds.la
endif
