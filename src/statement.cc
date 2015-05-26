#include <qore/common.h>
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



#include "connection.h"
#include "emptystatement.h"
#include "command.h"

#include "dbmodulewrap.h"
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
    AbstractQoreNode *_execRes;

    AbstractQoreNode * get_res() {
        return _execRes;
    }

    void set_res(AbstractQoreNode *res, ExceptionSink* xsink) {
        if (_execRes) _execRes->deref(xsink);
        _execRes = res;
    }

    ~Statement() {}

    std::auto_ptr<command> context;
public:
    typedef connection Connection;


    Statement() : _execRes(0) { }

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
        if (xsink->isException()) return false;
        if (!context.get()) return false;
        command::ResType res = context->read_next_result(xsink);
        if (expect_row(res)) {
            return true;
        }

        return false;
    }

    QoreHashNode * fetch_row(SQLStatement* stmt, ExceptionSink* xsink) {
        if (!context.get()) return 0;
        return context->fetch_row(xsink);
    }


    int define(SQLStatement* stmt, ExceptionSink* xsink) {
        return 0;
    }


    QoreListNode* fetch_rows(SQLStatement* stmt, int rows,
            ExceptionSink* xsink)
    {
        ReferenceHolder<QoreListNode> reslist(xsink);
        reslist = new QoreListNode();

        // next was already called once
        do {
            if (xsink->isException()) return 0;
            reslist->insert(fetch_row(stmt, xsink));
        } while (next(stmt, xsink));
        return reslist.release();
    }
};

void init(qore_dbi_method_list &methods) {
    DBModuleWrap<Statement> module(methods);
    module.reg();
}

} // namespace ss
