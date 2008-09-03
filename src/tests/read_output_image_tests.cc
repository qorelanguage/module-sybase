#ifdef DEBUG

#include "common.h"
#include "connection.h"

namespace sybase_tests_591606641112 {

//------------------------------------------------------------------------------
static void create_image_table()
{
  const char* cmd =  "create table image_table (image_col image )";

  connection c;
  ExceptionSink xsink;
  c.init(SYBASE_TEST_SETTINGS, &xsink);
  if (xsink.isException()) {
    assert(false);
  }
  c.direct_execute(cmd, &xsink);
  if (xsink.isException()) {
    assert(false);
  }
  cmd = "insert into image_table values (0xFFFFFE6788)";
  c.direct_execute(cmd, &xsink);
  if (xsink.isException()) {
    assert(false);
  }
}

//------------------------------------------------------------------------------
static void delete_image_table(bool quiet = false)
{
  const char* cmd =  "drop table image_table";

  connection c;
  ExceptionSink xsink;
  c.init(SYBASE_TEST_SETTINGS, &xsink);
  if (xsink.isException()) {
    assert(false);
  }
  c.direct_execute(cmd, &xsink);
  if (xsink.isException()) {
    if (quiet) {
      xsink.clear();
    } else {
      assert(false);
    }
  }
}

//------------------------------------------------------------------------------
TEST()
{
  printf("running test %s[%d]\n", __FILE__, __LINE__);
  delete_image_table(true);
  create_image_table();
  ON_BLOCK_EXIT(delete_image_table, false);

  connection conn;
  ExceptionSink xsink;
  conn.init(SYBASE_TEST_SETTINGS, &xsink);
  if (xsink.isException()) {
    assert(false);
  }
  command cmd(conn, &xsink);
  if (xsink.isException()) {
    assert(false);
  }
  cmd.initiate_language_command("select * from image_table", &xsink);
  if (xsink.isException()) {
    assert(false);
  }
  cmd.send(&xsink);
  if (xsink.isException()) {
    assert(false);
  }

  AbstractQoreNode* result = read_output(conn, cmd, QCS_DEFAULT, true, &xsink);
  if (xsink.isException()) {
    assert(false);
  }
  assert(result);
  assert(result->getType() == NT_HASH);
  assert(result->val.hash->size() == 1);
  
  result->deref(&xsink);
  if (xsink.isException()) {
    assert(false);
  }
  printf("test for image datatype is OK\n");
}

} // namespace
#endif

// EOF

