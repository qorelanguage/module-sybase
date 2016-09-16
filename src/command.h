/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  command.h

  Sybase DB layer for QORE
  uses Sybase OpenClient C library

  Qore Programming language

  Copyright (C) 2007 - 2016 Qore Technologies s.r.o.

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
        RES_CANCELED,
    };

private:
    ss::SafePtr<sybase_query> query;

    connection& m_conn;
    CS_COMMAND* m_cmd;
    CS_INT rowcount;
    ResType lastRes;

    Columns colinfo;
    row_output_buffers out_buffers;

    DLLLOCAL int retr_colinfo(ExceptionSink* xsink);

    DLLLOCAL int ensure_colinfo(ExceptionSink* xsink) {
        if (!colinfo.need_refresh())
           return 0;
        return retr_colinfo(xsink);
    }

    // returns 0=OK, -1=error (exception raised)
    DLLLOCAL int get_row_description(row_result_t &result, unsigned column_count, class ExceptionSink *xsink);
    DLLLOCAL int setup_output_buffers(const row_result_t &input_row_descriptions, class ExceptionSink *xsink);

    DLLLOCAL int append_buffers_to_list(row_result_t &column_info, row_output_buffers& all_buffers, class QoreHashNode *h, ExceptionSink* xsink);

    DLLLOCAL QoreHashNode *output_buffers_to_hash(const Placeholders *ph, ExceptionSink* xsink);
    DLLLOCAL AbstractQoreNode *get_node(const CS_DATAFMT_EX& datafmt, const output_value_buffer& buffer, ExceptionSink* xsink);

    // call ct_result() once. Takes care of return value
    DLLLOCAL ResType read_next_result1(bool& disconnect, ExceptionSink* xsink);

    // returns -1 if the cancel failed and the connection should be disconnected, 0 = OK
    DLLLOCAL int cancelIntern() {
       assert(m_cmd);
       return ct_cancel(0, m_cmd, CS_CANCEL_ALL) == CS_FAIL ? -1 : 0;
    }

   DLLLOCAL AbstractQoreNode* getNumber(const char* str, size_t len);

   DLLLOCAL void setupColumns(QoreHashNode& h, const Placeholders *ph);

public:
    DLLLOCAL ResType read_next_result(bool& disconnect, ExceptionSink* xsink) {
        /**
           Can't ask for result if rastRes==:
           * STATUS, PARAM or ROW - must ct_fetch() the values first
           * END or ERROR - reasonable, we're finished
           * RETRY - never gets here
           */
       while ((lastRes = read_next_result1(disconnect, xsink)) == RES_RETRY) {}
       return lastRes;
    }

    DLLLOCAL void cancelDisconnect() {
       assert(m_cmd);
       //printd(5, "command::cancelIntern() %d this: %p m_cmd: %p\n", cancelIntern(), this, m_cmd);
       cancelIntern();
       ct_cmd_drop(m_cmd);
       m_cmd = 0;
    }

    DLLLOCAL QoreHashNode* fetch_row(ExceptionSink* xsink, const Placeholders *ph = 0);

    DLLLOCAL int cancel() {
       if (lastRes == RES_END)
          return 0;

       lastRes = RES_CANCELED;
       return cancelIntern();
    }

    DLLLOCAL int get_row_count();

    DLLLOCAL command(connection& conn, ExceptionSink* xsink);
    DLLLOCAL ~command();

    DLLLOCAL void clear();

    DLLLOCAL CS_COMMAND* operator()() const { return m_cmd; }
    DLLLOCAL connection& getConnection() const { return m_conn; }

    DLLLOCAL void send(ExceptionSink* xsink);
    DLLLOCAL void initiate_language_command(const char *cmd_text, class ExceptionSink *xsink);
    // returns true if data returned, false if not
    DLLLOCAL bool fetch_row_into_buffers(class ExceptionSink *xsink);
    // returns the number of columns in the result
    DLLLOCAL unsigned get_column_count(ExceptionSink *xsink);
    // returns 0=OK, -1=error (exception raised)
    DLLLOCAL void set_params(sybase_query &query, const QoreListNode *args, ExceptionSink *xsink);

    DLLLOCAL AbstractQoreNode* readOutput(connection& conn, command& cmd, bool list, bool& connection_reset, bool cols, ExceptionSink* xsink);

    //DLLLOCAL AbstractQoreNode *read_output(bool list, bool &disconnect, ExceptionSink* xsink);

    DLLLOCAL QoreHashNode *read_cols(const Placeholders *placeholder_list,
                                     int cnt,
                                     bool cols,
                                     ExceptionSink* xsink);

    DLLLOCAL QoreHashNode *read_cols(const Placeholders *placeholder_list,
                                     bool cols,
                                     ExceptionSink* xsink) {
       return read_cols(placeholder_list, -1, cols, xsink);
    }

    DLLLOCAL AbstractQoreNode *read_rows(Placeholders *placeholder_list, bool list, bool cols, ExceptionSink* xsink);
    DLLLOCAL AbstractQoreNode *read_rows(const Placeholders *placeholder_list, ExceptionSink* xsink);

    DLLLOCAL void set_placeholders(const Placeholders &ph) {
        query->placeholders = ph;
    }

    DLLLOCAL int bind_query(std::unique_ptr<sybase_query> &query,
                            const QoreListNode *args,
                            ExceptionSink*);
};


#endif
