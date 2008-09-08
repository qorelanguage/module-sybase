/*
  sybase_connection.h

  Sybase DB layer for QORE
  uses Sybase OpenClient C library

  Qore Programming language

  Copyright (C) 2007

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

#ifndef SYBASE_CONNECTION_H_
#define SYBASE_CONNECTION_H_

#include <cstypes.h>
#include <ctpublic.h>

#include <stdarg.h>

#if defined(SYBASE) || defined(FREETDS_LOCALE)
#define SYB_HAVE_LOCALE 1
#else
#undef SYB_HAVE_LOCALE
#endif

#ifndef CLIENT_VER_LEN
#define CLIENT_VER_LEN 240
#endif

class context {
   private:
      CS_CONTEXT *m_context;

      DLLLOCAL void del() {
	 CS_RETCODE ret = ct_exit(m_context, CS_UNUSED);
	 if (ret != CS_SUCCEED) {
	    ret = ct_exit(m_context, CS_FORCE_EXIT);
	    assert(ret == CS_SUCCEED);
	 }
	 ret = cs_ctx_drop(m_context);
	 assert(ret == CS_SUCCEED);
      }

   public:
      DLLLOCAL context(ExceptionSink *xsink) {
	 CS_RETCODE ret = cs_ctx_alloc(CS_VERSION_100, &m_context);
	 if (ret != CS_SUCCEED) {
	    xsink->raiseException("DBI:SYBASE:CT-LIB-CANNOT-ALLOCATE-ERROR", "cs_ctx_alloc() failed with error %d", ret);   
	    return;
	 }

	 ret = ct_init(m_context, CS_VERSION_100);
	 if (ret != CS_SUCCEED) {
	    del();
	    xsink->raiseException("DBI:SYBASE:CT-LIB-INIT-FAILED", "ct_init() failed with error %d", ret);
	    return;
	 }
      }

      DLLLOCAL ~context() {
	 if (m_context)
	    del();
      }

      DLLLOCAL operator bool() const {
	 return (bool)m_context;
      }

      DLLLOCAL CS_CONTEXT *get_context() { return m_context; }

      DLLLOCAL QoreStringNode *get_client_version(ExceptionSink *xsink) {
	 char *buf = (char *)malloc(sizeof(char) * CLIENT_VER_LEN);
	 CS_INT olen;
	 CS_RETCODE ret = ct_config(m_context, CS_GET, CS_VER_STRING, buf, CLIENT_VER_LEN, &olen);
	 //printd(0, "olen=%d, ret=%d\n", olen, ret);
	 if (ret != CS_SUCCEED) {
	    free(buf);
	    xsink->raiseException("DBI:SYBASE:GET-CLIENT-VERSION-ERROR", "ct_config(CS_VER_STRING) failed with error %d", (int)ret);
	    return 0;
	 }  
	 //printd(5, "client version=%s\n", buf);
	 return new QoreStringNode(buf, olen, CLIENT_VER_LEN, QCS_DEFAULT);
      }
};

// Instantiated class is kept as private data of the Datasource
// for the time the Datasource exists. All other Sybase
// resources are shortlived (including CS_COMMAND* and its wrapper).
class connection
{
   private:
      context m_context;
      CS_CONNECTION* m_connection;
      bool connected;
      const QoreEncoding *enc;

      class AbstractQoreNode *exec_intern(class QoreString *cmd_text, const QoreListNode *qore_args, bool need_list, class ExceptionSink* xsink);

public:
      DLLLOCAL connection(ExceptionSink *xsink);
      DLLLOCAL ~connection();

      // to be called after the object is constructed
      // returns 0=OK, -1=error (exception raised)
      DLLLOCAL int init(const char *username, const char *password, const char *dbname, const char *db_encoding, const QoreEncoding *n_enc, ExceptionSink* xsink);
      // returns 0=OK, -1=error (exception raised)
      DLLLOCAL int purge_messages(class ExceptionSink *xsink);
      // returns -1
      DLLLOCAL int do_exception(class ExceptionSink *xsink, const char *err, const char *fmt, ...);
      // returns 0=OK, -1=error (exception raised)
      DLLLOCAL int direct_execute(const char *sql_text, class ExceptionSink *xsink);
      // returns 0=OK, -1=error (exception raised)
      DLLLOCAL int commit(class ExceptionSink *xsink);
      // returns 0=OK, -1=error (exception raised)
      DLLLOCAL int rollback(class ExceptionSink *xsink);
      
      DLLLOCAL class AbstractQoreNode *exec(const QoreString *cmd, const QoreListNode *parameters, class ExceptionSink *xsink);
      DLLLOCAL class AbstractQoreNode *exec_rows(const QoreString *cmd, const QoreListNode *parameters, class ExceptionSink *xsink);

      DLLLOCAL CS_CONNECTION* getConnection() const { return m_connection; }
      DLLLOCAL CS_CONTEXT* getContext() { return m_context.get_context(); }
      DLLLOCAL const QoreEncoding *getEncoding() const { return enc; }

      DLLLOCAL class QoreStringNode *get_client_version(class ExceptionSink *xsink);
      DLLLOCAL AbstractQoreNode *get_server_version(class ExceptionSink *xsink);
};

#endif

// EOF

