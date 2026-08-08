/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    bulk_load.h

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the
    Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef QORE_SYBASE_BULK_LOAD_H
#define QORE_SYBASE_BULK_LOAD_H

#if defined(FREETDS) && defined(HAVE_QORE_BULK_LOAD) && defined(HAVE_FREETDS_BULK)

#include <bkpublic.h>

#include <cstdint>
#include <string>
#include <vector>

#include <qore/Qore.h>

class connection;

//! SQL Server destination families supported by the FreeTDS native serializer.
enum class QoreSybaseBulkType {
    Text,
    Unicode,
    Binary,
    Integer,
    Numeric,
    Floating,
    Boolean,
    DateTime,
};

//! One requested column and its ordinal in the destination table.
struct QoreSybaseBulkColumn {
    std::string key;
    std::string type_name;
    CS_INT ordinal = 0;
    CS_INT max_length = 0;
    QoreSybaseBulkType type = QoreSybaseBulkType::Text;
    bool nullable = false;
};

//! Stable buffers used by blk_bind() until blk_rowxfer() has consumed one row.
struct QoreSybaseBulkCell {
    std::vector<unsigned char> data;
    CS_DATAFMT format = {};
    CS_INT length = 0;
    CS_SMALLINT indicator = CS_GOODDATA;
};

//! Persistent FreeTDS CT-Library bulk-copy state for one DBI native bulk load.
class QoreSybaseBulkLoadState {
public:
    DLLLOCAL QoreSybaseBulkLoadState(connection& conn, bool stream_bounds);
    DLLLOCAL ~QoreSybaseBulkLoadState();

    QoreSybaseBulkLoadState(const QoreSybaseBulkLoadState&) = delete;
    QoreSybaseBulkLoadState& operator=(const QoreSybaseBulkLoadState&) = delete;

    //! Initializes BCP and returns 0 for native, 1 for dynamic fallback, or -1 for error.
    DLLLOCAL int initialize(const QoreString* table, const QoreListNode* column_list, ExceptionSink* xsink);
    //! Sends and closes one BCP batch.
    DLLLOCAL int sendRows(const QoreHashNode* rows, ExceptionSink* xsink);
    //! Finishes or aborts the operation.
    DLLLOCAL int finish(bool success, ExceptionSink* xsink);

private:
    connection& conn;
    Datasource* ds;
    CS_BLKDESC* descriptor = nullptr;
    std::vector<QoreSybaseBulkColumn> columns;
    std::vector<QoreSybaseBulkCell> cells;
    bool savepoint = false;
    bool failed = false;
    bool stream_active = false;
    bool stream_started = false;
    bool stream_ok = true;
    bool protocol_started = false;
    int64 consumed = 0;
    int64 reported = 0;
    uint64_t rows_sent = 0;

    DLLLOCAL int serialize(QoreValue value, const QoreSybaseBulkColumn& column,
            QoreSybaseBulkCell& cell, ExceptionSink* xsink);
    DLLLOCAL int addBytes(size_t bytes, ExceptionSink* xsink);
    DLLLOCAL int endStream(bool success, ExceptionSink* xsink);
    DLLLOCAL int cancelAndRollback(ExceptionSink* xsink, bool report_errors);
    DLLLOCAL void emergencyAbort();
};

#endif

#endif
