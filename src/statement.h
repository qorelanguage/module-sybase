/* -*- mode: c++; indent-tabs-mode: nil -*- */

#ifndef _QORE_SYBASE_STATEMENT_H_

#define _QORE_SYBASE_STATEMENT_H_

#include <qore/common.h>
#include <qore/QoreValue.h>
#include <qore/DBI.h>
#include <qore/QoreEncoding.h>
#include <qore/QoreString.h>
#include <qore/SQLStatement.h>
#include <qore/QoreStringNode.h>
#include <qore/AbstractPrivateData.h>
#include <qore/QoreQueue.h>
#include <qore/Datasource.h>
#include <qore/QoreListNode.h>
#include <qore/QoreHashNode.h>
#include <qore/QoreValue.h>

#include "connection.h"
#include "emptystatement.h"
#include "command.h"
#include "dbmodulewrap.h"
#include "utils.h"

namespace ss {

inline bool expect_row(command::ResType rt) {
    switch (rt) {
        case command::RES_ROW:
        case command::RES_PARAM:
            return true;
        default:
            return false;
    }
}

class Statement : public EmptyStatement {
    QoreHashNode *_execRes;

    bool has_res() {
        return _execRes ? true : false;
    }

    void set_res(QoreHashNode *res, ExceptionSink* xsink) {
        if (_execRes) _execRes->deref(xsink);
        _execRes = res;
    }

    QoreHashNode * release_res() {
        QoreHashNode *rv = _execRes;
        _execRes = 0;
        return rv;
    }

   DLLLOCAL int checkValid(ExceptionSink* xsink) {
      if (valid)
         return 0;
      xsink->raiseException("STATEMENT-CONNECTION-ERROR", "the connection was lost while the statement was in progress; it cannot be used any longer");
      return -1;
   }

    ~Statement() {}

    SafePtr<command> context;
    Placeholders placeholders;
    bool valid;
public:
    typedef connection Connection;

    Statement() : _execRes(0), valid(true) {}

    static void Delete(Statement *self, ExceptionSink* xsink) {
        if (!self) return;
        self->set_res(0, xsink);
        delete self;
    }

    DLLLOCAL void invalidate() {
        assert(valid);
        //printd(5, "Statement::invalidate() context: %p\n", &*context);
        context->cancelDisconnect();
        valid = false;
    }

    DLLLOCAL int exec(connection *conn, const QoreString *query, const QoreListNode *args, bool raw, ExceptionSink* xsink);

    DLLLOCAL bool next(SQLStatement* stmt, ExceptionSink* xsink);

    DLLLOCAL bool isValid() const {
        return valid;
    }

    QoreHashNode * fetch_row(SQLStatement* stmt, ExceptionSink* xsink) {
        return release_res();
    }

    int define(SQLStatement* stmt, ExceptionSink* xsink) {
        return 0;
    }

    QoreListNode* fetch_rows(SQLStatement* stmt, int rows, ExceptionSink* xsink) {
        if (checkValid(xsink))
           return 0;
        int r = rows;
        ReferenceHolder<QoreListNode> reslist(xsink);
        reslist = new QoreListNode;

        // next was already called once
        do {
            // according to the doc rows <= 0 means fetch all
            if (r > 0) {
                if (rows < 0)
                   break;
                rows--;
            }
            if (*xsink)
               return 0;

            ReferenceHolder<QoreHashNode> h(xsink);
            h = fetch_row(stmt, xsink);
            if (!h)
               continue;

            reslist->push(h.release());
        } while (next(stmt, xsink));
        return reslist.release();
    }

    QoreHashNode* get_output(SQLStatement* stmt, ExceptionSink* xsink) {
        if (checkValid(xsink))
           return 0;
        return context->read_cols(&placeholders, false, xsink);
    }

    int affected_rows(SQLStatement* stmt, ExceptionSink* xsink);

    QoreHashNode* fetch_columns(SQLStatement* stmt, int rows, ExceptionSink* xsink) {
        if (checkValid(xsink))
           return 0;
        return context->read_cols(0, rows, false, xsink);
    }

    int bind_placeholders(SQLStatement* stmt, const QoreListNode& l, ExceptionSink* xsink) {
        if (checkValid(xsink))
           return -1;
        placeholders.clear();
        ConstListIterator it(l);
        while (it.next()) {
            QoreValue v(it.getValue());
            QoreStringNode *s = v.get<QoreStringNode>();
            placeholders.push_back(s->getBuffer());
        }
        return 0;
    }
};

void init(qore_dbi_method_list &methods);

} // namespace ss

#endif
