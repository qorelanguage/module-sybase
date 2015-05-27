/*
  sybase_query.h

  Sybase DB layer for QORE
  uses Sybase OpenClient C library

  Qore Programming language

  Copyright (C) 2007 Qore Technologies

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

#ifndef _SYBASE_QUERY_H
#define _SYBASE_QUERY_H

#include <string>
#include <vector>
#include <utility>

typedef std::vector<char> param_list_t;
typedef std::vector<std::string> Placeholders;

struct sybase_query {
   private:
      // not implemented
      sybase_query(const sybase_query &);
      sybase_query& operator=(const sybase_query &);

   public:
      DLLLOCAL sybase_query() {}

      // with %v and %d replaced with @parX
      QoreString m_cmd;

      param_list_t param_list;
      Placeholders placeholders;

      // returns 0=OK, -1=err (exception raised)
      int init(const QoreString *n_cmd,
              const QoreListNode *args,
              class ExceptionSink *xsink);

      int init(const QoreString *n_cmd) {
          m_cmd = *n_cmd;
          return 0;
      }

      const char * buff() const {
          return m_cmd.getBuffer();
      }
};

#endif
