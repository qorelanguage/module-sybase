/*
  sybase_connection.h

  Sybase DB layer for QORE
  uses Sybase OpenClient C library

  Qore Programming language

  Copyright (C) 2007 - 2016 Qore Technologies, s.r.o.

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
#include <qore/common.h>

#include <stdarg.h>
#include <qore/ExceptionSink.h>

#include "command.h"
#include "dbmodulewrap.h"
#include "statement.h"

#if defined(SYBASE) || defined(FREETDS_LOCALE)
#define SYB_HAVE_LOCALE 1
#else
#undef SYB_HAVE_LOCALE
#endif

#ifndef CLIENT_VER_LEN
#define CLIENT_VER_LEN 240
#endif

#ifdef SYBASE
extern QoreThreadLock ct_lock;
extern QoreThreadLock cs_lock;
#endif

class AbstractQoreZoneInfo;

typedef ss::DBModuleWrap<ss::Statement>::ModuleWrap stmt_t;

class context {
   private:
      CS_CONTEXT *m_context;

      DLLLOCAL void del() {
         //printd(5, "context::del() this=%p deleting %p\n", this, m_context);

         CS_RETCODE ret;
#ifdef SYBASE
         {
            AutoLocker al(ct_lock);
#endif
            ret = ct_exit(m_context, CS_UNUSED);
#ifdef SYBASE
         }
#endif
         if (ret != CS_SUCCEED) {
#ifdef SYBASE
            {
               AutoLocker al(ct_lock);
#endif
               ret = ct_exit(m_context, CS_FORCE_EXIT);
#ifdef SYBASE
            }
#endif
            assert(ret == CS_SUCCEED);
         }
#ifdef SYBASE
         AutoLocker al(cs_lock);
#endif
         ret = cs_ctx_drop(m_context);
         assert(ret == CS_SUCCEED);
      }

   public:
      DLLLOCAL context(ExceptionSink *xsink) {
         CS_RETCODE ret;

#ifdef SYBASE
         {
            AutoLocker al(cs_lock);
#endif
            ret = cs_ctx_alloc(CS_VERSION_100, &m_context);
#ifdef SYBASE
         }
#endif
         if (ret != CS_SUCCEED) {
            xsink->raiseException("TDS-CT-LIB-CANNOT-ALLOCATE-ERROR", "cs_ctx_alloc() failed with error %d", ret);
            return;
         }

#ifdef SYBASE
         {
            AutoLocker al(ct_lock);
#endif
            ret = ct_init(m_context, CS_VERSION_100);
#ifdef SYBASE
         }
#endif
         if (ret != CS_SUCCEED) {
            del();
            xsink->raiseException("TDS-CT-LIB-INIT-FAILED", "ct_init() failed with error %d", ret);
            return;
         }
         //printd(5, "context::context() this=%p m_context=%p\n", this, m_context);
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
         CS_INT olen = 0;
         CS_RETCODE ret = ct_config(m_context, CS_GET, CS_VER_STRING, buf, CLIENT_VER_LEN, &olen);
         //printd(5, "olen=%d, ret=%d\n", olen, ret);
         if (ret != CS_SUCCEED) {
            free(buf);
            xsink->raiseException("TDS-GET-CLIENT-VERSION-ERROR", "ct_config(CS_VER_STRING) failed with error %d", (int)ret);
            return 0;
         }
         //printd(5, "client version=%s (olen=%d, strlen=%d)\n", buf, olen, strlen(buf));
         return new QoreStringNode(buf, olen - 1, CLIENT_VER_LEN, QCS_DEFAULT);
      }
};

// Instantiated class is kept as private data of the Datasource
// for the time the Datasource exists. All other Sybase
// resources are shortlived (including CS_COMMAND* and its wrapper).
class connection {
private:
    context m_context;
    CS_CONNECTION* m_connection;
    bool connected;
    const QoreEncoding *enc;
    Datasource *ds;
    int numeric_support;
    const AbstractQoreZoneInfo* server_tz;

    stmt_t* stmt;

    // returns -1 if an exception was thrown, 0 if all errors were ignored
    DLLLOCAL void do_check_exception(ExceptionSink *xsink, bool check, const char *err, const char *fmt, ...);
    // returns -1 if an exception was thrown, 0 if all errors were ignored
    DLLLOCAL void do_check_exception(ExceptionSink *xsink, bool check, const char *err, QoreStringNode* estr);

    // returns 0 if reconnected without any errors, -1 if there were errors (transaction in progress, reconnect failed, etc)
    DLLLOCAL int closeAndReconnect(ExceptionSink* xsink, command& cmd, bool try_reconnect = true);

public:
    static const int OPT_NUM_OPTIMAL = 0;
    static const int OPT_NUM_STRING = 1;
    static const int OPT_NUM_NUMERIC = 2;

    DLLLOCAL connection(Datasource *n_ds, ExceptionSink *xsink);
    DLLLOCAL ~connection();

    DLLLOCAL command* setupCommand(const QoreString* cmd_text, const QoreListNode* args, bool raw, ExceptionSink* xsink);

    // to be called after the object is constructed
    // returns 0=OK, -1=error (exception raised)
    DLLLOCAL int init(const char *username, const char *password, const char *dbname,
            const char *db_encoding, const QoreEncoding *n_enc, const char *hostname,
            int port, ExceptionSink* xsink);

    // returns 0=OK, -1=error (exception raised)
    DLLLOCAL int purge_messages(ExceptionSink *xsink);
    // discard all messages
    DLLLOCAL void discard_messages();
    // returns -1
    DLLLOCAL void do_exception(ExceptionSink *xsink, const char *err, const char *fmt, ...);
    // returns 0=OK, -1=error (exception raised)
    DLLLOCAL int direct_execute(const char *sql_text, ExceptionSink *xsink);

    // returns 0=OK, -1=error (exception raised)
    DLLLOCAL int commit(ExceptionSink *xsink);
    // returns 0=OK, -1=error (exception raised)
    DLLLOCAL int rollback(ExceptionSink *xsink);

    DLLLOCAL AbstractQoreNode* execReadOutput(QoreString *cmd_text, const QoreListNode *qore_args, bool need_list, bool doBinding, bool cols, ExceptionSink* xsink);
    DLLLOCAL command::ResType readNextResult(command& cmd, bool& connection_reset, ExceptionSink* xsink);

    DLLLOCAL AbstractQoreNode *select(const QoreString *cmd, const QoreListNode *parameters, ExceptionSink *xsink);

    DLLLOCAL AbstractQoreNode *exec(const QoreString *cmd, const QoreListNode *parameters, ExceptionSink *xsink);
#ifdef _QORE_HAS_DBI_EXECRAW
    DLLLOCAL AbstractQoreNode *execRaw(const QoreString *cmd, ExceptionSink *xsink);
#endif
    DLLLOCAL AbstractQoreNode *exec_rows(const QoreString *cmd, const QoreListNode *parameters, ExceptionSink *xsink);

    // returns true if the server is still reachable on the connection, false if not
    DLLLOCAL bool ping() const;

    // invalidate / close any open statement
    DLLLOCAL void invalidateStatement() {
       if (stmt) {
	  stmt->invalidate();
	  stmt = 0;
       }
    }

    DLLLOCAL bool wasConnectionAborted() const {
       return ds->wasConnectionAborted();
    }

    DLLLOCAL void registerStatement(stmt_t* n_stmt) {
       // invalidate any existing statement
       if (stmt)
	  stmt->invalidate();
       stmt = n_stmt;
    }

#ifdef DEBUG
   DLLLOCAL void deregisterStatement(stmt_t* n_stmt) {
      assert(n_stmt == stmt);
#else
   DLLLOCAL void deregisterStatement() {
#endif
      assert(stmt);
      stmt = 0;
    }

    DLLLOCAL CS_CONNECTION* getConnection() const { return m_connection; }
    DLLLOCAL CS_CONTEXT* getContext() { return m_context.get_context(); }
    DLLLOCAL const QoreEncoding *getEncoding() const { return enc; }

    DLLLOCAL QoreStringNode *get_client_version(ExceptionSink *xsink);
    DLLLOCAL AbstractQoreNode *get_server_version(ExceptionSink *xsink);

    DLLLOCAL int setOption(const char* opt, const AbstractQoreNode* val, ExceptionSink* xsink);
    DLLLOCAL AbstractQoreNode* getOption(const char* opt);

    DLLLOCAL int getNumeric() const { return numeric_support; }

    DLLLOCAL const AbstractQoreZoneInfo* getTZ() const;
};

#endif

// EOF
