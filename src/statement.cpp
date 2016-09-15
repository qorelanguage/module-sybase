/* -*- indent-tabs-mode: nil -*- */

#include "statement.h"

int ss::Statement::affected_rows(SQLStatement* stmt, ExceptionSink* xsink) {
   if (checkValid(xsink))
      return 0;
   return context->get_row_count();
}

int ss::Statement::exec(connection *conn, const QoreString *query, const QoreListNode *args, bool raw, ExceptionSink* xsink) {
   if (checkValid(xsink))
      return -1;
   if (context.get())
      context->cancel();

   context.reset(conn->setupCommand(query, args, raw, xsink));
   if (*xsink)
      return -1;

   bool connection_reset = false;
   // not sure what to do with the return value here
   conn->readNextResult(*context.get(), connection_reset, xsink);
   return *xsink ? -1 : 0;
}

bool ss::Statement::next(SQLStatement* stmt, ExceptionSink* xsink) {
   if (checkValid(xsink))
      return false;
   connection* conn = (connection*)stmt->getDatasource()->getPrivateData();

   bool connection_reset = false;
   command::ResType res = conn->readNextResult(*context.get(), connection_reset, xsink);
   if (*xsink)
      return false;
   assert(!connection_reset);

   //command::ResType res = context->read_next_result(xsink);
   if (expect_row(res)) {
      if (!has_res()) {
         set_res(context->fetch_row(xsink), xsink);
      }
   }

   return has_res();
}

void ss::init(qore_dbi_method_list &methods) {
    DBModuleWrap<Statement> module(methods);
    module.reg();
}
