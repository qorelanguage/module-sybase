/*
  sybase_connection.cpp

  Sybase DB layer for QORE
  uses Sybase OpenClient C library or FreeTDS' ct-lib

  Qore Programming Language

  Copyright (C) 2007 - 2015 Qore Technolgoies sro

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

#include <assert.h>
#include <memory>

#include <ctpublic.h>

#include "connection.h"
#include "encoding_helpers.h"
#include "sybase_query.h"
#include "command.h"

static QoreString ver_str("begin tran select @@version commit tran");

#ifdef SYBASE
// to serialize calls to ct_init() and ct_exit()
QoreThreadLock ct_lock;
QoreThreadLock cs_lock;
#endif

connection::connection(Datasource *n_ds, ExceptionSink *xsink)
   : m_context(xsink),
     m_connection(0),
     connected(false),
     ds(n_ds),
     numeric_support(OPT_NUM_OPTIMAL),
     server_tz(0)
{}

connection::~connection() {
   CS_RETCODE ret = CS_SUCCEED;

   if (m_connection) {
      if (connected) {
         ret = ct_close(m_connection, CS_UNUSED);
         if (ret != CS_SUCCEED) {
            ret = ct_close(m_connection, CS_FORCE_CLOSE);
            assert(ret == CS_SUCCEED);
         }
      }
      ret = ct_con_drop(m_connection);
      assert(ret == CS_SUCCEED);
   }
}

// FIXME: check for auto-reconnect here if ct_command fails
int connection::direct_execute(const char* sql_text, ExceptionSink* xsink) {
   assert(sql_text && sql_text[0]);

   CS_COMMAND* cmd = 0;

   CS_RETCODE err = ct_cmd_alloc(m_connection, &cmd);
   if (err != CS_SUCCEED)
      do_exception(xsink, "DBI:SYBASE:ERROR", "ct_cmd_alloc() failed");

   ON_BLOCK_EXIT(ct_cmd_drop, cmd);
   ScopeGuard canceller = MakeGuard(ct_cancel, (CS_CONNECTION*)0, cmd, CS_CANCEL_ALL);

   err = ct_command(cmd, CS_LANG_CMD, (CS_CHAR*)sql_text, strlen(sql_text), CS_END);
   if (err != CS_SUCCEED)
      do_exception(xsink, "DBI-EXEC-EXCEPTION", "ct_command() failed");

   err = ct_send(cmd);
   if (err != CS_SUCCEED)
      do_exception(xsink, "DBI-EXEC-EXCEPTION", "ct_send() failed");

   // no results expected
   CS_INT result_type;
   err = ct_results(cmd, &result_type);
   if (err != CS_SUCCEED)
        do_exception(xsink, "DBI:SYBASE:EXEC-ERROR",
                "connection::direct_execute(): ct_results()"
                " returned error code %d", err);

   if (result_type != CS_CMD_SUCCEED)
       do_check_exception(xsink, true, "DBI:SYBASE:EXEC-ERROR",
               "connection::direct_execute(): ct_results()"
               " failed with result_type = %d", result_type);

   while ((err = ct_results(cmd, &result_type)) == CS_SUCCEED);
   canceller.Dismiss();

   return purge_messages(xsink);
}

static inline bool wasInTransaction(Datasource *ds) {
#ifdef _QORE_HAS_DATASOURCE_ACTIVETRANSACTION
   return ds->activeTransaction();
#else
   return ds->isInTransaction();
#endif
}

command * connection::create_command(const QoreString *cmd_text,
        const QoreListNode *qore_args,
        ExceptionSink* xsink)
{
    std::auto_ptr<sybase_query> query(new sybase_query());

    if (query->init(cmd_text, qore_args, xsink))
        return 0;

    std::auto_ptr<command> cmd(new command(*this, xsink));
    cmd->bind_query(query, qore_args, xsink);
    cmd->send(xsink);
    return cmd.release();
}


command * connection::create_command(const QoreString *cmd_text,
        ExceptionSink* xsink)
{
    std::auto_ptr<sybase_query> query(new sybase_query());

    if (query->init(cmd_text)) return 0;

    std::auto_ptr<command> cmd(new command(*this, xsink));
    cmd->bind_query(query, 0, xsink);
    cmd->send(xsink);
    return cmd.release();
}

AbstractQoreNode *connection::exec_intern(QoreString *cmd_text, const QoreListNode *qore_args, bool need_list, ExceptionSink* xsink, bool doBinding) {
    std::auto_ptr<command> cmd;
    if (doBinding)
        cmd.reset(create_command(cmd_text, qore_args, xsink));
    else
        cmd.reset(create_command(cmd_text, xsink));

    while (true) {
        if (*xsink)
            return 0;

        bool disconnect = false;

        ReferenceHolder<AbstractQoreNode>
            result(cmd->read_output(need_list, disconnect, xsink), xsink);

        if (*xsink)
            return 0;

        if (!disconnect)
            return result.release();

        // see if we need to reconnect and try again
        ct_close(m_connection, CS_FORCE_CLOSE);
        connected = false;

        // discard all current messages
        discard_messages();

        if (wasInTransaction(ds))
            xsink->raiseException("DBI:SYBASE:TRANSACTION-ERROR",
                    "connection to server lost while in a transaction; transaction has been lost");

        // otherwise try to reconnect
        ct_con_drop(m_connection);
        m_connection = 0;

#ifdef QORE_HAS_DATASOURCE_PORT
        int port = ds->getPort();
#else
        int port = 0;
#endif

        // make the actual connection to the database
        init(ds->getUsername(), ds->getPassword() ? ds->getPassword() : "", ds->getDBName(), ds->getDBEncoding(), ds->getQoreEncoding(), ds->getHostName(), port, xsink);
        // return with an error if it didn't work
        if (*xsink) {
	   // make sure and mark Datasource as closed
	   ds->connectionAborted();
	   return 0;
        }

        // if the connection was aborted while in a transaction, return now
        if (wasInTransaction(ds))
            return 0;

#ifdef DEBUG
	// otherwise show the exception on stdout in debug mode
	xsink->handleExceptions();
#endif
	// clear any exceptions that have been ignored
	xsink->clear();

        printd(5, "connection::exec_intern() this=%p auto reconnected to %s@%s\n", this, ds->getUsername(), ds->getDBName());
    }

    // to avoid warnings
    return 0;
}

AbstractQoreNode *connection::exec(const QoreString *cmd, const QoreListNode *parameters, ExceptionSink *xsink) {
   // copy the string here for intrusive editing, convert encoding too if necessary
   QoreString *query = cmd->convertEncoding(enc, xsink);
   if (!query)
      return 0;

   std::auto_ptr<QoreString> tmp(query);
   ReferenceHolder<> rv(exec_intern(query, parameters, false, xsink), xsink);
   purge_messages(xsink);
   return rv.release();
}

#ifdef _QORE_HAS_DBI_EXECRAW
AbstractQoreNode *connection::execRaw(const QoreString *cmd, ExceptionSink *xsink) {
   // copy the string here for intrusive editing, convert encoding too if necessary
   QoreString *query = cmd->convertEncoding(enc, xsink);
   if (!query)
      return 0;

   std::auto_ptr<QoreString> tmp(query);
   ReferenceHolder<> rv(exec_intern(query, 0, false, xsink, false), xsink);
   purge_messages(xsink);
   return rv.release();
}
#endif

AbstractQoreNode *connection::exec_rows(const QoreString *cmd, const QoreListNode *parameters, ExceptionSink *xsink) {
   // copy the string here for intrusive editing, convert encoding too if necessary
   QoreString *query = cmd->convertEncoding(enc, xsink);
   if (!query)
      return 0;

   std::auto_ptr<QoreString> tmp(query);
   ReferenceHolder<> rv(exec_intern(query, parameters, true, xsink), xsink);
   purge_messages(xsink);
   return rv.release();
}

// returns 0=OK, -1=error (exception raised)
int connection::commit(ExceptionSink *xsink) {
   // first clear any pending results in case a statement was in progress (does not clear any actions already effected with exec())
   ct_cancel(m_connection, 0, CS_CANCEL_ALL);
   return direct_execute("commit", xsink);
}

// returns 0=OK, -1=error (exception raised)
int connection::rollback(ExceptionSink *xsink) {
   // first clear any pending results in case a statement was in progress
   ct_cancel(m_connection, 0, CS_CANCEL_ALL);
   return direct_execute("rollback", xsink);
}

// Post-constructor initialization
int connection::init(const char* username,
        const char* password,
        const char* dbname,
        const char* db_encoding,
        const QoreEncoding *n_enc,
        const char *hostname,
        int port,
        ExceptionSink* xsink) {
   assert(!m_connection);

   printd(5, "connection::init() user=%s pass=%s dbname=%s, db_enc=%s\n", username, password ? password : "<n/a>", dbname, db_encoding ? db_encoding : "<n/a>");

   enc = n_enc;

/*
  // add callbacks
  ret = ct_callback(m_context, 0, CS_SET, CS_CLIENTMSG_CB, (CS_VOID*)clientmsg_callback);
  if (ret != CS_SUCCEED) {
    xsink->raiseException("DBI:SYBASE:CTLIB-SET-CALLBACK", "ct_callback(CS_CLIENTMSG_CB) failed with error %d", ret);
    return -1;
  }
  ret = ct_callback(m_context, 0, CS_SET, CS_SERVERMSG_CB, (CS_VOID*)servermsg_callback);
  if (ret != CS_SUCCEED) {
    xsink->raiseException("DBI:SYBASE:CTLIB-SET-CALLBACK", "ct_callback(CS_SERVERMSG_CB) failed with error %d", ret);
    return -1;
  }
*/
   CS_RETCODE ret = ct_con_alloc(m_context.get_context(), &m_connection);
   if (ret != CS_SUCCEED) {
      xsink->raiseException("DBI:SYBASE:CTLIB-CREATE-CONNECTION", "ct_con_alloc() failed with error %d", ret);
      return -1;
   }

   // set inline message handling
   ret = ct_diag(m_connection, CS_INIT, CS_UNUSED, CS_UNUSED, 0);
   if (ret != CS_SUCCEED) {
      xsink->raiseException("DBI:SYBASE:CTLIB-INIT-ERROR-HANDLING-ERROR", "ct_diag(CS_INIT) failed with error %d, unable to initialize inline error handling", ret);
      return -1;
   }

   ret = ct_con_props(m_connection, CS_SET, CS_USERNAME, (CS_VOID*)username, CS_NULLTERM, 0);
   if (ret != CS_SUCCEED) {
      xsink->raiseException("DBI:SYBASE:CTLIB-SET-USERNAME", "ct_con_props(CS_USERNAME) failed with error %d", ret);
      return -1;
   }
   if (password && password[0]) {
      ret = ct_con_props(m_connection, CS_SET, CS_PASSWORD, (CS_VOID*)password, CS_NULLTERM, 0);
      if (ret != CS_SUCCEED) {
         xsink->raiseException("DBI:SYBASE:CTLIB-SET-PASSWORD", "ct_con_props(CS_PASSWORD) failed with error %d", ret);
         return -1;
      }
   }

   // WARNING: seems to only work for freetds, although this is the documented format for Sybase 12.5 - 15
   // set hostname and port
   if (hostname && port) {
      QoreString hn(hostname);
      hn.sprintf(" %d", port);

      ret = ct_con_props(m_connection, CS_SET, CS_SERVERADDR, (CS_VOID*)hn.getBuffer(), CS_NULLTERM, 0);
      if (ret != CS_SUCCEED) {
         xsink->raiseException("DBI:SYBASE:CTLIB-SET-SERVERADDR", "ct_con_props(CS_SERVERADDR, '%s') failed with error %d", hn.getBuffer(), ret);
         return -1;
      }
   }

#if defined(SYBASE) || defined(FREETDS_LOCALE)
   CS_LOCALE *m_charset_locale = 0;

   ret = cs_loc_alloc(m_context.get_context(), &m_charset_locale);
   if (ret != CS_SUCCEED) {
      xsink->raiseException("DBI-EXEC-EXCEPTION", "cs_loc_alloc() returned error %d", (int)ret);
      return -1;
   }
   ret = cs_locale(m_context.get_context(), CS_SET, m_charset_locale, CS_LC_ALL, 0, CS_NULLTERM, 0);
   if (ret != CS_SUCCEED) {
      xsink->raiseException("DBI-EXEC-EXCEPTION", "cs_locale(CS_LC_ALL) returned error %d", (int)ret);
      return -1;
   }
   ret = cs_locale(m_context.get_context(), CS_SET, m_charset_locale, CS_SYB_CHARSET, (CS_CHAR*)db_encoding, CS_NULLTERM, 0);
   if (ret != CS_SUCCEED) {
      xsink->raiseException("DBI-EXEC-EXCEPTION", "cs_locale(CS_SYB_CHARSET, '%s') failed with error %d", db_encoding, (int)ret);
      return -1;
   }
   ret = ct_con_props(m_connection, CS_SET, CS_LOC_PROP, m_charset_locale, CS_UNUSED, 0);
   if (ret !=CS_SUCCEED) {
      xsink->raiseException("DBI-EXEC-EXCEPTION", "ct_con_props(CS_SET, CS_LOC_PROP) failed with error %d", (int)ret);
      return -1;
   }

   ret = cs_loc_drop(m_context.get_context(), m_charset_locale);
   assert(ret == CS_SUCCEED);
#endif

   ret = ct_connect(m_connection, (CS_CHAR*)dbname,  strlen(dbname));
   if (ret != CS_SUCCEED) {
      do_exception(xsink, "DBI:SYBASE:CTLIB-CONNECT", "ct_connect() failed with error %d", ret);
      //xsink->raiseException("DBI:SYBASE:CTLIB-CONNECT", "ct_connect() failed with error %d", ret);
      return -1;
   }
   connected = true;

   // turn on chained transaction mode, this fits with Qore's transaction management approach
   // - in autocommit mode qore executes a commit after every request manually
   CS_BOOL cs_bool = CS_TRUE;
   ret = ct_options(m_connection, CS_SET, CS_OPT_CHAINXACTS, &cs_bool, CS_UNUSED, 0);
   if (ret != CS_SUCCEED)
      do_exception(xsink, "DBI:SYBASE:INIT-ERROR", "ct_options(CS_OPT_CHAINXACTS) failed");

   // Set default type of string representation of DATETIME to long (like Jan 1 1990 12:32:55:0000 PM)
   // Without this some routines in conversions.cpp would fail.
   CS_INT aux = CS_DATES_LONG;
   ret = cs_dt_info(m_context.get_context(), CS_SET, NULL, CS_DT_CONVFMT, CS_UNUSED, (CS_VOID*)&aux, sizeof(aux), 0);
   if (ret != CS_SUCCEED)
      do_exception(xsink, "DBI:SYBASE:INIT-ERROR", "cs_dt_info(CS_DT_CONVFMT) failed");

   return purge_messages(xsink);
}

void connection::discard_messages() {
#ifdef DEBUG
   CS_RETCODE ret =
#endif
   ct_diag(m_connection, CS_CLEAR, CS_ALLMSG_TYPE, CS_UNUSED, 0);
   assert(ret == CS_SUCCEED);

}

// purges all outstanding messages using ct_diag
int connection::purge_messages(ExceptionSink *xsink) {
   int rc = 0;
   // make sure no messages have severity > 10
   int num;
   CS_RETCODE ret = ct_diag(m_connection, CS_STATUS, CS_CLIENTMSG_TYPE, CS_UNUSED, &num);
   assert(ret == CS_SUCCEED);
   CS_CLIENTMSG cmsg;
   for (int i = 1; i <= num; ++i) {
      ret = ct_diag(m_connection, CS_GET, CS_CLIENTMSG_TYPE, i, &cmsg);
      assert(ret == CS_SUCCEED);
      if (CS_SEVERITY(cmsg.msgnumber) > 10) {
         QoreStringNode *desc = new QoreStringNode;
         desc->sprintf("client message %d, severity %d: %s", CS_NUMBER(cmsg.msgnumber), CS_SEVERITY(cmsg.msgnumber), cmsg.msgstring);
         if (cmsg.osstringlen)
            desc->sprintf(" (%d): '%s'", cmsg.osnumber, cmsg.osstring);
         xsink->raiseException("DBI:SYBASE:CLIENT-ERROR", desc);
         rc = -1;
      }
#ifdef DEBUG
      printd(1, "client: severity:%d, n:%d: %s\n", CS_SEVERITY(cmsg.msgnumber), CS_NUMBER(cmsg.msgnumber), cmsg.msgstring);
      if (cmsg.osstringlen > 0)
         printd(1, "Operating System Error: %s\n", cmsg.osstring);
#endif
   }
   ret = ct_diag(m_connection, CS_STATUS, CS_SERVERMSG_TYPE, CS_UNUSED, &num);
   assert(ret == CS_SUCCEED);
   CS_SERVERMSG smsg;
   for (int i = 1; i <= num; ++i) {
      ret = ct_diag(m_connection, CS_GET, CS_SERVERMSG_TYPE, i, &smsg);
      assert(ret == CS_SUCCEED);
      if (smsg.severity > 10) {
         QoreStringNode *desc = new QoreStringNode;
         desc->sprintf("state %d, server message %d, ", smsg.state, smsg.msgnumber);
         if (smsg.line)
            desc->sprintf("line %d, ", smsg.line);
         desc->sprintf("severity %ld: %s", smsg.severity, smsg.text);
         desc->trim_trailing('\n');
         xsink->raiseException("DBI:SYBASE:SERVER-ERROR", desc);
         rc = -1;
      }
      printd(1, "server: line:%ld, severity:%ld, n:%d: %s", smsg.line, smsg.severity, smsg.msgnumber, smsg.text);
   }
   ret = ct_diag(m_connection, CS_CLEAR, CS_ALLMSG_TYPE, CS_UNUSED, 0);
   assert(ret == CS_SUCCEED);
   return rc;
}

// if check = true, ignore server messages:
// 3902: The COMMIT TRANSACTION request has no corresponding BEGIN TRANSACTION
// 3903: The ROLLBACK TRANSACTION request has no corresponding BEGIN TRANSACTION
void connection::do_check_exception(ExceptionSink *xsink, bool check, const char *err, QoreStringNode* estr) {
   int count = 0;
   int num;
   CS_RETCODE ret = ct_diag(m_connection, CS_STATUS, CS_CLIENTMSG_TYPE, CS_UNUSED, &num);
   assert(ret == CS_SUCCEED);
   CS_CLIENTMSG cmsg;
   for (int i = 1; i <= num; ++i) {
      ret = ct_diag(m_connection, CS_GET, CS_CLIENTMSG_TYPE, i, &cmsg);
      if (ret != CS_SUCCEED)
         continue;
      int severity = CS_SEVERITY(cmsg.msgnumber);
      if (severity <= 10)
         continue;

      if (count)
         estr->concat(", ");
      estr->sprintf("client message %d: severity %d: %s", CS_NUMBER(cmsg.msgnumber), severity, cmsg.msgstring);
      estr->trim_trailing('.');
      if (cmsg.osnumber && cmsg.osstringlen > 0)
         estr->sprintf(", OS error %d: %s", cmsg.osnumber, cmsg.osstring);
      count++;
   }

   ret = ct_diag(m_connection, CS_STATUS, CS_SERVERMSG_TYPE, CS_UNUSED, &num);
   assert(ret == CS_SUCCEED);
   CS_SERVERMSG smsg;

   bool fnd_ignore = !check;

   for (int i = 1; i <= num; ++i) {
      ret = ct_diag(m_connection, CS_GET, CS_SERVERMSG_TYPE, i, &smsg);
      if (!fnd_ignore && ret == CS_SUCCEED && (smsg.msgnumber == 3902 || smsg.msgnumber == 3903))
	 fnd_ignore = true;

      if (ret != CS_SUCCEED || smsg.severity <= 10)
         continue;

      if (count)
         estr->concat(", ");
      if (smsg.svrnlen)
         estr->sprintf("%s: ", smsg.svrname);
      estr->sprintf("state %d, server message %d, ", smsg.state, smsg.msgnumber);
      if (smsg.line)
         estr->sprintf("line %d, ", smsg.line);
      estr->sprintf("severity %d", smsg.severity);
      if (smsg.textlen)
         estr->sprintf(": %s", smsg.text);
      estr->trim_trailing("\n.");
      ++count;
   }
   ret = ct_diag(m_connection, CS_CLEAR, CS_ALLMSG_TYPE, CS_UNUSED, 0);
   assert(ret == CS_SUCCEED);
   if (check && fnd_ignore && num == 1)
      return;

   throw ss::Error(err, estr->getBuffer());
}

void connection::do_check_exception(ExceptionSink *xsink, bool check, const char *err, const char *fmt, ...) {
   SimpleRefHolder<QoreStringNode> estr(new QoreStringNode);
   va_list args;
   while (fmt) {
      va_start(args, fmt);
      int rc = estr->vsprintf(fmt, args);
      va_end(args);
      if (!rc) {
         estr->concat(": ");
         break;
      }
   }
   do_check_exception(xsink, check, err, *estr);
}

void connection::do_exception(ExceptionSink *xsink, const char *err, const char *fmt, ...) {
   SimpleRefHolder<QoreStringNode> estr(new QoreStringNode);
   va_list args;
   while (fmt) {
      va_start(args, fmt);
      int rc = estr->vsprintf(fmt, args);
      va_end(args);
      if (!rc) {
         estr->concat(": ");
         break;
      }
   }
   do_check_exception(xsink, false, err, *estr);
}

/*
CS_RETCODE connection::clientmsg_callback(CS_CONTEXT* ctx, CS_CONNECTION* conn, CS_CLIENTMSG* errmsg)
{
#ifdef DEBUG
  // This will print out description about an Sybase error. Most of the information
  // can be ignored but if the application crashes the last error may be very informative.
  // Comment it out if this output is not required.
  if ((CS_NUMBER(errmsg->msgnumber) == 211) || (CS_NUMBER(errmsg->msgnumber) == 212)) { // acc. to the docs
    return CS_SUCCEED;
  }
  fprintf(stderr, "-------------------------------------------------------------");
  fprintf(stderr, "\nOpen Client Message:\n");
  fprintf(stderr, "Message number: LAYER = (%d) ORIGIN = (%d) ",
    (int)CS_LAYER(errmsg->msgnumber), (int)CS_ORIGIN(errmsg->msgnumber));
  fprintf(stderr, "SEVERITY = (%d) NUMBER = (%d)\n",
    (int)CS_SEVERITY(errmsg->msgnumber), (int)CS_NUMBER(errmsg->msgnumber));
  fprintf(stderr, "Message String: %s\n", errmsg->msgstring);
  if (errmsg->osstringlen > 0) {
    fprintf(stderr, "Operating System Error: %s\n", errmsg->osstring);
  }
  fprintf(stderr, "--------------------------------------------------\n");
  fflush(stderr);
#endif
  return CS_SUCCEED;
}

CS_RETCODE connection::servermsg_callback(CS_CONTEXT* ctx, CS_CONNECTION* conn, CS_SERVERMSG* svrmsg)
{
#ifdef DEBUG
  // This will print out description about an Sybase error. Most of the information
  // can be ignored but if the application crashes the last error may be very informative.
  // Comment it out if this output is not required.
  fprintf(stderr, "-------------------------------------------------------------");
  fprintf(stderr, "\nOpen Server Message:\n");
  fprintf(stderr, "Message number = %d, severity = %d\n", (int)svrmsg->msgnumber, (int)svrmsg->severity);
  fprintf(stderr, "State = %d, line = %d\n", (int)svrmsg->state, (int)svrmsg->line);
  if (svrmsg->svrnlen) {
    fprintf(stderr, "Server: %s\n", svrmsg->svrname);
  }
  if (svrmsg->proclen) {
    fprintf(stderr, "Procedure: %s\n", svrmsg->proc);
  }
  fprintf(stderr, "Message string: %s\n", svrmsg->text);
  fprintf(stderr, "--------------------------------------------------\n");
  fflush(stderr);
#endif
  return CS_SUCCEED;
}
*/

// get client version
QoreStringNode *connection::get_client_version(ExceptionSink *xsink) {
   return m_context.get_client_version(xsink);
}

AbstractQoreNode *connection::get_server_version(ExceptionSink *xsink) {
   AbstractQoreNode *res = exec_intern(&ver_str, 0, true, xsink);
   if (!res)
      return 0;
   assert(res->getType() == NT_HASH);
   HashIterator hi(reinterpret_cast<QoreHashNode *>(res));
   hi.next();
   AbstractQoreNode *rv = hi.takeValueAndDelete();
   res->deref(xsink);

   QoreStringNode *str = dynamic_cast<QoreStringNode *>(rv);
   if (str)
      str->trim_trailing('\n');

   return rv;
}

DLLLOCAL int connection::setOption(const char* opt, const AbstractQoreNode* val, ExceptionSink* xsink) {
    if (!strcasecmp(opt, DBI_OPT_NUMBER_OPT)) {
        numeric_support = OPT_NUM_OPTIMAL;
        return 0;
    }

    if (!strcasecmp(opt, DBI_OPT_NUMBER_STRING)) {
        numeric_support = OPT_NUM_STRING;
        return 0;
    }

    if (!strcasecmp(opt, DBI_OPT_NUMBER_NUMERIC)) {
        numeric_support = OPT_NUM_NUMERIC;
        return 0;
    }

    if (!strcasecmp(opt, DBI_OPT_TIMEZONE)) {
        assert(get_node_type(val) == NT_STRING);
        const QoreStringNode* str =
            reinterpret_cast<const QoreStringNode*>(val);
        const AbstractQoreZoneInfo* tz =
            find_create_timezone(str->getBuffer(), xsink);
        if (*xsink) return -1;
        server_tz = tz;
        return 0;
    }

    assert(false);
    return 0;
}

DLLLOCAL AbstractQoreNode* connection::getOption(const char* opt) {
    if (!strcasecmp(opt, DBI_OPT_NUMBER_OPT)) {
        return get_bool_node(numeric_support == OPT_NUM_OPTIMAL);
    }

    if (!strcasecmp(opt, DBI_OPT_NUMBER_STRING)) {
        return get_bool_node(numeric_support == OPT_NUM_STRING);
    }

    if (!strcasecmp(opt, DBI_OPT_NUMBER_NUMERIC)) {
        return get_bool_node(numeric_support == OPT_NUM_NUMERIC);
    }

    if (!strcasecmp(opt, DBI_OPT_TIMEZONE)) {
        return new QoreStringNode(tz_get_region_name(getTZ()));
    }

    assert(false);
    return 0;
}

DLLLOCAL const AbstractQoreZoneInfo* connection::getTZ() const {
    if (server_tz) return server_tz;
    return currentTZ();
}
