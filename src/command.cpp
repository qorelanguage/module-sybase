/* -*- indent-tabs-mode: nil -*- */
/*
  command.cpp

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

#include "sybase.h"

#include "minitest.hpp"

#include <assert.h>
#include <cstypes.h>

#include <memory>
#include <iostream>
#include <algorithm>
#include <string>

#include "command.h"
#include "connection.h"
#include "utils.h"
#include "resultfactory.h"

static std::string get_placeholder_at(const Placeholders *ph, size_t i) {
   if (!ph || ph->size() <= i) return ss::string_cast(i);
   if (ph->at(i).empty()) return ss::string_cast(i);
   return ph->at(i);
}

command::command(connection& conn, ExceptionSink* xsink) : m_conn(conn), m_cmd(0), rowcount(-1), lastRes(RES_NONE) {
   CS_RETCODE err = ct_cmd_alloc(m_conn.getConnection(), &m_cmd);
   if (err != CS_SUCCEED) {
      xsink->raiseException("TDS-EXEC-EXCEPTION", "Sybase call ct_cmd_alloc() failed with error %d", (int)err);
      return;
   }
}

//------------------------------------------------------------------------------
command::~command() {
   if (!m_cmd) return;
   // cancel only unfinished, not already canceled command
   if (lastRes != RES_CANCELED && lastRes != RES_END) {
      if (ct_cancel(0, m_cmd, CS_CANCEL_ALL) != CS_SUCCEED) {
	 throw ss::Error("TDS-EXEC-EXCEPTION", "ct_cancel failed");
      }
   }
   ct_cmd_drop(m_cmd);
}

void command::send(ExceptionSink *xsink) {
   CS_RETCODE err = ct_send(m_cmd);

   if (err != CS_SUCCEED) {
      m_conn.do_exception(xsink, "TDS-EXEC-ERROR", "ct_send() failed");
   }
}

void command::initiate_language_command(const char *cmd_text, ExceptionSink *xsink) {
   assert(cmd_text && cmd_text[0]);
   CS_RETCODE err = ct_command(m_cmd, CS_LANG_CMD, (CS_CHAR*)cmd_text, CS_NULLTERM, CS_UNUSED);
   if (err != CS_SUCCEED) {
      m_conn.do_exception(xsink, "TDS-EXEC-ERROR", "ct_command(CS_LANG_CMD, '%s') failed with error %d", cmd_text, (int)err);
   }
}

bool command::fetch_row_into_buffers(ExceptionSink *xsink) {
   CS_INT rows_read;
   CS_RETCODE err = ct_fetch(m_cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, &rows_read);
   if (err == CS_SUCCEED) {
      if (rows_read != 1) {
	 m_conn.do_exception(xsink, "TDS-EXEC-ERROR", "ct_fetch() returned %d rows (expected 1)", (int)rows_read);
      }
      return true;
   }
   if (err == CS_END_DATA) {
      // all data read, we can continue reading results
      lastRes = RES_NONE;
      return false;
   }
   m_conn.do_exception(xsink, "TDS-EXEC-EXCEPTION", "ct_fetch() returned errno %d", (int)err);
   return false;
}

unsigned command::get_column_count(ExceptionSink* xsink) {
   CS_INT num_cols;
   CS_RETCODE err = ct_res_info(m_cmd, CS_NUMDATA, &num_cols, CS_UNUSED, NULL);
   if (err != CS_SUCCEED) {
      m_conn.do_exception(xsink, "TDS-EXEC-EXCEPTION", "ct_res_info() failed with error %d", (int)err);
   }
   if (num_cols <= 0) {
      m_conn.do_exception(xsink, "TDS-EXEC-EXCEPTION", "ct_res_info() failed");
   }
   return num_cols;
}

// FIXME: use ct_setparam to avoid copying data
void command::set_params(sybase_query &query, const QoreListNode *args, ExceptionSink *xsink) {
   unsigned nparams = query.param_list.size();

   for (unsigned i = 0; i < nparams; ++i) {
      if (query.param_list[i] == 'd')
	 continue;

      const AbstractQoreNode *val = args ? args->retrieve_entry(i) : NULL;

      CS_DATAFMT datafmt;
      memset(&datafmt, 0, sizeof(datafmt));
      datafmt.status = CS_INPUTVALUE;
      datafmt.namelen = CS_NULLTERM;
      sprintf(datafmt.name, "@par%d", int(i + 1));
      datafmt.maxlength = CS_UNUSED;
      datafmt.count = 1;

      CS_RETCODE err = CS_FAIL;

      if (!val || is_null(val) || is_nothing(val)) {
#ifdef FREETDS
	 // it seems to be necessary to specify a type like
	 // this to get a null value to be bound with freetds
	 datafmt.datatype = CS_CHAR_TYPE;
	 datafmt.format = CS_FMT_NULLTERM;
	 datafmt.maxlength = 1;
#endif
	 // SQL NULL value
	 err = ct_param(m_cmd, &datafmt, 0, CS_UNUSED, -1);
	 if (err != CS_SUCCEED) {
	    m_conn.do_exception(xsink, "TDS-EXEC-ERROR",
				"ct_param() for 'null' failed for parameter %u with error %d",
				i, (int)err);
	    return;
	 }
	 continue;
      }

      qore_type_t ntype = val ? val->getType() : 0;

      switch (ntype) {
	 case NT_STRING: {
	    const QoreStringNode *str = reinterpret_cast<const QoreStringNode*>(val);
	    // ensure we bind with the proper encoding for the connection
	    TempEncodingHelper s(str, m_conn.getEncoding(), xsink);
	    if (!s) throw ss::Error("TDS-EXEC-ERROR", "encoding");

	    int slen = s->strlen();
            // note that freetds requires maxlength to be set to the byte length of the string
            // even if CS_FMT_NULLTERM is used, therefore we set CS_FMT_UNUSED
	    datafmt.datatype = CS_CHAR_TYPE;
	    datafmt.format = CS_FMT_UNUSED;
	    // NOTE: setting large sizes here like 2GB works for sybase ctlib,
	    // not for freetds
	    datafmt.maxlength = slen;
	    err = ct_param(m_cmd, &datafmt, (CS_VOID*)s->getBuffer(), slen, 0);
	    break;
	 }

	 case NT_NUMBER: {
	    QoreStringValueHelper vh(val);
	    int slen = vh->strlen();
	    datafmt.datatype = CS_CHAR_TYPE;
	    datafmt.format = CS_FMT_NULLTERM;
	    datafmt.maxlength = slen + 1;
	    err = ct_param(m_cmd, &datafmt, (CS_VOID *)vh->getBuffer(), slen, 0);
	    break;
	 }

	 case NT_DATE: {
	    const DateTimeNode *date = reinterpret_cast<const DateTimeNode*>(val);
	    CS_DATETIME dt;
	    ss::Conversions conv;
	    if (conv.DateTime_to_DATETIME(date, dt, xsink))
	       throw ss::Error("TDS-EXEC-ERROR", "can't convert date");

	    datafmt.datatype = CS_DATETIME_TYPE;
	    err = ct_param(m_cmd, &datafmt, &dt, sizeof(dt), 0);
	    break;
	 }

	 case NT_INT: {
#ifdef CS_BIGINT_TYPE
	    datafmt.datatype = CS_BIGINT_TYPE;
	    err = ct_param(m_cmd, &datafmt, &(const_cast<QoreBigIntNode*>(reinterpret_cast<const QoreBigIntNode*>(val))->val), sizeof(int64), 0);
#else
	    int64 ival = reinterpret_cast<const QoreBigIntNode*>(val)->val;
	    // if it's a 32-bit integer, bind as integer
	    if (ival <= 2147483647 && ival >= -2147483647) {
	       datafmt.datatype = CS_INT_TYPE;
	       CS_INT vint = ival;
	       err = ct_param(m_cmd, &datafmt, &vint, sizeof(CS_INT), 0);
	    }
	    else { // bind as float
	       CS_FLOAT fval = ival;
	       datafmt.datatype = CS_FLOAT_TYPE;
	       err = ct_param(m_cmd, &datafmt, &fval, sizeof(CS_FLOAT), 0);
	    }
#endif
	    break;
	 }

	 case NT_BOOLEAN: {
	    // Seems mssql doesn't like  CS_BIT_TYPE for some reason.
	    // Replacing by CS_INT_TYPE helps
	    //
	    // The "BIT" code is supposed to be like this:
	    // datafmt.datatype = CS_BIT_TYPE;
	    // err = ct_param(m_cmd, &datafmt, &bval, sizeof(bval), 0);
	    // ... but it doesn't work

	    CS_BIT bval = reinterpret_cast<const QoreBoolNode*>(val)->getValue();
	    datafmt.datatype = CS_INT_TYPE;
	    int64 ival = bval ? 1 : 0;
	    err = ct_param(m_cmd, &datafmt, &ival, sizeof(ival), 0);
	    break;
	 }

	 case NT_FLOAT: {
	    CS_FLOAT fval = reinterpret_cast<const QoreFloatNode*>(val)->f;
	    datafmt.datatype = CS_FLOAT_TYPE;
	    err = ct_param(m_cmd, &datafmt, &fval, sizeof(CS_FLOAT), 0);
	    break;
	 }

	 case NT_BINARY: {
	    const BinaryNode *b = reinterpret_cast<const BinaryNode*>(val);
	    datafmt.datatype = CS_BINARY_TYPE;
	    datafmt.maxlength = b->size();
	    datafmt.count = 1;
	    err = ct_param(m_cmd, &datafmt, (void *)b->getPtr(), b->size(), 0);
	    break;
	 }

	 default:
	    m_conn.do_exception(xsink, "TDS-BIND-ERROR",
				"do not know how to bind values of type '%s'",
				val->getTypeName());
	    return;
      } // switch(ntype)

      if (err != CS_SUCCEED) {
	 m_conn.do_exception(xsink, "TDS-EXEC-ERROR",
			     "ct_param() for binary parameter %u failed with error",
			     i, (int)err);
      }
   }
}

int command::get_row_count() {
   return rowcount;
}

command::ResType command::read_next_result1(bool& disconnect, ExceptionSink* xsink) {
   if (*xsink)
      return RES_ERROR;

   switch (lastRes) {
      case RES_DONE:
      case RES_NONE:
      case RES_RETRY:
	 // we can read result only in this states
	 break;
      default:
	 return lastRes;
   }

   CS_INT result_type;
   CS_RETCODE err = ct_results(m_cmd, &result_type);
   //printf("command::read_next_result1 result: %d\n", err);
   switch (err) {
      case CS_END_RESULTS:
	 return RES_END;
      case CS_FAIL: {
         if (cancelIntern())
            disconnect = true;
	 // TODO: handle err == CS_FAIL
	 xsink->raiseException("TDS-EXEC-ERROR",
			       "command::read_output(): ct_results() failed with"
			       " CS_FAIL, command canceled");
	 return RES_ERROR;
      }
      case CS_SUCCEED:
	 break;
      default:
	 m_conn.do_exception(xsink, "TDS-EXEC-ERROR",
			     "command::read_output(): ct_results() returned error code %d", err);
	 return RES_ERROR;
   }

   switch (result_type) {
      case CS_CMD_DONE: {
	 CS_RETCODE ret;
	 rowcount = -1;

         /* from the Sybase docs:
            if ct_results() returns CS_FAIL:
            The routine failed; any remaining results are no longer available.

            If ct_results returns CS_FAIL, an application must call ct_cancel
            with type as CS_CANCEL_ALL before using the affected command
            structure to send another command.

            If ct_cancel returns CS_FAIL, the application must call
            ct_close(CS_FORCE_CLOSE) to force the connection closed.
          */

	 ret = ct_res_info(m_cmd, CS_ROW_COUNT,
			   (CS_VOID *)&rowcount,
			   CS_UNUSED, 0);
	 if (ret != CS_SUCCEED) {
	    m_conn.do_exception(xsink, "TDS-EXEC-EXCEPTION",
				"ct_res_info() failed with error %d", (int)ret);
	    m_conn.purge_messages(xsink);
	    return RES_ERROR;
	 }

	 colinfo.set_dirty();
	 return RES_DONE;
      }
      case CS_CMD_SUCCEED:
	 // the command has no output, continue by reading
	 // next result
	 return RES_RETRY;
      case CS_CMD_FAIL:
	 m_conn.do_exception(xsink, "TDS-EXEC-ERROR",
			     "command::read_output(): SQL command failed");
	 return RES_ERROR;

      case CS_PARAM_RESULT:
	 return RES_PARAM;
      case CS_STATUS_RESULT:
	 return RES_STATUS;
      case CS_ROW_RESULT:
	 return RES_ROW;
   }

   m_conn.do_exception(xsink, "TDS-EXEC-ERROR", "command::read_output(): ct_results() returned unexpected result type %d", (int)result_type);
   return RES_ERROR;
}

AbstractQoreNode* command::readOutput(connection& conn, command& cmd, bool list, bool& connection_reset, ExceptionSink* xsink) {
   ReferenceHolder<AbstractQoreNode> qresult(xsink);

   ss::ResultFactory rf(xsink);

   while (true) {
      ResType rt = conn.readNextResult(cmd, connection_reset, xsink);
      if (*xsink || connection_reset)
         return 0;

      //read_next_result(disconnect, xsink);
      switch (rt) {
	 case RES_ERROR:
	    return 0;

	 case RES_PARAM:
	    if (retr_colinfo(xsink))
               return 0;
	    qresult = read_rows(&query->placeholders, xsink);
	    //add_rowcount(*qresult, 1, xsink);
	    rf.add_params(qresult);
	    break;

         case RES_ROW:
	    qresult = read_rows(0, list, xsink);
	    rf.add(qresult, list);
	    break;

         case RES_END:
	    return rf.res();

         case RES_DONE:
	    rf.done(rowcount);
	    continue;

         case RES_STATUS:
            if (retr_colinfo(xsink))
               return 0;

	    qresult = read_rows(0, list, xsink);
	    // TODO: check status?
	    colinfo.set_dirty();
	    continue;

         default:
	    m_conn.do_exception(xsink, "TDS-EXEC-ERROR", "command::readOutput(): ct_results() returned unknown result value %d", rt);
	    break;
      }
      if (*xsink)
	 return 0;
   }
}

/*
AbstractQoreNode *command::read_output(bool list, bool &disconnect, ExceptionSink* xsink) {
   ReferenceHolder<AbstractQoreNode> qresult(xsink);

   ss::ResultFactory rf(xsink);

   for (;;) {
      ResType rt = read_next_result(disconnect, xsink);
      switch (rt) {
	 case RES_ERROR:
	    return 0;

	 case RES_PARAM:
	    if (retr_colinfo(xsink))
               return 0;
	    qresult = read_rows(&query->placeholders, xsink);
	    //add_rowcount(*qresult, 1, xsink);
	    rf.add_params(qresult);
	    break;

         case RES_ROW:
	    qresult = read_rows(0, list, xsink);
	    rf.add(qresult, list);
	    break;

         case RES_END:
	    return rf.res();

         case RES_DONE:
	    rf.done(rowcount);
	    continue;

         case RES_STATUS:
            if (retr_colinfo(xsink))
               return 0;

	    qresult = read_rows(0, list, xsink);
	    // TODO: check status?
	    colinfo.set_dirty();
	    continue;

         default:
	    m_conn.do_exception(xsink, "TDS-EXEC-ERROR", "ct_results() returned unknown result value %d", rt);
	    break;
      }
      if (*xsink)
	 return 0;
   }
}
*/

int command::retr_colinfo(ExceptionSink* xsink) {
   unsigned columns = get_column_count(xsink);
   if (*xsink)
      return -1;

   colinfo.reset();
   get_row_description(colinfo.datafmt, columns, xsink);
   setup_output_buffers(colinfo.datafmt, xsink);
   colinfo.dirty = false;

   return 0;
}

QoreHashNode *command::read_cols(const Placeholders *ph, int cnt, ExceptionSink* xsink) {
   if (ensure_colinfo(xsink)) return 0;

   if (xsink->isException()) return 0;

   row_result_t &descriptions = colinfo.datafmt;

   // setup hash of lists if necessary
   ReferenceHolder<QoreHashNode> h(new QoreHashNode, xsink);

   bool done = false;
   while (fetch_row_into_buffers(xsink)) {
      if (!done) {
         for (unsigned i = 0, n = descriptions.size(); i != n; ++i) {
            std::string col_name;

            if (!ss::is_empty(descriptions[i].name)) {
               col_name = descriptions[i].name;
               std::transform(col_name.begin(), col_name.end(), col_name.begin(), ::tolower);
            } else {
               col_name = get_placeholder_at(ph, i);
            }

            HashAssignmentHelper hah(**h, col_name);
            if (*hah) {
               // find a unique column name
               unsigned num = 1;
               while (true) {
                  QoreStringMaker tmp("%s_%d", col_name.c_str(), num);
                  hah.reassign(tmp.c_str());
                  if (*hah) {
                     ++num;
                     continue;
                  }
                  break;
               }
            }
            hah.assign(new QoreListNode, xsink);
         }
      }
      if (append_buffers_to_list(descriptions, out_buffers, *h, xsink))
	 return 0;
      if (--cnt == 0) break;
   }
   return h.release();
}

QoreHashNode * command::fetch_row(ExceptionSink* xsink, const Placeholders *ph) {
   if (ensure_colinfo(xsink)) return 0;

   if (!fetch_row_into_buffers(xsink)) return 0;
   QoreHashNode *h = output_buffers_to_hash(ph, xsink);
   return h;
}

AbstractQoreNode *command::read_rows(const Placeholders *ph, ExceptionSink* xsink) {
   if (ensure_colinfo(xsink)) return 0;

   ReferenceHolder<AbstractQoreNode> rv(xsink);
   QoreListNode *l = 0;
   while (fetch_row_into_buffers(xsink)) {
      ReferenceHolder<QoreHashNode> h(output_buffers_to_hash(ph, xsink), xsink);
      if (*xsink) return 0;
      if (rv) {
	 if (!l) {
	    ReferenceHolder<QoreListNode> lholder(new QoreListNode, xsink);
	    l = *lholder;
	    l->push(rv.release());
	    rv = lholder.release();
	 }
	 l->push(h.release());
      }
      else
	 rv = h.release();
   }
   return rv.release();
}

AbstractQoreNode *command::read_rows(Placeholders *placeholder_list, bool list, ExceptionSink* xsink) {
   if (ensure_colinfo(xsink)) return 0;

   // setup hash of lists if necessary
   if (!list) {
      if (!placeholder_list) {
	 return read_cols(0, xsink);
      }
      return read_cols(placeholder_list, xsink);
   } else {
      if (!placeholder_list) {
	 return read_rows(0, xsink);
      }
      return read_rows(placeholder_list, xsink);
   }
}

// returns 0=OK, -1=error (exception raised)
int command::get_row_description(row_result_t &result, unsigned column_count, ExceptionSink* xsink) {
   for (unsigned i = 0; i < column_count; ++i) {
      CS_DATAFMT_EX datafmt;
      memset(&datafmt, 0, sizeof(datafmt));

      CS_RETCODE err = ct_describe(m_cmd, i + 1, &datafmt);
      if (err != CS_SUCCEED) {
	 m_conn.do_exception(xsink, "TDS-EXEC-ERROR", "ct_describe() failed with error %d", (int)err);
	 return -1;
      }
      datafmt.count = 1; // fetch just single row per every ct_fetch()
      bool is_multi_byte = m_conn.getEncoding()->isMultiByte();

      printd(5, "command::get_row_description(): name=%s type=%d usertype=%d\n",
	     datafmt.name, datafmt.datatype, datafmt.usertype);

      datafmt.origin_datatype = datafmt.datatype;
      switch (datafmt.datatype) {
	 // we map DECIMAL types to strings so we have no conversion to do
	 case CS_DECIMAL_TYPE:
	 case CS_NUMERIC_TYPE:
	    datafmt.maxlength = 50;
	    datafmt.datatype = CS_CHAR_TYPE;
	    datafmt.format = CS_FMT_PADBLANK;
	    break;

	 case CS_UNICHAR_TYPE:
	    datafmt.datatype = CS_TEXT_TYPE;
	    datafmt.format = CS_FMT_NULLTERM;
	    break;

	    // freetds only works with CS_FMT_PADBLANK with CS_CHAR columns it seems
	    // however this is also compatible with Sybase's ct-lib
	 case CS_CHAR_TYPE:
	    datafmt.format = CS_FMT_PADBLANK;
	    break;

	 case CS_LONGCHAR_TYPE:
	 case CS_VARCHAR_TYPE:
	 case CS_TEXT_TYPE:
	    // if it's a multi-byte encoding, double the buffer size
	    if (is_multi_byte)
	       datafmt.maxlength *= 2;
	    datafmt.format = CS_FMT_NULLTERM;
	    break;

#ifdef FREETDS
	    // FreeTDS seems to return DECIMAL types as FLOAT for some reason
	 case CS_FLOAT_TYPE:
	    // can't find a defined USER_TYPE_* for 26
	    if (datafmt.usertype == 26) {
	       datafmt.maxlength = 50;
	       datafmt.datatype = CS_CHAR_TYPE;
	       datafmt.format = CS_FMT_NULLTERM;
	       break;
	    }
#endif

	 case CS_MONEY_TYPE:
	 case CS_MONEY4_TYPE:
	    datafmt.datatype = CS_FLOAT_TYPE;

	 default:
	    datafmt.format = CS_FMT_UNUSED;
	    break;
      }

      printd(5, "command::get_row_description(): name=%s type=%d usertype=%d maxlength=%d\n", datafmt.name, datafmt.datatype, datafmt.usertype, datafmt.maxlength);

      result.push_back(datafmt);
   }
   return 0;
}

int command::setup_output_buffers(const row_result_t &input_row_descriptions, ExceptionSink *xsink) {
   out_buffers.reset();
   for (unsigned i = 0, n = input_row_descriptions.size(); i != n; ++i) {
      unsigned size = input_row_descriptions[i].maxlength;
      output_value_buffer *out = out_buffers.insert(size);

      CS_RETCODE err = ct_bind(m_cmd, i + 1,
			       (CS_DATAFMT*)&input_row_descriptions[i],
			       out->value, &out->value_len, &out->indicator);

      if (err != CS_SUCCEED) {
	 m_conn.do_exception(xsink, "TDS-EXEC-ERROR", "ct_bind() failed with error %d", (int)err);
	 return -1;
      }
   }
   return 0;
}

int command::append_buffers_to_list(row_result_t &column_info, row_output_buffers& all_buffers, QoreHashNode *h, ExceptionSink *xsink) {
   HashIterator hi(h);
   for (unsigned i = 0, n = column_info.size(); i != n; ++i) {
      hi.next();

      const output_value_buffer& buff = *all_buffers[i];
      AbstractQoreNode* value = get_node(column_info[i], buff, xsink);
      if (xsink->isException()) {
	 if (value) value->deref(xsink);
	 return -1;
      }

      QoreListNode *l = reinterpret_cast<QoreListNode*>(hi.getValue());
      l->push(value);
   } // for

   return 0;
}

QoreHashNode *command::output_buffers_to_hash(const Placeholders *ph, ExceptionSink* xsink) {
   row_result_t &column_info = colinfo.datafmt;
   ReferenceHolder<QoreHashNode> result(new QoreHashNode, xsink);

   for (unsigned i = 0, n = column_info.size(); i != n; ++i) {
      const output_value_buffer& buff = *out_buffers[i];

      ReferenceHolder<AbstractQoreNode>
	 value(get_node(column_info[i], buff, xsink), xsink);

      if (*xsink) return 0;

      std::string column_name;
      if (!ss::is_empty(column_info[i].name)) {
	 column_name = column_info[i].name;
	 std::transform(column_name.begin(), column_name.end(), column_name.begin(), ::tolower);
      } else {
	 column_name = get_placeholder_at(ph, i);
      }

      HashAssignmentHelper hah(**result, column_name);
      if (*hah) {
         // find a unique column name
         unsigned num = 1;
         while (true) {
            QoreStringMaker tmp("%s_%d", column_name.c_str(), num);
            hah.reassign(tmp.c_str());
            if (*hah) {
               ++num;
               continue;
            }
            break;
         }
      }

      hah.assign(value.release(), xsink);
      //result->setKeyValue(column_name, value.release(), xsink);
   }

   return result.release();
}

static bool is_number(const CS_DATAFMT_EX& datafmt) {
   switch (datafmt.origin_datatype) {
      case CS_DECIMAL_TYPE:
      case CS_NUMERIC_TYPE:
	 return true;
   }
   return false;
}

static inline bool need_trim(const CS_DATAFMT_EX& datafmt) {
   if (datafmt.format == CS_FMT_PADBLANK || datafmt.usertype == 34 ||
       // seems TEXT needs trim as well (found on mssql, sybase-test.q)
       datafmt.datatype == CS_TEXT_TYPE
#ifdef SYBASE
       // for some reason sybase returns a char field as LONGCHAR when the
       // server is uses iso_1 character encoding, but the connection is set
       // to utf-8 also in this case the result is always blank padded even
       // though the datafmt.format is set to CS_FMT_NULLTERM
       || datafmt.datatype == CS_LONGCHAR_TYPE
#endif
      ) {
      return true;
   }
   return false;
}

AbstractQoreNode* command::getNumber(const char* str, size_t len) {
   assert(!str[len]);
   int nf = m_conn.getNumeric();

   assert(nf != connection::OPT_NUM_STRING);

   // trim off trailing zeros after the decimal in any case
   bool has_decimal = (bool)strchr(str, '.');
   if (has_decimal) {
      char* c = (char*)str;
      // trim off trailing zeros
      while (len && c[len - 1] == '0') {
	 --len;
	 c[len] = '\0';
      }
      if (c[len - 1] == '.') {
	 --len;
	 c[len] = '\0';
	 has_decimal = false;
      }
   }
   //printf("num: '%s' has_dec: %d\n", str, has_decimal);

   if (nf == connection::OPT_NUM_OPTIMAL && !has_decimal) {
      bool sign = str[0] == '-';
      if (sign)
	 --len;
      if (!strchr(str, '.')
	  && (len < 19
	      || (len == 19 &&
		  ((!sign && strcmp(str, "9223372036854775807") <= 0)
		   ||(sign && strcmp(str, "-9223372036854775808") >= 0)))))
	 return new QoreBigIntNode(strtoll(str, 0, 10));
   }

   return new QoreNumberNode(str);
}

AbstractQoreNode *command::get_node(const CS_DATAFMT_EX& datafmt, const output_value_buffer& buffer, ExceptionSink* xsink) {
   if (buffer.indicator == -1) { // SQL NULL
      return null();
   }

   const QoreEncoding *encoding = m_conn.getEncoding();

   switch (datafmt.datatype) {
      case CS_LONGCHAR_TYPE:
      case CS_VARCHAR_TYPE:
      case CS_TEXT_TYPE: {
	 CS_CHAR* value = (CS_CHAR*)(buffer.value);

	 // copy the value to a null-terminated string for processing
	 QoreString tmp((const char*)value, buffer.value_len - 1);

#ifdef SYBASE
	 if (need_trim(datafmt))
	    tmp.trim_trailing(' ');
#endif

	 if (is_number(datafmt) && m_conn.getNumeric() != connection::OPT_NUM_STRING)
	    return getNumber(tmp.c_str(), tmp.size());

	 size_t len = tmp.size();
	 size_t all = tmp.capacity();
	 return new QoreStringNode(tmp.giveBuffer(), len, all, encoding);
      }

      case CS_CHAR_TYPE: {
	 CS_CHAR* value = (CS_CHAR*)(buffer.value);

	 // copy the value to a null-terminated string for processing
	 QoreString tmp((const char*)value, buffer.value_len);
	 if (need_trim(datafmt))
	    tmp.trim_trailing(' ');

	 if (is_number(datafmt) && m_conn.getNumeric() != connection::OPT_NUM_STRING)
	    return getNumber(tmp.c_str(), tmp.size());

	 size_t len = tmp.size();
	 size_t all = tmp.capacity();
	 return new QoreStringNode(tmp.giveBuffer(), len, all, encoding);
      }

      case CS_VARBINARY_TYPE:
      case CS_BINARY_TYPE:
      case CS_LONGBINARY_TYPE:
      case CS_IMAGE_TYPE: {
	 CS_BINARY* value = (CS_BINARY*)(buffer.value);
	 int size = buffer.value_len;
	 void* block = malloc(size);
	 if (!block) {
	    xsink->outOfMemory();
	    return 0;
	 }
	 memcpy(block, value, size);
	 return new BinaryNode(block, size);
      }

      case CS_TINYINT_TYPE: {
	 CS_TINYINT* value = (CS_TINYINT*)(buffer.value);
	 return new QoreBigIntNode(*value);
      }

      case CS_SMALLINT_TYPE: {
	 CS_SMALLINT* value = (CS_SMALLINT*)(buffer.value);
	 return new QoreBigIntNode(*value);
      }

      case CS_INT_TYPE: {
	 CS_INT* value = (CS_INT*)(buffer.value);
	 return new QoreBigIntNode(*value);
      }

#ifdef CS_BIGINT_TYPE
      case CS_BIGINT_TYPE: {
	 int64 *value = (int64 *)(buffer.value);
	 return new QoreBigIntNode(*value);
      }
#endif

      case CS_REAL_TYPE: {
	 CS_REAL* value = (CS_REAL*)(buffer.value);
	 return new QoreFloatNode((double)*value);
      }

      case CS_FLOAT_TYPE: {
	 CS_FLOAT* value = (CS_FLOAT*)(buffer.value);
	 return new QoreFloatNode((double)*value);
      }

      case CS_BIT_TYPE: {
	 CS_BIT* value = (CS_BIT*)(buffer.value);
	 return get_bool_node(*value != 0);
      }

      case CS_DATETIME_TYPE: {
	 CS_DATETIME* value = (CS_DATETIME*)(buffer.value);

	 ss::Conversions conv;
	 // NOTE: can't find a USER_* define for 38!
	 if (datafmt.usertype == 38)
            return conv.TIME_to_DateTime(*value, m_conn.getTZ());

	 return conv.DATETIME_to_DateTime(*value, m_conn.getTZ());
      }
      case CS_DATETIME4_TYPE: {
	 ss::Conversions conv;
	 CS_DATETIME4* value = (CS_DATETIME4*)(buffer.value);
	 return conv.DATETIME4_to_DateTime(*value);
      }

      default:
	 xsink->raiseException("TDS-EXEC-EXCEPTION", "Unknown data type %d", (int)datafmt.datatype);
	 return 0;
   } // switch
}

int command::bind_query(std::auto_ptr<sybase_query> &q, const QoreListNode *args, ExceptionSink *xsink) {
   query.reset(q.release());

   initiate_language_command(query->buff(), xsink);

   if (args) set_params(*query, args, xsink);

   return 0;
}
