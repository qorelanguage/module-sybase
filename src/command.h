/*
  command.h

  Sybase DB layer for QORE
  uses Sybase OpenClient C library

  Qore Programming language

  Copyright (C) 2007 Qore Technologies sro

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

#ifndef SYBASE_COMMAND_H_
#define SYBASE_COMMAND_H_

#include <ctpublic.h>

#include <memory>

#include "sybase_query.h"
#include "row_output_buffers.h"
#include "conversions.h"
#include "utils.h"

class connection;


struct CS_DATAFMT_EX : public CS_DATAFMT {
    int origin_datatype;
};

typedef std::vector<CS_DATAFMT_EX> row_result_t;


class Columns {
public:
    Columns() : dirty(true) {}

    bool dirty;
    row_result_t datafmt;

    size_t count() { return datafmt.size(); }
    bool empty() { return datafmt.empty(); }

    void reset() {
        datafmt.clear();
        dirty = true;
    }

    bool need_refresh() {
        return dirty || empty();
    }

    void set_dirty() { dirty = true; }
};


class command {
public:
    enum ResType {
        RES_NONE,
        RES_PARAM,
        RES_STATUS,
        RES_ROW,
        RES_END,
        RES_DONE,
        RES_ERROR,
        RES_RETRY,
    };


private:
    ss::SafePtr<sybase_query> query;

    connection& m_conn;
    CS_COMMAND* m_cmd;
    bool canceled;
    CS_INT rowcount;
    ResType lastRes;

    Columns colinfo;
    row_output_buffers out_buffers;

    void retr_colinfo(ExceptionSink* xsink);

    int ensure_colinfo(ExceptionSink* xsink) {
        if (!colinfo.need_refresh()) return 0;
        retr_colinfo(xsink);
        return xsink->isException();
    }

    // returns 0=OK, -1=error (exception raised)
    int get_row_description(row_result_t &result, unsigned column_count, class ExceptionSink *xsink);
    int setup_output_buffers(const row_result_t &input_row_descriptions, class ExceptionSink *xsink);

    int append_buffers_to_list(row_result_t &column_info, row_output_buffers& all_buffers, class QoreHashNode *h, ExceptionSink* xsink);

    QoreHashNode *output_buffers_to_hash(const Placeholders *ph, ExceptionSink* xsink);
    AbstractQoreNode *get_node(const CS_DATAFMT_EX& datafmt, const output_value_buffer& buffer, ExceptionSink* xsink);

    ResType read_next_result1(ExceptionSink* xsink);

public:
    ResType read_next_result(ExceptionSink* xsink) {
        if (lastRes == RES_DONE) lastRes = RES_NONE;
        if (lastRes != RES_NONE) return lastRes;
        while ((lastRes = read_next_result1(xsink)) == RES_RETRY) {}
        return lastRes;
    }

    QoreHashNode * fetch_row(ExceptionSink* xsink, const Placeholders *ph = 0);


    int get_row_count();

    DLLLOCAL command(connection& conn, ExceptionSink* xsink);
    DLLLOCAL ~command();

    DLLLOCAL CS_COMMAND* operator()() const { return m_cmd; }
    DLLLOCAL connection& getConnection() const { return m_conn; }

    DLLLOCAL void send(ExceptionSink* xsink);
    DLLLOCAL void  initiate_language_command(const char *cmd_text, class ExceptionSink *xsink);
    // returns true if data returned, false if not
    DLLLOCAL bool fetch_row_into_buffers(class ExceptionSink *xsink);
    // returns the number of columns in the result
    DLLLOCAL unsigned get_column_count(ExceptionSink *xsink);
    // returns 0=OK, -1=error (exception raised)
    DLLLOCAL void set_params(sybase_query &query, const QoreListNode *args, ExceptionSink *xsink);
    DLLLOCAL AbstractQoreNode *read_output(bool list, bool &disconnect, ExceptionSink* xsink);

    QoreHashNode *read_cols(const Placeholders *placeholder_list,
            ExceptionSink* xsink);
    AbstractQoreNode *read_rows(Placeholders *placeholder_list, bool list, ExceptionSink* xsink);
    AbstractQoreNode *read_rows(const Placeholders *placeholder_list, ExceptionSink* xsink);


    void set_placeholders(const Placeholders &ph) {
        query->placeholders = ph;
    }

    int bind_query(std::auto_ptr<sybase_query> &query,
            const QoreListNode *args,
            ExceptionSink*);
};


#endif

