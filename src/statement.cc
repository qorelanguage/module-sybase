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

    ~Statement() {}

    SafePtr<command> context;
    Placeholders placeholders;
public:
    typedef connection Connection;

    Statement() : _execRes(0) {}

    static void Delete(Statement *self, ExceptionSink* xsink) {
        if (!self) return;
        self->set_res(0, xsink);
        delete self;
    }

    int exec(connection *conn,
            const QoreString *query,
            const QoreListNode *args,
            bool raw,
            ExceptionSink* xsink)
    {
#ifdef _QORE_HAS_DBI_EXECRAW
        if (raw) {
            context.reset(conn->create_command(query, xsink));
        } else
#endif
        {
            context.reset(conn->create_command(query, args, xsink));
        }
        return 0;
    }

    bool next(SQLStatement* stmt, ExceptionSink* xsink) {
        command::ResType res = context->read_next_result(xsink);
        if (expect_row(res)) {
            if (!has_res()) {
                set_res(context->fetch_row(xsink), xsink);
            }
        }

        return has_res();
    }

    QoreHashNode * fetch_row(SQLStatement* stmt, ExceptionSink* xsink) {
        return  release_res();
    }

    int define(SQLStatement* stmt, ExceptionSink* xsink) {
        return 0;
    }


    QoreListNode* fetch_rows(SQLStatement* stmt, int rows,
            ExceptionSink* xsink)
    {
        int r = rows;
        ReferenceHolder<QoreListNode> reslist(xsink);
        reslist = new QoreListNode();

        // next was already called once
        do {
            // acording the doc rows <=0 means fetch all
            if (r > 0) {
                if (rows <= 0) break;
                rows--;
            }
            if (xsink->isException()) return 0;
            ReferenceHolder<QoreHashNode> h(xsink);
            h = fetch_row(stmt, xsink);
            if (!h) continue;
            reslist->insert(h.release());
        } while (next(stmt, xsink));
        return reslist.release();
    }

    QoreHashNode* get_output(SQLStatement* stmt, ExceptionSink* xsink) {
        return context->read_cols(&placeholders, xsink);
    }


    int affected_rows(SQLStatement* stmt, ExceptionSink* xsink) {
        return context->get_row_count();
    }


    QoreHashNode* fetch_columns(SQLStatement* stmt, int rows, ExceptionSink* xsink) {
        return context->read_cols(0,  xsink);
    }

    int bind_placeholders(SQLStatement* stmt,
            const QoreListNode& l,
            ExceptionSink* xsink)
    {
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

void init(qore_dbi_method_list &methods) {
    DBModuleWrap<Statement> module(methods);
    module.reg();
}

} // namespace ss
