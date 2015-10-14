/*
  sybase.cpp

  Sybase DB layer for QORE
  uses Sybase OpenClient C library

  Qore Programming language

  Copyright (C) 2003 - 2015 Qore Technologies, sro

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "sybase.h"

#include "minitest.hpp"

#include <ctpublic.h>
#include <assert.h>
#include <ctype.h>
#include <memory>
#include <string>
#include <vector>

#include "connection.h"
#include "encoding_helpers.h"

#ifdef SYBASE
DLLEXPORT char qore_module_name[] = "sybase";
DLLEXPORT char qore_module_description[] = "Sybase database driver";
#else
DLLEXPORT char qore_module_name[] = "freetds";
DLLEXPORT char qore_module_description[] = "FreeTDS-based database driver for MS-SQL Server and Sybase";
#endif
DLLEXPORT char qore_module_version[] = PACKAGE_VERSION;
DLLEXPORT char qore_module_author[] = "Qore Technologies";
DLLEXPORT char qore_module_url[] = "http://qore.org";
DLLEXPORT int qore_module_api_major = QORE_MODULE_API_MAJOR;
DLLEXPORT int qore_module_api_minor = QORE_MODULE_API_MINOR;
DLLEXPORT qore_module_init_t qore_module_init = sybase_module_init;
DLLEXPORT qore_module_ns_init_t qore_module_ns_init = sybase_module_ns_init;
DLLEXPORT qore_module_delete_t qore_module_delete = sybase_module_delete;
#ifdef _QORE_HAS_QL_MIT
DLLEXPORT qore_license_t qore_module_license = QL_MIT;
#else
DLLEXPORT qore_license_t qore_module_license = QL_LGPL;
#endif
DLLEXPORT char qore_module_license_str[] = "MIT";
static DBIDriver* DBID_SYBASE;

// capabilities of this driver
int DBI_SYBASE_CAPS =
   DBI_CAP_TRANSACTION_MANAGEMENT
   | DBI_CAP_CHARSET_SUPPORT
   | DBI_CAP_LOB_SUPPORT
   | DBI_CAP_STORED_PROCEDURES
   | DBI_CAP_BIND_BY_VALUE
   | DBI_CAP_BIND_BY_PLACEHOLDER
   | DBI_CAP_HAS_NUMBER_SUPPORT
   | DBI_CAP_AUTORECONNECT
#ifdef _QORE_HAS_DBI_EXECRAW
   | DBI_CAP_HAS_EXECRAW
#endif
#ifdef _QORE_HAS_DBI_EXECRAW
   |DBI_CAP_HAS_EXECRAW
#endif
#ifdef _QORE_HAS_TIME_ZONES
   |DBI_CAP_TIME_ZONE_SUPPORT
#endif
#ifdef _QORE_HAS_FIND_CREATE_TIMEZONE
   |DBI_CAP_SERVER_TIME_ZONE
#endif
   ;

#define BEGIN_CALLBACK \
       do { \
           try { do {} while(0)

#define END_CALLBACK(RV) \
           } catch (const ss::Error &e) { \
               e.raise(xsink); \
               return RV; \
           }\
      } while(0)

#ifdef DEBUG
// exported
AbstractQoreNode* runSybaseTests(const QoreListNode *params, ExceptionSink *xsink) {
   minitest::result res = minitest::execute_all_tests();
   if (res.all_tests_succeeded) {
      printf("************************************************\n");
      printf("Sybase module: %d tests succeeded\n", res.sucessful_tests_count);
      printf("************************************************\n");
      return 0;
   }

   xsink->raiseException("SYBASE-TEST-FAILURE", "Sybase test in file %s, line %d threw an exception.",
			 res.failed_test_file, res.failed_test_line);
   return 0;
}

AbstractQoreNode* runRecentSybaseTests(const QoreListNode *params, ExceptionSink *xsink) {
   minitest::result res = minitest::test_last_changed_files(1);
   if (res.all_tests_succeeded) {
      printf("************************************************\n");
      printf("Sybase module: %d recent tests succeeded\n", res.sucessful_tests_count);
      printf("************************************************\n");
      return 0;
   }

   xsink->raiseException("SYBASE-TEST-FAILURE", "Sybase test in file %s, line %d threw an exception.",
			 res.failed_test_file, res.failed_test_line);
   return 0;
}
#endif

static int sybase_open(Datasource *ds, ExceptionSink *xsink) {
    BEGIN_CALLBACK;
    // username is a required parameter
    if (!ds->getUsername()) {
        xsink->raiseException("DATASOURCE-MISSING-USERNAME", "Datasource has an empty username parameter");
        return -1;
    }

    // DB name is a required parameter
    if (!ds->getDBName()) {
        xsink->raiseException("DATASOURCE-MISSING-DBNAME", "Datasource has an empty dbname parameter");
        return -1;
    }

    // set the encoding for the connection
    if (ds->getDBEncoding()) {
        const QoreEncoding *enc = name_to_QoreEncoding(ds->getDBEncoding());
        ds->setQoreEncoding(enc);
    }
    else {
        const char *enc = QoreEncoding_to_SybaseName(QCS_DEFAULT);
        // if the encoding cannot be mapped, throw a Qore-language exception and return
        if (!enc) {
            xsink->raiseException("DBI:SYBASE:UNKNOWN-CHARACTER-SET", "cannot find the Sybase character encoding equivalent for '%s'", QCS_DEFAULT->getCode());
            return -1;
        }
        ds->setDBEncoding(enc);
        ds->setQoreEncoding(QCS_DEFAULT);
    }

    // create the connection object
    std::auto_ptr<connection> sc(new connection(ds, xsink));
    if (*xsink)
        return -1;

#ifdef QORE_HAS_DATASOURCE_PORT
    int port = ds->getPort();
#else
    int port = 0;
#endif

    if (port && !ds->getHostName()) {
        xsink->raiseException("DBI:SYBASE:CONNECT-ERROR", "port is set to %d, but no hostname is set; both hostname and port must be set to override the interfaces file", port);
        return -1;
    }

    if (!port && ds->getHostName()) {
        xsink->raiseException("DBI:SYBASE:CONNECT-ERROR", "hostname is set to '%s', but no port is set; both hostname and port must be set to override the interfaces file", ds->getHostName());
        return -1;
    }

    // make the actual connection to the database
    sc->init(ds->getUsername(), ds->getPassword() ? ds->getPassword() : "", ds->getDBName(), ds->getDBEncoding(), ds->getQoreEncoding(), ds->getHostName(), port, xsink);
    // return with an error if it didn't work
    if (*xsink)
        return -1;

    // set the private data
    ds->setPrivateData(sc.release());

    // return 0 for OK
    return 0;
    END_CALLBACK(-1);
}

static int sybase_close(Datasource *ds) {
   try {
       connection* sc = (connection*)ds->getPrivateData();
       ds->setPrivateData(0);
       delete sc;
   } catch (const ss::Error &e) {}
   return 0;
}

static AbstractQoreNode* sybase_select(Datasource *ds, const QoreString *qstr, const QoreListNode *args, ExceptionSink *xsink) {
   BEGIN_CALLBACK;
   connection *conn = (connection*)ds->getPrivateData();
   return conn->exec(qstr, args, xsink);
   END_CALLBACK(0);
}

static AbstractQoreNode* sybase_select_rows(Datasource *ds, const QoreString *qstr, const QoreListNode *args, ExceptionSink *xsink) {
   BEGIN_CALLBACK;
   connection *conn = (connection*)ds->getPrivateData();
   AbstractQoreNode* rv = conn->exec_rows(qstr, args, xsink);
   if (get_node_type(rv) == NT_HASH) {
      QoreListNode* l = new QoreListNode;
      l->push(rv);
      rv = l;
   }
   return rv;
   //return conn->exec_rows(qstr, args, xsink);
   END_CALLBACK(0);
}

static AbstractQoreNode* sybase_exec(Datasource *ds, const QoreString *qstr, const QoreListNode *args, ExceptionSink *xsink) {
   BEGIN_CALLBACK;
   connection *conn = (connection*)ds->getPrivateData();
   return conn->exec(qstr, args, xsink);
   END_CALLBACK(0);
}

#ifdef _QORE_HAS_DBI_EXECRAW
static AbstractQoreNode* sybase_execRaw(Datasource *ds, const QoreString *qstr, ExceptionSink *xsink) {
   BEGIN_CALLBACK;
   connection *conn = (connection*)ds->getPrivateData();
   return conn->execRaw(qstr, xsink);
   END_CALLBACK(0);
}
#endif

static int sybase_commit(Datasource *ds, ExceptionSink *xsink) {
   BEGIN_CALLBACK;
   connection* conn = (connection*)ds->getPrivateData();
   return conn->commit(xsink);
   END_CALLBACK(0);
}

static int sybase_rollback(Datasource *ds, ExceptionSink *xsink) {
   BEGIN_CALLBACK;
   connection* conn = (connection*)ds->getPrivateData();
   return conn->rollback(xsink);
   END_CALLBACK(0);
}

static AbstractQoreNode *sybase_get_client_version(const Datasource *ds, ExceptionSink *xsink) {
   BEGIN_CALLBACK;
   context m_context(xsink);
   if (!m_context)
      return 0;

   return m_context.get_client_version(xsink);
   END_CALLBACK(0);
}

static AbstractQoreNode *sybase_get_server_version(Datasource *ds, ExceptionSink *xsink) {
   BEGIN_CALLBACK;
   connection* conn = (connection*)ds->getPrivateData();
   return conn->get_server_version(xsink);
   END_CALLBACK(0);
}

static int sybase_opt_set(Datasource* ds, const char* opt, const AbstractQoreNode* val, ExceptionSink* xsink) {
   BEGIN_CALLBACK;
    connection *conn = (connection*)ds->getPrivateData();
    return conn->setOption(opt, val, xsink);
   END_CALLBACK(0);
}

static AbstractQoreNode* sybase_opt_get(const Datasource* ds, const char* opt) {
    try {
        connection *conn = (connection*)ds->getPrivateData();
        return conn->getOption(opt);
    } catch (const ss::Error &e) {
        return 0;
    }
}

namespace ss {
    void init(qore_dbi_method_list &methods);
}

QoreStringNode *sybase_module_init() {
   QORE_TRACE("sybase_module_init()");

   // init_namespace();

#ifdef DEBUG
   builtinFunctions.add("runSybaseTests", runSybaseTests, QDOM_DATABASE);
   builtinFunctions.add("runRecentSybaseTests", runRecentSybaseTests, QDOM_DATABASE);
#endif

   // register driver with DBI subsystem
   qore_dbi_method_list methods;
   methods.add(QDBI_METHOD_OPEN, sybase_open);
   methods.add(QDBI_METHOD_CLOSE, sybase_close);
   methods.add(QDBI_METHOD_SELECT, sybase_select);
   methods.add(QDBI_METHOD_SELECT_ROWS, sybase_select_rows);
   methods.add(QDBI_METHOD_EXEC, sybase_exec);
#ifdef _QORE_HAS_DBI_EXECRAW
   methods.add(QDBI_METHOD_EXECRAW, sybase_execRaw);
#endif
   methods.add(QDBI_METHOD_COMMIT, sybase_commit);
   methods.add(QDBI_METHOD_ROLLBACK, sybase_rollback);
   methods.add(QDBI_METHOD_GET_CLIENT_VERSION, sybase_get_client_version);
   methods.add(QDBI_METHOD_GET_SERVER_VERSION, sybase_get_server_version);

   methods.add(QDBI_METHOD_OPT_SET, sybase_opt_set);
   methods.add(QDBI_METHOD_OPT_GET, sybase_opt_get);

   methods.registerOption(DBI_OPT_NUMBER_OPT, "when set, numeric/decimal values are returned as integers if possible, otherwise as arbitrary-precision number values; the argument is ignored; setting this option turns it on and turns off 'string-numbers' and 'numeric-numbers'");
   methods.registerOption(DBI_OPT_NUMBER_STRING, "when set, numeric/decimal values are returned as strings for backwards-compatibility; the argument is ignored; setting this option turns it on and turns off 'optimal-numbers' and 'numeric-numbers'");
   methods.registerOption(DBI_OPT_NUMBER_NUMERIC, "when set, numeric/decimal values are returned as arbitrary-precision number values; the argument is ignored; setting this option turns it on and turns off 'string-numbers' and 'optimal-numbers'");
   methods.registerOption(DBI_OPT_TIMEZONE, "set the server-side timezone,"
           " value must be a string in the format accepted by"
           " Timezone::constructor() on the client (ie either a region"
           " name or a UTC offset like \"+01:00\"), if not set the"
           " server's time zone will be assumed to be the same as"
           " the client's", stringTypeInfo);

   ss::init(methods);

#ifdef SYBASE
   DBID_SYBASE = DBI.registerDriver("sybase", methods, DBI_SYBASE_CAPS);
#else
   DBID_SYBASE = DBI.registerDriver("freetds", methods, DBI_SYBASE_CAPS);
#endif

   return 0;
}

void sybase_module_ns_init(QoreNamespace *rns, QoreNamespace *qns) {
}

void sybase_module_delete() {
   QORE_TRACE("sybase_module_delete()");
}
