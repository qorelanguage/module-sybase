#ifndef SYBASE_SRC_EMPTY_STATEMENT_H
#define SYBASE_SRC_EMPTY_STATEMENT_H

#include <iostream>

#include "qore/SQLStatement.h"
#include "qore/QoreListNode.h"

namespace ss {

class EmptyStatement {
  protected:
        void log(const char *s) {
            std::cout << s << std::endl;
        }

  public:
        int bind(SQLStatement* stmt, const QoreListNode& l, ExceptionSink* xsink) {
            log(__PRETTY_FUNCTION__);
            return 0;
        }

        int bind_placeholders(SQLStatement* stmt, const QoreListNode& l, ExceptionSink* xsink) {
            log(__PRETTY_FUNCTION__);
            return 0;
        }

        int bind_values(SQLStatement* stmt, const QoreListNode& l, ExceptionSink* xsink) {
            log(__PRETTY_FUNCTION__);
            return 0;
        }

        int exec(SQLStatement* stmt, ExceptionSink* xsink) {
            log(__PRETTY_FUNCTION__);
            return 0;
        }

        int define(SQLStatement* stmt, ExceptionSink* xsink) {
            log(__PRETTY_FUNCTION__);
            return 0;
        }

        QoreHashNode* fetch_row(SQLStatement* stmt, ExceptionSink* xsink) {
            log(__PRETTY_FUNCTION__);
            return 0;
        }

        QoreListNode* fetch_rows(SQLStatement* stmt, int rows, ExceptionSink* xsink) {
            log(__PRETTY_FUNCTION__);
            return 0;
        }

        QoreHashNode* fetch_columns(SQLStatement* stmt, int rows, ExceptionSink* xsink) {
            log(__PRETTY_FUNCTION__);
            return 0;
        }

        QoreHashNode* describe(SQLStatement* stmt, ExceptionSink* xsink) {
            log(__PRETTY_FUNCTION__);
            return 0;
        }

        bool next(SQLStatement* stmt, ExceptionSink* xsink) {
            log(__PRETTY_FUNCTION__);
            return 0;
        }

        int affected_rows(SQLStatement* stmt, ExceptionSink* xsink) {
            log(__PRETTY_FUNCTION__);
            return 0;
        }

        QoreHashNode* get_output(SQLStatement* stmt, ExceptionSink* xsink) {
            log(__PRETTY_FUNCTION__);
            return 0;
        }

        QoreHashNode* get_output_rows(SQLStatement* stmt, ExceptionSink* xsink) {
            log(__PRETTY_FUNCTION__);
            return 0;
        }
};


} // namespace ss

#endif /* SYBASE_SRC_EMPTY_STATEMENT_H */
