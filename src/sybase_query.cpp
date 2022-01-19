/*
    sybase_query.cpp

    Sybase DB layer for QORE
    uses Sybase OpenClient C library

    Qore Programming language

    Copyright (C) 2007 - 2022 Qore Technologies

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

#include <assert.h>
#include <ctype.h>

#include "sybase.h"
#include "sybase_query.h"

#include "minitest.hpp"

// returns 0=OK, -1=error (exception raised)
int sybase_query::init(const QoreString *cmd_text,
        const QoreListNode *args,
        ExceptionSink *xsink)
{
   m_cmd = *cmd_text;

   const char* s = m_cmd.getBuffer();
   QoreString tmp;
   while (*s) {
       char ch = *s++;

       // skip double qouted strings
       if (ch == '"') {
           for (;;) {
               ch = *s++;
               if (ch == '\\') {
                   ch = *s++;
                   continue;
               }
               if (ch == '"') {
                   goto next;
               }
           }
       }
       // skip single qouted strings
       if (ch == '\'') {
           for (;;) {
               ch = *s++;
               if (ch == '\\') {
                   ch = *s++;
                   continue;
               }
               if (ch == '\'') {
                   goto next;
               }
           }
       }

       if (ch == '%') {
           int offset = s - m_cmd.getBuffer() - 1;
           ch = *s++;
           if (ch == 'v') {
               param_list.push_back('v');
               //param_list.resize(count + 1);
               //param_list[count++].set(PN_VALUE);

               tmp.clear();
               tmp.sprintf("@par%u", param_list.size());
               m_cmd.replace(offset, 2, tmp.c_str());
               s = m_cmd.getBuffer() + offset + tmp.strlen();
           } else if (ch == 'd') {
               QoreValue v = args ? args->retrieveEntry(param_list.size()) : QoreValue();
               tmp.clear();
               DBI_concat_numeric(&tmp, v);
               m_cmd.replace(offset, 2, tmp.c_str());
               s = m_cmd.getBuffer() + offset + tmp.strlen();

               param_list.push_back('d');
               //param_list.resize(count + 1);
               //param_list[count++].set(PN_NUMERIC);
           } else if (ch == 's') {
               QoreValue v = args ? args->retrieveEntry(param_list.size()) : QoreValue();
               tmp.clear();
               if (DBI_concat_string(&tmp, v, xsink))
                   return -1;
               m_cmd.replace(offset, 2, tmp.c_str());
               s = m_cmd.getBuffer() + offset + tmp.strlen();

               // mark it with a 'd' to ensure it gets skipped
               param_list.push_back('d');
               //param_list.resize(count + 1);
               //param_list[count++].set(PN_NUMERIC);
           } else {
               xsink->raiseException("DBI-EXEC-EXCEPTION",
                       "Only %%v or %%d expected in parameter list");
               return -1;
           }
       } else if (ch == ':') {
           // read placeholder name
           int offset = s - m_cmd.getBuffer() - 1;

           const char* placeholder_start = s;
           while (isalnum(*s) || *s == '_') ++s;
           if (s == placeholder_start) {
               xsink->raiseException("DBI-EXEC-EXCEPTION", "Placeholder name missing after ':'");
               return -1;
           }
           m_cmd.replace(offset, 1, "@");

           //param_list.resize(count + 1);
           //param_list[count++].set(placeholder_start, s - placeholder_start);

           // is this creating a std::string and then copying it and creating another in the vector?
           //placeholder_list.add(placeholder_start, s - placeholder_start);

           std::string ph(placeholder_start, s - placeholder_start);
           placeholders.push_back(ph);
       }
next:
       ;
   } // while

   s = m_cmd.getBuffer();
   s = 0;
   //printd(5, "size=%d, m_cmd=%s\n", param_list.size(), m_cmd.getBuffer());
   return 0;
}

