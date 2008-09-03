/*
  sybase.cc

  Sybase DB layer for QORE
  uses Sybase OpenClient C library

  Qore Programming language

  Copyright (C) 2003, 2004, 2005, 2006

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

#include <qore/Qore.h>
#include <qore/minitest.hpp>

#include <ctpublic.h>
#include <assert.h>
#include <ctype.h>
#include <memory>
#include <string>
#include <vector>

#include "sybase.h"
#include "connection.h"
#include "encoding_helpers.h"

#ifndef QORE_MONOLITHIC
#ifdef SYBASE
DLLEXPORT char qore_module_name[] = "sybase";
DLLEXPORT char qore_module_description[] = "Sybase database driver";
#else
DLLEXPORT char qore_module_name[] = "mssql";
DLLEXPORT char qore_module_description[] = "FreeTDS-based database driver for MS-SQL Server and Sybase";
#endif
DLLEXPORT char qore_module_version[] = "1.0";
DLLEXPORT char qore_module_author[] = "Qore Technologies";
DLLEXPORT char qore_module_url[] = "http://qore.sourceforge.net";
DLLEXPORT int qore_module_api_major = QORE_MODULE_API_MAJOR;
DLLEXPORT int qore_module_api_minor = QORE_MODULE_API_MINOR;
DLLEXPORT qore_module_init_t qore_module_init = sybase_module_init;
DLLEXPORT qore_module_ns_init_t qore_module_ns_init = sybase_module_ns_init;
DLLEXPORT qore_module_delete_t qore_module_delete = sybase_module_delete;
DLLEXPORT qore_license_t qore_module_license = QL_LGPL;
#endif

static DBIDriver* DBID_SYBASE;

// capabilities of this driver
#define DBI_SYBASE_CAPS ( DBI_CAP_TRANSACTION_MANAGEMENT | \
  DBI_CAP_CHARSET_SUPPORT | \
  DBI_CAP_LOB_SUPPORT | \
  DBI_CAP_STORED_PROCEDURES | \
  DBI_CAP_BIND_BY_VALUE | \
  DBI_CAP_BIND_BY_PLACEHOLDER )

#ifdef DEBUG
// exported
AbstractQoreNode* runSybaseTests(const QoreListNode *params, ExceptionSink *xsink)
{
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

AbstractQoreNode* runRecentSybaseTests(const QoreListNode *params, ExceptionSink *xsink)
{
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

static int sybase_open(Datasource *ds, ExceptionSink *xsink)
{
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
   std::auto_ptr<connection> sc(new connection);
  
   // make the actual connection to the database
   sc->init(ds->getUsername(), ds->getPassword() ? ds->getPassword() : "", ds->getDBName(), ds->getDBEncoding(), ds->getQoreEncoding(), xsink);
   // return with an error if it didn't work
   if (*xsink)
      return -1;

   // set the private data
   ds->setPrivateData(sc.release());

   // return 0 for OK
   return 0;
}

static int sybase_close(Datasource *ds)
{
   connection* sc = (connection*)ds->getPrivateData();
   ds->setPrivateData(0);
   delete sc;

   return 0;
}

//------------------------------------------------------------------------------
static AbstractQoreNode* sybase_select(Datasource *ds, const QoreString *qstr, const QoreListNode *args, ExceptionSink *xsink)
{
   connection *conn = (connection*)ds->getPrivateData();
   return conn->exec(qstr, args, xsink);
}

static AbstractQoreNode* sybase_select_rows(Datasource *ds, const QoreString *qstr, const QoreListNode *args, ExceptionSink *xsink)
{
   connection *conn = (connection*)ds->getPrivateData();
   //printd(5, "sybase_select_rows(ds=%08p, qstr='%s', args=%08p)\n", ds, qstr->getBuffer(), args);
   return conn->exec_rows(qstr, args, xsink);
}

static AbstractQoreNode* sybase_exec(Datasource *ds, const QoreString *qstr, const QoreListNode *args, ExceptionSink *xsink)
{
   connection *conn = (connection*)ds->getPrivateData();
   return conn->exec(qstr, args, xsink);
}

static int sybase_commit(Datasource *ds, ExceptionSink *xsink)
{
   connection* conn = (connection*)ds->getPrivateData();
   return conn->commit(xsink);
}

static int sybase_rollback(Datasource *ds, ExceptionSink *xsink)
{
   connection* conn = (connection*)ds->getPrivateData();
   return conn->rollback(xsink);
}

static class AbstractQoreNode *sybase_get_client_version(const Datasource *ds, ExceptionSink *xsink)
{
   connection* conn = (connection*)ds->getPrivateData();
   return conn->get_client_version(xsink);
}

static class AbstractQoreNode *sybase_get_server_version(Datasource *ds, ExceptionSink *xsink)
{
   connection* conn = (connection*)ds->getPrivateData();
   return conn->get_server_version(xsink);
}

/*
// constants are not needed now as specifying placeholder buffer types is not necessary
static void add_constants(QoreNamespace* ns)
{
   ns->addConstant("CS_CHAR_TYPE", new QoreBigIntNode(CS_CHAR_TYPE));
   ns->addConstant("CS_BINARY_TYPE", new QoreBigIntNode(CS_BINARY_TYPE));
   ns->addConstant("CS_LONGCHAR_TYPE", new QoreBigIntNode(CS_LONGCHAR_TYPE));
   ns->addConstant("CS_LONGBINARY_TYPE", new QoreBigIntNode(CS_LONGBINARY_TYPE));
   ns->addConstant("CS_TEXT_TYPE", new QoreBigIntNode(CS_TEXT_TYPE));
   ns->addConstant("CS_IMAGE_TYPE", new QoreBigIntNode(CS_IMAGE_TYPE));
   ns->addConstant("CS_TINYINT_TYPE", new QoreBigIntNode(CS_TINYINT_TYPE));
   ns->addConstant("CS_SMALLINT_TYPE", new QoreBigIntNode(CS_SMALLINT_TYPE));
   ns->addConstant("CS_INT_TYPE", new QoreBigIntNode(CS_INT_TYPE));
   ns->addConstant("CS_REAL_TYPE", new QoreBigIntNode(CS_REAL_TYPE));
   ns->addConstant("CS_FLOAT_TYPE", new QoreBigIntNode(CS_FLOAT_TYPE));
   ns->addConstant("CS_BIT_TYPE", new QoreBigIntNode(CS_BIT_TYPE));
   ns->addConstant("CS_DATETIME_TYPE", new QoreBigIntNode(CS_DATETIME_TYPE));
   ns->addConstant("CS_DATETIME4_TYPE", new QoreBigIntNode(CS_DATETIME4_TYPE));
   ns->addConstant("CS_MONEY_TYPE", new QoreBigIntNode(CS_MONEY_TYPE));
   ns->addConstant("CS_MONEY4_TYPE", new QoreBigIntNode(CS_MONEY4_TYPE));
   ns->addConstant("CS_NUMERIC_TYPE", new QoreBigIntNode(CS_NUMERIC_TYPE));
   ns->addConstant("CS_DECIMAL_TYPE", new QoreBigIntNode(CS_DECIMAL_TYPE));
   ns->addConstant("CS_VARCHAR_TYPE", new QoreBigIntNode(CS_VARCHAR_TYPE));
   ns->addConstant("CS_VARBINARY_TYPE", new QoreBigIntNode(CS_VARBINARY_TYPE));
   ns->addConstant("CS_LONG_TYPE", new QoreBigIntNode(CS_LONG_TYPE));
   ns->addConstant("CS_SENSITIVITY_TYPE", new QoreBigIntNode(CS_SENSITIVITY_TYPE));
   ns->addConstant("CS_BOUNDARY_TYPE", new QoreBigIntNode(CS_BOUNDARY_TYPE));
   ns->addConstant("CS_VOID_TYPE", new QoreBigIntNode(CS_VOID_TYPE));
   ns->addConstant("CS_USHORT_TYPE", new QoreBigIntNode(CS_USHORT_TYPE));
   ns->addConstant("CS_UNICHAR_TYPE", new QoreBigIntNode(CS_UNICHAR_TYPE));
#ifdef CS_BLOB_TYPE
   ns->addConstant("CS_BLOB_TYPE", new QoreBigIntNode(CS_BLOB_TYPE));
#endif
#ifdef CS_DATE_TYPE
   ns->addConstant("CS_DATE_TYPE", new QoreBigIntNode(CS_DATE_TYPE));
#endif
#ifdef CS_TIME_TYPE
   ns->addConstant("CS_TIME_TYPE", new QoreBigIntNode(CS_TIME_TYPE));
#endif
#ifdef CS_UNITEXT_TYPE
   ns->addConstant("CS_UNITEXT_TYPE", new QoreBigIntNode(CS_UNITEXT_TYPE));
#endif
#ifdef CS_BIGINT_TYPE
   ns->addConstant("CS_BIGINT_TYPE", new QoreBigIntNode(CS_BIGINT_TYPE));
#endif
#ifdef CS_USMALLINT_TYPE
   ns->addConstant("CS_USMALLINT_TYPE", new QoreBigIntNode(CS_USMALLINT_TYPE));
#endif
#ifdef CS_UINT_TYPE
   ns->addConstant("CS_UINT_TYPE", new QoreBigIntNode(CS_UINT_TYPE));
#endif
#ifdef CS_UBIGINT_TYPE
   ns->addConstant("CS_UBIGINT_TYPE", new QoreBigIntNode(CS_UBIGINT_TYPE));
#endif
#ifdef CS_XML_TYPE
   ns->addConstant("CS_XML_TYPE", new QoreBigIntNode(CS_XML_TYPE));
#endif
}

static QoreNamespace *sybase;
 
static void init_namespace()
{
   QoreNamespace *sybase = 
#ifdef SYBASE
      new QoreNamespace("Sybase");
#else
      new QoreNamespace("MSSQL");
#endif
   add_constants(sybase);
}
 */

QoreStringNode *sybase_module_init()
{
   QORE_TRACE("sybase_module_init()");

   // init_namespace();
   
#ifdef DEBUG
   builtinFunctions.add("runSybaseTests", runSybaseTests, QDOM_DATABASE);
   builtinFunctions.add("runRecentSybaseTests", runRecentSybaseTests, QDOM_DATABASE);
#endif
   
   // register driver with DBI subsystem
   class qore_dbi_method_list methods;
   methods.add(QDBI_METHOD_OPEN, sybase_open);
   methods.add(QDBI_METHOD_CLOSE, sybase_close);
   methods.add(QDBI_METHOD_SELECT, sybase_select);
   methods.add(QDBI_METHOD_SELECT_ROWS, sybase_select_rows);
   methods.add(QDBI_METHOD_EXEC, sybase_exec);
   methods.add(QDBI_METHOD_COMMIT, sybase_commit);
   methods.add(QDBI_METHOD_ROLLBACK, sybase_rollback);
   methods.add(QDBI_METHOD_GET_CLIENT_VERSION, sybase_get_client_version);
   methods.add(QDBI_METHOD_GET_SERVER_VERSION, sybase_get_server_version);
   
#ifdef SYBASE
   DBID_SYBASE = DBI.registerDriver("sybase", methods, DBI_SYBASE_CAPS);
#else
   DBID_SYBASE = DBI.registerDriver("mssql", methods, DBI_SYBASE_CAPS);
#endif


   return 0;
}

void sybase_module_ns_init(QoreNamespace *rns, QoreNamespace *qns)
{
/*
  // this is commented out because the constants are not needed (or documented) at the moment
  // maybe later we can use them
   QORE_TRACE("sybase_module_ns_init()");
   qns->addInitialNamespace(sybase->copy());

*/
}

void sybase_module_delete()
{
   QORE_TRACE("sybase_module_delete()");

}
