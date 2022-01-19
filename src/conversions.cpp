/*
    conversions.cpp

    Sybase DB layer for QORE
    uses Sybase OpenClient C library

    Qore Programming language

    Copyright (C) 2007 - 2022 Qore Technologies, s.r.o.

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

#include <cstypes.h>
#include <ctpublic.h>
#include <assert.h>

#include <string>

#include <cmath>
#include <stdint.h>

#include "sybase.h"
#include "connection.h"
#include "conversions.h"

#include "minitest.hpp"

namespace ss {

// Sybase dates (so the "Sybase epoch") start from 1900-01-01
// 25567 days from 1900-01-01 to 1970-01-01, the start of the Qore 64-bit epoch
static const uint32_t SYB_DAYS_TO_EPOCH = 25567;
static const uint32_t SYB_SECS_TO_EPOCH = (SYB_DAYS_TO_EPOCH * 86400LL);

// number of seconds before the UNIX epoch of 0000-01-01 for CS_BIGDATETIME
/** this value was determined empirically (rather than calculated) from retrieving date 1970-01-01 00:00:00 +00:00
    from MS SQL Server
*/
static const int64_t YEAR_ZERO_SECS = -62167219200;

DateTimeNode* Conversions::TIME_to_DateTime(CS_DATETIME& dt, const AbstractQoreZoneInfo* tz) {
    int64 secs = dt.dttime / 300;

    // use floating point to get more accurate 1/3 s
    double ts = round((double)(dt.dttime - (secs * 300)) * 3.3333333);
    DateTimeNode* rv = new DateTimeNode(secs, (int)ts);
    try { if (tz) rv->setZone(tz); } catch(...) {}
    return rv;
}

DateTimeNode* Conversions::DATETIME_to_DateTime(CS_DATETIME& dt, const AbstractQoreZoneInfo* tz) {
    int64 secs = dt.dttime / 300;
    // use floating point to get more accurate 1/3 s
    double ts = round((double)(dt.dttime - (secs * 300)) * 3.3333333);
    DateTimeNode* rv = new DateTimeNode(secs + dt.dtdays * 86400ll - SYB_SECS_TO_EPOCH, (int)ts);
    try { if (tz) rv->setZone(tz); } catch(...) {}
    return rv;
}

DateTimeNode* Conversions::BIGDATETIME_to_DateTime(uint64_t dt, const AbstractQoreZoneInfo* tz) {
    // get seconds
    uint64_t secs = dt / 1000000;
    // get microseconds
    int us = dt - (secs * 1000000);
    // get seconds from the UNIX epoch
    secs += YEAR_ZERO_SECS;
    // we return a time in the local time zone; not in UTC
    return DateTimeNode::makeAbsoluteLocal(tz, secs, us);
}

DateTimeNode* Conversions::BIGTIME_to_DateTime(uint64_t dt, const AbstractQoreZoneInfo* tz) {
    // get seconds
    uint64_t secs = dt / 1000000;
    // get microseconds
    int us = dt - (secs * 1000000);
    // we return a time in the local time zone; not in UTC
    return DateTimeNode::makeAbsoluteLocal(tz, secs, us);
}

DateTimeNode* Conversions::DATE_to_DateTime(unsigned dt, const AbstractQoreZoneInfo* tz) {
    int64 secs = dt * 60 * 60 * 24 - SYB_SECS_TO_EPOCH;
    return DateTimeNode::makeAbsoluteLocal(tz, secs, 0);
}

static int check_epoch(int64 secs, const DateTime &dt, ExceptionSink *xsink) {
    // 9999-12-31 23:59:59 has an epoch offset of: 253402300799 seconds
    if (secs > 253402300799ll) {
        QoreStringNode *desc = new QoreStringNode("maximum sybase datetime value is 9999-12-31, date passed: ");
        dt.format(*desc, "YYYY-DD-MM");
        xsink->raiseException("TDS-DATE-ERROR", desc);
        return -1;
    }
    // 1753-01-01 00:00:00 has an eopch offset of: -6847804800 seconds
    if (secs < -6847804800ll) {
        QoreStringNode *desc = new QoreStringNode("minumum sybase datetime value is 1753-01-01, date passed: ");
        dt.format(*desc, "YYYY-DD-MM");
        xsink->raiseException("TDS-DATE-ERROR", desc);
        return -1;
    }
    return 0;
}

int Conversions::DateTime_to_DATETIME(const DateTime* dt, CS_DATETIME &out, ExceptionSink* xsink) {
    if (dt->isRelative()) {
        xsink->raiseException("TDS-DATE-ERROR", "relative date passed for binding as absolute date");
        return -1;
    }
    int64 secs = dt->getEpochSeconds();

    if (check_epoch(secs, *dt, xsink)) {
        return -1;
    }

    int days = secs / 86400;
    out.dtdays = days + SYB_DAYS_TO_EPOCH;

#ifdef _QORE_HAS_TIME_ZONES
    // use floating point to get more accurate 1/3 s
    double ts = round((double)dt->getMicrosecond() / 3333.3333333);
#else
    // use floating point to get more accurate 1/3 s
    double ts = round((double)dt->getMillisecond() / 3.3333333);
#endif
    out.dttime = (secs - (days * 86400)) * 300 + (int)ts;

    return 0;
}

DateTimeNode * Conversions::DATETIME4_to_DateTime(CS_DATETIME4 &dt) {
    int64 secs = dt.minutes * 60LL + dt.days * 86400LL - SYB_SECS_TO_EPOCH;
    return new DateTimeNode(secs);
}

} // namespace ss
