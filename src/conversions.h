/*
  conversions.h

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

#ifndef SYBASE_CONVERSIONS_H_
#define SYBASE_CONVERSIONS_H_

// Conversions between some Sybase and Qore types.

#include <ctpublic.h>

#include "qore/DateTime.h"
#include "qore/DateTimeNode.h"

namespace ss {
class Conversions {
public:
    DLLLOCAL static int DateTime_to_DATETIME(const DateTime* dt, CS_DATETIME& out, ExceptionSink* xsink);

    DLLLOCAL static DateTimeNode *TIME_to_DateTime(CS_DATETIME &dt, const AbstractQoreZoneInfo *tz = nullptr);

    DLLLOCAL static DateTimeNode* DATETIME_to_DateTime(CS_DATETIME& dt, const AbstractQoreZoneInfo *tz = nullptr);

    DLLLOCAL static DateTimeNode* DATETIME4_to_DateTime(CS_DATETIME4& dt);

    DLLLOCAL static DateTimeNode* BIGDATETIME_to_DateTime(uint64_t dt, const AbstractQoreZoneInfo *tz = nullptr);

    DLLLOCAL static DateTimeNode* BIGTIME_to_DateTime(uint64_t dt, const AbstractQoreZoneInfo *tz = nullptr);

    DLLLOCAL static DateTimeNode* DATE_to_DateTime(unsigned dt, const AbstractQoreZoneInfo *tz = nullptr);
};
} // namespace ss

#endif

// EOF

