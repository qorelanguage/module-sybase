#ifndef SYBASE_SRC_DBMODULEWRAP_H
#define SYBASE_SRC_DBMODULEWRAP_H

#include <assert.h>

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

#include <iostream>
#include <memory>
#include <map>

namespace ss {

template<typename Module>
class DBModuleWrap {
    class StatementHelper {
    public:
        typedef typename Module::Connection Connection;
        StatementHelper(SQLStatement* stmt) : stmt(stmt) {}

        Datasource * ds() { return stmt->getDatasource(); }
        const QoreEncoding * enc() { return ds()->getQoreEncoding(); }


        QoreString * encodeSQL(const QoreString &s, ExceptionSink* xsink) {
            return s.convertEncoding(enc(), xsink);
        }

        Connection * conn() {
            return (Connection *)ds()->getPrivateData();
        }

    private:
        SQLStatement* stmt;
    };


    class ModuleWrap {
        ~ModuleWrap() {}
    public:
        ModuleWrap() : m(new Module()), params(0) {}

        Module *m;
        std::auto_ptr<QoreString> query;
        bool raw;

        void set_params(const QoreListNode *p, ExceptionSink* xsink) {
            if (params) {
                params->deref(xsink);
                params = 0;
            }
            if (!p) return;
            params = p->listRefSelf();
        }

        const QoreListNode *get_params() {
            return params;
        }

        static void Delete(ModuleWrap *mv, ExceptionSink* xsink) {
            if (!mv) return;
            mv->set_params(0, xsink);
            Module::Delete(mv->m, xsink);
            delete mv;
        }
    private:
        QoreListNode *params;
    };


    static ModuleWrap * module_wrap(SQLStatement* stmt,
            bool create = true)
    {
        ModuleWrap * mv = (ModuleWrap *)stmt->getPrivateData();
        if (!mv) {
            if (!create) return 0;
            mv = new ModuleWrap();
            stmt->setPrivateData(mv);
        }
        return mv;
    }

    static Module * module(SQLStatement* stmt) {
        Module *rv = module_wrap(stmt)->m;
        assert(rv);
        return rv;
    }

    static int _prepare(SQLStatement* stmt,
            const QoreString& str,
            const QoreListNode* args,
            ExceptionSink* xsink)
    {
        StatementHelper sh(stmt);
        ModuleWrap *mv = module_wrap(stmt);

        mv->query.reset(sh.encodeSQL(str, xsink));
        mv->set_params(args, xsink);
        return 0;
    }

    static int prepare(SQLStatement* stmt,
            const QoreString& str,
            const QoreListNode* args,
            ExceptionSink* xsink)
    {
        module_wrap(stmt)->raw = false;
        return _prepare(stmt, str, args, xsink);
    }

    static int prepare_raw(SQLStatement* stmt,
            const QoreString& str,
            ExceptionSink* xsink)
    {
        module_wrap(stmt)->raw = true;
        return _prepare(stmt, str, 0, xsink);
    }

    static int close(SQLStatement* stmt, ExceptionSink* xsink) {
        ModuleWrap *m = module_wrap(stmt, false);
        stmt->setPrivateData(0);
        ModuleWrap::Delete(m, xsink);
        return 0;
    }


    static int bind(SQLStatement* stmt, const QoreListNode& l,
            ExceptionSink* xsink)
    {
        return module(stmt)->bind(stmt, l, xsink);
    }

    static int bind_placeholders(SQLStatement* stmt, const QoreListNode& l,
            ExceptionSink* xsink) 
    {
        return module(stmt)->bind_placeholders(stmt, l, xsink);
    }

    static int bind_values(SQLStatement* stmt, const QoreListNode& l,
            ExceptionSink* xsink)
    {
        return module(stmt)->bind_values(stmt, l, xsink);
    }

    static int exec(SQLStatement* stmt, ExceptionSink* xsink) {
        ModuleWrap *mv = module_wrap(stmt);
        StatementHelper sh(stmt);
        typename Module::Connection *conn = sh.conn();
        const QoreListNode *args = mv->get_params();
        const QoreString *query = mv->query.get();
        return module(stmt)->exec(conn, query, args, mv->raw, xsink);
    }

    static int define(SQLStatement* stmt, ExceptionSink* xsink) {
        return module(stmt)->define(stmt, xsink);
    }

    static QoreHashNode* fetch_row(SQLStatement* stmt, ExceptionSink* xsink) {
        return module(stmt)->fetch_row(stmt, xsink);
    }

    static QoreListNode* fetch_rows(SQLStatement* stmt, int rows,
            ExceptionSink* xsink)
    {
        return module(stmt)->fetch_rows(stmt, rows, xsink);
    }

    static QoreHashNode* fetch_columns(SQLStatement* stmt, int rows,
            ExceptionSink* xsink)
    {
        return module(stmt)->fetch_columns(stmt, rows, xsink);
    }

    static QoreHashNode* describe(SQLStatement* stmt, ExceptionSink* xsink) {
        return module(stmt)->describe(stmt, xsink);
    }

    static bool next(SQLStatement* stmt, ExceptionSink* xsink) {
        return module(stmt)->next(stmt, xsink);
    }

    static int affected_rows(SQLStatement* stmt, ExceptionSink* xsink) {
        return module(stmt)->affected_rows(stmt, xsink);
    }

    static QoreHashNode* get_output(SQLStatement* stmt, ExceptionSink* xsink) {
        return module(stmt)->get_output(stmt, xsink);
    }

    static QoreHashNode* get_output_rows(SQLStatement* stmt,
            ExceptionSink* xsink)
    {
        return module(stmt)->get_output_rows(stmt, xsink);
    }


    qore_dbi_method_list &methods;

  public:
    DBModuleWrap(qore_dbi_method_list &methods) :
        methods(methods)
    {}

    void reg() {
        methods.add(QDBI_METHOD_STMT_PREPARE, prepare);
        methods.add(QDBI_METHOD_STMT_PREPARE_RAW, prepare_raw);
        methods.add(QDBI_METHOD_STMT_CLOSE, close);
        methods.add(QDBI_METHOD_STMT_BIND, bind);
        methods.add(QDBI_METHOD_STMT_BIND_PLACEHOLDERS, bind_placeholders);
        methods.add(QDBI_METHOD_STMT_BIND_VALUES, bind_values);
        methods.add(QDBI_METHOD_STMT_EXEC, exec);
        methods.add(QDBI_METHOD_STMT_DEFINE, define);
        methods.add(QDBI_METHOD_STMT_FETCH_ROW, fetch_row);
        methods.add(QDBI_METHOD_STMT_FETCH_ROWS, fetch_rows);
        methods.add(QDBI_METHOD_STMT_FETCH_COLUMNS, fetch_columns);
        methods.add(QDBI_METHOD_STMT_DESCRIBE, describe);
        methods.add(QDBI_METHOD_STMT_NEXT, next);
        methods.add(QDBI_METHOD_STMT_AFFECTED_ROWS, affected_rows);
        methods.add(QDBI_METHOD_STMT_GET_OUTPUT, get_output);
        methods.add(QDBI_METHOD_STMT_GET_OUTPUT_ROWS, get_output_rows);
    }
};

} // namespace ss

#endif /* SYBASE_SRC_DBMODULEWRAP_H */
