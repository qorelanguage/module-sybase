#ifdef DEBUG

#include "common.h"
#include <qore/AbstractQoreNode.h>
#include <math.h>
#include <memory>

namespace sybase_tests_7527024186 {

//------------------------------------------------------------------------------
TEST()
{
  // testing DATETIME <-> Qore Datetime conversion
  printf("running test %s[%d]\n", __FILE__, __LINE__);

  connection conn;
  ExceptionSink xsink;
  conn.init(SYBASE_TEST_SETTINGS, &xsink);
  if (xsink.isException()) {
    assert(false);
  }

  std::auto_ptr<DateTime> d(new DateTime);

  CS_DATETIME dt1;
  DateTime_to_DATETIME(d.get(), dt1, &xsink);
  if (xsink.isException()) {
    assert(false);
  }
 
  std::auto_ptr<DateTime> d2(DATETIME_to_DateTime(dt1));
  if (xsink.isException()) {
    assert(false);
  }

  if (d->getEpochSeconds() != d2->getEpochSeconds()) {
    assert(false);
  }
  printf("DateTime <-> DATETIME conversion works\n");
}

//------------------------------------------------------------------------------
TEST()
{
  // testing DATETIME4 <-> Qore Datetime conversion
  printf("running test %s[%d]\n", __FILE__, __LINE__);

  connection conn;
  ExceptionSink xsink;
  conn.init(SYBASE_TEST_SETTINGS, &xsink);
  if (xsink.isException()) {
    assert(false);
  }

  std::auto_ptr<DateTime> d(new DateTime);

  CS_DATETIME4 dt1;
  DateTime_to_DATETIME4(conn, d.get(), dt1, &xsink);
  if (xsink.isException()) {
    assert(false);
  }

  std::auto_ptr<DateTime> d2(DATETIME4_to_DateTime(conn, dt1, &xsink));
  if (xsink.isException()) {
    assert(false);
  }

  if (d->getEpochSeconds() != d2->getEpochSeconds()) {
    assert(false);
  }
  printf("conversion DateTime <-> DATETIME4 works\n");
}

//------------------------------------------------------------------------------
// This test fails with FreeTDS 0.64 and Sybase 15.0

TEST()
{
  // testing MONEY <-> float conversion
  printf("running test %s[%d]\n", __FILE__, __LINE__);

  connection conn;
  ExceptionSink xsink;
  conn.init(SYBASE_TEST_SETTINGS, &xsink);
  if (xsink.isException()) {
    assert(false);
  }

  CS_MONEY m;
  double_to_MONEY(conn, 1.2, m, &xsink);
  if (xsink.isException()) {
    assert(false);
  }

  double d = MONEY_to_double(conn, m, &xsink);
  if (xsink.isException()) {
    assert(false);
  }
  assert(d == 1.2);  
  printf("conversion double <-> MONEY works\n");
}

//------------------------------------------------------------------------------
// This test fails with FreeTDS 0.64 and Sybase 15.0

TEST()
{
  // testing MONEY4 <-> float conversion
  printf("running test %s[%d]\n", __FILE__, __LINE__);

  connection conn;
  ExceptionSink xsink;
  conn.init(SYBASE_TEST_SETTINGS, &xsink);
  if (xsink.isException()) {
    assert(false);
  }

  CS_MONEY4 m;
  double_to_MONEY4(conn, 6.2, m, &xsink);
  if (xsink.isException()) {
    assert(false);
  }

  double d = MONEY4_to_double(conn, m, &xsink);
  if (xsink.isException()) {
    assert(false);
  }
  assert(d == 6.2);
  printf("conversion double <-> MONEY4 works\n");
}

//------------------------------------------------------------------------------
TEST()
{
  // testing DECIMAL <-> float conversion
  printf("running test %s[%d]\n", __FILE__, __LINE__);

  connection conn;
  ExceptionSink xsink;
  conn.init(SYBASE_TEST_SETTINGS, &xsink);
  if (xsink.isException()) {
    assert(false);
  }

  CS_DECIMAL dc;  
  double_to_DECIMAL(conn, 6.22, dc, &xsink);
  if (xsink.isException()) {
    assert(false);
  }

  printf("conversion double <-> DECIMAL works\n");
}

} // namespace
#endif

// EOF

