#!/usr/bin/env qore

# database test script
# databases users must be able to create and destroy tables and procedures, etc
# in order to execute all tests

%require-our
%enable-all-warnings
%new-style

our hash o;
our int errors;
our int test_count;

const opts = (
    "help"    : "h,help",
    "verbose" : "v,verbose:i+",
    "leave"   : "l,leave"
    );

sub usage() {
    printf("usage: %s [options] <db-string>
 -h,--help          this help text
 -v,--verbose       more v's = more information
 -l,--leave         leave test tables in schema at end\n",
           basename(ENV."_"));
    exit();
}

const object_map = (
    "sybase": (
        "tables": syb_tables,
        "procs": sybase_procs,
    ),
    "freetds": (
        "tables": freetds_sybase_tables,
        "procs": sybase_procs,
    ),
    );

const syb_tables = (
    "family" : "create table family (
   family_id int not null,
   name varchar(80) not null
)",
    "people" : "create table people (
   person_id int not null,
   family_id int not null,
   name varchar(250) not null,
   dob date not null
)",
    "attributes" : "create table attributes (
   person_id int not null,
   attribute varchar(80) not null,
   value varchar(160) not null
)",
    "data_test" : "create table data_test (
        null_f char(1) null,

        varchar_f varchar(40) not null,
        char_f char(40) not null,
        unichar_f unichar(40) not null,
        univarchar_f univarchar(40) not null,
        text_f text not null,
        unitext_f unitext not null, -- note that unitext is stored as 'image'

        bit_f bit not null,
        tinyint_f tinyint not null,
        smallint_f smallint not null,
        int_f int not null,
        int_f2 int not null,

        decimal_f decimal(10,4) not null,

        float_f float not null,     -- 8-bytes
        real_f real not null,       -- 4-bytes
        money_f money not null,
        smallmoney_f smallmoney not null,

        date_f date not null,
        time_f time not null,
        datetime_f datetime not null,
        smalldatetime_f smalldatetime not null,

        binary_f binary(4) not null,
        varbinary_f varbinary(4) not null,
        image_f image not null
)" );

const sybase_procs = (
    "find_family" :
"create procedure find_family @name varchar(80)
as
select * from family where name = @name
commit -- to maintain transaction count
",
    "get_values" :
"create procedure get_values @string varchar(80) output, @int int output
as
select @string = 'hello there'
select @int = 150
commit -- to maintain transaction count
",
    "get_values_and_select" :
"create procedure get_values_and_select @string varchar(80) output, @int int output
as
select @string = 'hello there'
select @int = 150
select * from family where family_id = 1
commit -- to maintain transaction count
",
    "get_values_and_multiple_select" :
"create procedure get_values_and_multiple_select @string varchar(80) output, @int int output
as
select @string = 'hello there'
select @int = 150
select * from family where family_id = 1
select * from people where person_id = 1
commit -- to maintain transaction count
",
    "just_select" :
"create procedure just_select
as
select * from family where family_id = 1
commit -- to maintain transaction count
",
    "multiple_select" :
"create procedure multiple_select
as
select * from family where family_id = 1
select * from people where person_id = 1
commit -- to maintain transaction count
"
 );

const freetds_sybase_tables = (
    "family" : "create table family (
   family_id int not null,
   name varchar(80) not null
)",
    "people" : "create table people (
   person_id int not null,
   family_id int not null,
   name varchar(250) not null,
   dob date not null
)",
    "attributes" : "create table attributes (
   person_id int not null,
   attribute varchar(80) not null,
   value varchar(160) not null
)",
    "data_test" : "create table data_test (
        null_f char(1) null,

        varchar_f varchar(40) not null,
        char_f char(40) not null,
        text_f text not null,
        unitext_f unitext not null, -- note that unitext is stored as 'image'

        bit_f bit not null,
        tinyint_f tinyint not null,
        smallint_f smallint not null,
        int_f int not null,
        int_f2 int not null,

        decimal_f decimal(10,4) not null,

        float_f float not null,     -- 8-bytes
        real_f real not null,       -- 4-bytes
        money_f money not null,
        smallmoney_f smallmoney not null,

        date_f date not null,
        time_f time not null,
        datetime_f datetime not null,
        smalldatetime_f smalldatetime not null,

        binary_f binary(4) not null,
        varbinary_f varbinary(4) not null,
        image_f image not null
)" );

const freetds_mssql_tables = (
    "family" : "create table family (
   family_id int not null,
   name varchar(80) not null
)",
    "people" : "create table people (
   person_id int not null,
   family_id int not null,
   name varchar(250) not null,
   dob datetime not null
)",
    "attributes" : "create table attributes (
   person_id int not null,
   attribute varchar(80) not null,
   value varchar(160) not null
)",
    "data_test" : "create table data_test (
        null_f char(1) null,

        varchar_f varchar(40) not null,
        char_f char(40) not null,
        text_f text not null,

        bit_f bit not null,
        tinyint_f tinyint not null,
        smallint_f smallint not null,
        int_f int not null,
        int_f2 int not null,

        decimal_f decimal(10,4) not null,

        float_f float not null,     -- 8-bytes
        real_f real not null,       -- 4-bytes
        money_f money not null,
        smallmoney_f smallmoney not null,

        datetime_f datetime not null,
        smalldatetime_f smalldatetime not null,

        binary_f binary(4) not null,
        varbinary_f varbinary(4) not null,
        image_f image not null
)" );

sub parse_command_line() {
    GetOpt g(opts);
    o = g.parse(\ARGV);
    if (o.help)
        usage();

    if (!ARGV) {
        stderr.printf("missing connection string on the command-line: -h for help\n");
        exit(1);
    }

    o.conn = shift ARGV;
    *string db = (o.conn =~ x/^([^:]+):.+$/)[0];
    if (!db) {
        stderr.printf("missing database driver argument in connection string %yn", o.conn);
        exit(1);
    }
    if (db != "sybase" && db != "freetds") {
        stderr.printf("unsupported driver argument %y in connection string, expecting \"sybase\" or \"freetds\"n", db);
        exit(1);
    }
}

sub create_datamodel(Datasource db) {
    drop_test_datamodel(db);

    string driver = db.getDriverName();
    # create tables
    hash tables = object_map{driver}.tables;
    if (driver == "freetds")
        if (db.is_sybase)
            tables = freetds_sybase_tables;
        else
            tables = freetds_mssql_tables;

    on_success db.commit();
    on_error db.rollback();

    foreach string table in (tables.keyIterator()) {
        tprintf(2, "creating table %n\n", table);
        db.exec(tables{table});
    }

    # create procedures if any
    foreach my proc in (keys object_map{driver}.procs) {
        tprintf(2, "creating procedure %n\n", proc);
        db.exec(object_map{driver}.procs{proc});
    }

    # create functions if any
    foreach my func in (object_map{driver}.funcs.keyIterator()) {
        tprintf(2, "creating function %n\n", func);
        db.exec(object_map{driver}.funcs{func});
    }

    db.exec("insert into family values ( 1, 'Smith' )");
    db.exec("insert into family values ( 2, 'Jones' )");

    # we insert the dates here using binding by value so we don't have
    # to worry about each database's specific date format
    db.exec("insert into people values ( 1, 1, 'Arnie', %v)", 1983-05-13);
    db.exec("insert into people values ( 2, 1, 'Sylvia', %v)", 1994-11-10);
    db.exec("insert into people values ( 3, 1, 'Carol', %v)", 2003-07-23);
    db.exec("insert into people values ( 4, 1, 'Bernard', %v)", 1979-02-27);
    db.exec("insert into people values ( 5, 1, 'Isaac', %v)", 2000-04-04);
    db.exec("insert into people values ( 6, 2, 'Alan', %v)", 1992-06-04);
    db.exec("insert into people values ( 7, 2, 'John', %v)", 1995-03-23);

    db.exec("insert into attributes values ( 1, 'hair', 'blond' )");
    db.exec("insert into attributes values ( 1, 'eyes', 'hazel' )");
    db.exec("insert into attributes values ( 2, 'hair', 'blond' )");
    db.exec("insert into attributes values ( 2, 'eyes', 'blue' )");
    db.exec("insert into attributes values ( 3, 'hair', 'brown' )");
    db.exec("insert into attributes values ( 3, 'eyes', 'grey')");
    db.exec("insert into attributes values ( 4, 'hair', 'brown' )");
    db.exec("insert into attributes values ( 4, 'eyes', 'brown' )");
    db.exec("insert into attributes values ( 5, 'hair', 'red' )");
    db.exec("insert into attributes values ( 5, 'eyes', 'green' )");
    db.exec("insert into attributes values ( 6, 'hair', 'black' )");
    db.exec("insert into attributes values ( 6, 'eyes', 'blue' )");
    db.exec("insert into attributes values ( 7, 'hair', 'brown' )");
    db.exec("insert into attributes values ( 7, 'eyes', 'brown' )");
}

sub drop_test_datamodel(db) {
    my driver = db.getDriverName();
    # drop the tables and ignore exceptions
    # the commits are needed for databases like postgresql, where errors will prohibit and further
    # actions from being taken on the Datasource
    foreach my table in (keys object_map{driver}.tables)
        try {
            db.exec("drop table " + table);
            db.commit();
            tprintf(2, "dropped table %n\n", table);
        }
        catch () {
            db.commit();
        }

    # drop procedures and ignore exceptions
    foreach string proc in (keys object_map{driver}.procs) {
        string cmd = object_map{driver}.drop_proc_cmd ?? "drop procedure";
        try {
            db.exec(cmd + " " + proc);
            db.commit();
            tprintf(2, "dropped procedure %n\n", proc);
        }
        catch () {
            db.commit();
        }
    }

    # drop functions and ignore exceptions
    foreach string func in (keys object_map{driver}.funcs) {
        string cmd = object_map{driver}.drop_func_cmd ?? "drop function";
        try {
            db.exec(cmd + " " + func);
            db.commit();
            tprintf(2, "dropped function %n\n", func);
        }
        catch () {
            db.commit();
        }
    }
}

Datasource sub getDS() {
    return new Datasource(o.conn);
}

sub tprintf(v, msg) {
    if (v <= o.verbose)
        vprintf(msg, argv);
}

sub test_value(any v1, any v2, string msg) {
    ++test_count;
    if (v1 == v2) {
        tprintf(1, "OK: %s test\n", msg);
    } else {
        tprintf(0, "ERROR: %s test failed! (%y (%s) != %y (%s))\n", msg, v1, v1.type(), v2, v2.type());
        errors++;
    }
}

sub test_value_eo(any v1, any v2, string msg) {
    if (v1 == v2)
        return True;
    tprintf(0, "ERROR: %s test failed! (%y (%s) != %y (%s))\n", msg, v1, v1.type(), v2, v2.type());
    return False;
}

const family_hash = (
  "Jones" : (
      "people" : (
          "John" : (
              "dob" : 1995-03-23,
              "eyes" : "brown",
              "hair" : "brown" ),
          "Alan" : (
              "dob" : 1992-06-04,
              "eyes" : "blue",
              "hair" : "black" ) ) ),
    "Smith" : (
        "people" : (
            "Arnie" : (
                "dob" : 1983-05-13,
                "eyes" : "hazel",
                "hair" : "blond" ),
            "Carol" : (
                "dob" : 2003-07-23,
                "eyes" : "grey",
                "hair" : "brown" ),
            "Isaac" : (
                "dob" : 2000-04-04,
                "eyes" : "green",
                "hair" : "red" ),
            "Bernard" : (
                "dob" : 1979-02-27,
                "eyes" : "brown",
                "hair" : "brown" ),
            "Sylvia" : (
                "dob" : 1994-11-10,
                "eyes" : "blue",
                "hair" : "blond" ) ) ) );

sub context_test(Datasource db) {
    # first we select all the data from the tables and then use
    # context statements to order the output hierarchically

    # context statements are most useful when a set of queries can be executed once
    # and the results processed many times by creating "views" with context statements

    my people = db.select("select * from people");
    my attributes = db.select("select * from attributes");

    # in this test, we create a big hash structure out of the queries executed above
    # and compare it at the end to the expected result

    # display each family sorted by family name
    my fl;
    context family (db.select("select * from family")) sortBy (%name) {
        my pl;
        tprintf(2, "Family %d: %s\n", %family_id, %name);

        # display people, sorted by eye color, descending
        context people (people)
            sortDescendingBy (find %value in attributes
                              where (%attribute == "eyes"
                                     && %person_id == %people:person_id))
            where (%family_id == %family:family_id) {
            my al;
            tprintf(2, "  %s, born %s\n", %name, format_date("Month DD, YYYY", %dob));
            context (attributes) sortBy (%attribute) where (%person_id == %people:person_id) {
                al.%attribute = %value;
                tprintf(2, "    has %s %s\n", %value, %attribute);
            }
            # leave out the ID fields and name from hash under name; subtracting a
            # string from a hash removes that key from the result
            # this is "doing it the hard way", there is only one key left,
            # "dob", then attributes are added directly into the person hash
            pl.%name = %% - "family_id" - "person_id" - "name" + al;
        }
        # leave out family_id and name fields (leaving an empty hash)
        fl.%name = %% - "family_id" - "name" + ( "people" : pl );
    }

    # test context ordering
    test_value(keys fl, ("Jones", "Smith"), "first context");
    test_value(keys fl.Smith.people, ("Arnie", "Carol", "Isaac", "Bernard", "Sylvia"), "second context");
    # test entire context value
    test_value(fl, family_hash, "third context");
}

sub test_timeout(db, c) {
    db.setTransactionLockTimeout(1ms);
    try {
        # this should cause a TRANSACTION-LOCK-TIMEOUT exception to be thrown
        db.exec("insert into family values (3, 'Test')\n");
        test_value(True, False, "transaction timeout");
        db.exec("delete from family where name = 'Test'");
    }
    catch (ex) {
        test_value(True, True, "transaction timeout");
    }
    # signal parent thread to continue
    c.dec();
}

sub transaction_test(Datasource db) {
    Datasource ndb = getDS();
    tprintf(2, "db.autocommit=%N, ndb.autocommit=%N\n", db.getAutoCommit(), ndb.getAutoCommit());

    # first, we insert a new row into "family" but do not commit it
    int rows = db.exec("insert into family values (3, 'Test')\n");
    if (rows !== 1)
        printf("FAILED INSERT, rows=%y\n", rows);

    # now we verify that the new row is visible to the inserting datasource
    *string name = db.selectRow("select name from family where family_id = 3").name;
    test_value(name, "Test", "second transaction");

    # test datasource timeout
    # this Counter variable will allow the parent thread to sleep
    # until the child thread times out
    Counter c(1);
    background test_timeout(db, c);

    # wait for child thread to time out
    c.waitForZero();

    # now, we commit the transaction
    db.commit();

    # now we verify that the new row is visible in the other datasource
    name = ndb.selectRow("select name from family where family_id = 3").name;
    test_value(name, "Test", "third transaction");

    # now we delete the row we inserted (so we can repeat the test)
    int cnt = ndb.exec("delete from family where family_id = 3");
    test_value(cnt, 1, "delete row count");
    ndb.commit();
}

sub oracle_test() {
}

# here we use a little workaround for modules that provide functions,
# namespace additions (constants, classes, etc) needed by test functions
# at parse time.  To avoid parse errors (as database modules are loaded
# in this script at run-time when the Datasource class is instantiated)
# we use a Program object that we parse and run on demand to return the
# value required
sub get_val(code) {
    Program p();
    string str = sprintf("return %s;", code);
    p.parse(str, "code");
    return p.run();
}

const family_q = (
    "family_id" : 1,
    "name" : "Smith",
    );
const person_q = (
    "person_id" : 1,
    "family_id" : 1,
    "name" : "Arnie",
    "dob" : 1983-05-13,
    );
const params = (
    "string" : "hello there",
    "int" : 150,
    );

sub sybase_test(db) {
    # simple stored proc test, bind by name
    *hash x = db.exec("exec find_family %v", "Smith");
    test_value(x, ("name": list("Smith"), "family_id" : list(1)), "simple stored proc");

    # stored proc execute with output params
    x = db.exec("declare @string varchar(40), @int int
exec get_values :string output, :int output");
    test_value(x, params, "get_values");

    # we use Datasource::selectRows() in the following queries because we
    # get hash results instead of a hash of lists as with exec in the queries
    # normally we should not use selectRows to execute a stored procedure,
    # as the Datasource::selectRows() method will not grab the transaction lock,
    # but we already called Datasource::exec() above, so we have it already.
    # the other alternative would be to call Datasource::beginTransaction() before
    # Datasource::selectRows()

    # simple stored proc test, bind by name, returns hash
    x = db.selectRows("exec find_family %v", "Smith");
    test_value(x, family_q, "simple stored proc");

    # stored proc execute with output params and select results
    x = db.selectRows("declare @string varchar(40), @int int
exec get_values_and_select :string output, :int output");
    test_value(x, ("query":family_q,"params":params), "get_values_and_select");

    # stored proc execute with output params and multiple select results
    x = db.selectRows("declare @string varchar(40), @int int
exec get_values_and_multiple_select :string output, :int output");
    test_value(x, ("query":("query0":family_q,"query1":person_q),"params":params), "get_values_and_multiple_select");

    # stored proc execute with just select results
    x = db.selectRows("exec just_select");
    test_value(x, family_q, "just_select");

    # stored proc execute with multiple select results
    x = db.selectRows("exec multiple_select");
    test_value(x, ("query0":family_q,"query1":person_q), "multiple_select");

    hash args = (
        "null_f"          : NULL,
        "varchar_f"       : "varchar",
        "char_f"          : "char",
        "unichar_f"       : "unichar",
        "univarchar_f"    : "univarchar",
        "text_f"          : "test",
        "unitext_f"       : "test",
        "bit_f"           : True,
        "tinyint_f"       : 55,
        "smallint_f"      : 4285,
        "int_f"           : 405402,
        "int_f2"          : 214123498,
        "decimal_f"       : 500.1231, # TODO: make it work with 500.1231n
        "float_f"         : 23443.234324234,
        "real_f"          : 213.123,
        "money_f"         : 3434234250.2034,
        "smallmoney_f"    : 211100.1012,
        "date_f"          : 2007-05-01,
        "time_f"          : 10:30:01,
        "datetime_f"      : 3459-01-01T11:15:02.250,
        "smalldatetime_f" : 2007-12-01T12:01:00,
        "binary_f"        : <0badbeef>,
        "varbinary_f"     : <feedface>,
        "image_f"         : <cafebead>,
        );

    # insert data
    db.vexec("insert into data_test values (%v, %v, %v, %v, %v, %v, %v, %v, %v, %v, %v, %d, %v, %v, %v, %v, %v, %v, %v, %v, %v, %v, %v, %v)", hash_values(args));

    *hash q = db.selectRow("select * from data_test");
    if (o.verbose > 1)
        foreach string k in (q.keyIterator())
            tprintf(2, " %-16s= %-10s %N\n", k, type(q{k}), q{k});

    # remove values where we know they won't match
    # unitext_f is returned as IMAGE by the server
    delete q.unitext_f;
    delete args.unitext_f;
    # rounding errors can happen in real
    q.real_f = round(q.real_f);
    args.real_f = round(args.real_f);

    # compare each value
    foreach string k in (q.keyIterator())
        test_value(q{k}, args{k}, sprintf("%s bind and retrieve", k));

    db.commit();
}

sub freetds_test(Datasource db) {
    # simple stored proc test, bind by name
    hash x = db.exec("exec find_family %v", "Smith");
    test_value(x, ("name": list("Smith"), "family_id" : list(1)), "simple stored proc");

    # we cannot retrieve parameters from newer SQL Servers with the approach we use;
    # Microsoft changed the handling of the protocol and require us to use RPC calls,
    # this will be implemented in the next version of qore where the "freetds" driver will
    # be able to add custom methods to the Datasource class.  For now, we skip these tests

    if (db.is_sybase) {
        x = db.exec("declare @string varchar(40), @int int
exec get_values :string output, :int output");
        test_value(x, params, "get_values");
    }

    # we use Datasource::selectRows() in the following queries because we
    # get hash results instead of a hash of lists as with exec in the queries
    # normally we should not use selectRows to execute a stored procedure,
    # as the Datasource::selectRows() method will not grab the transaction lock,
    # but we already called Datasource::exec() above, so we have it already.
    # the other alternative would be to call Datasource::beginTransaction() before
    # Datasource::selectRows()

    # simple stored proc test, bind by name, returns hash
    x = db.selectRows("exec find_family %v", "Smith");
    test_value(x, family_q, "simple stored proc");

    # stored proc execute with output params and select results
    if (db.is_sybase) {
        x = db.selectRows("declare @string varchar(40), @int int
exec get_values_and_select :string output, :int output");
        test_value(x, ("query":family_q,"params":params), "get_values_and_select");

        # stored proc execute with output params and multiple select results
        x = db.selectRows("declare @string varchar(40), @int int
exec get_values_and_multiple_select :string output, :int output");
        test_value(x, ("query":("query0":family_q,"query1":person_q),"params":params), "get_values_and_multiple_select");
    }

    # stored proc execute with just select results
    x = db.selectRows("exec just_select");
    test_value(x, family_q, "just_select");

    # stored proc execute with multiple select results
    x = db.selectRows("exec multiple_select");
    test_value(x, ("query0":family_q,"query1":person_q), "multiple_select");

    # the freetds driver does not work with the following sybase column types:
    # unichar, univarchar

    hash args = (
        "null_f"          : NULL,
        "varchar_f"       : "test",
        "char_f"          : "test",
        "text_f"          : "test",
        "unitext_f"       : "test",
        "bit_f"           : True,
        "tinyint_f"       : 55,
        "smallint_f"      : 4285,
        "int_f"           : 405402,
        "int_f2"          : 214123498,
        "decimal_f"       : 500.1231n,
        "float_f"         : 23443.234324234,
        "real_f"          : 213.123,
        "money_f"         : 3434234250.2034,
        "smallmoney_f"    : 211100.1012,
        "date_f"          : 2007-05-01,
        "time_f"          : 10:30:01,
        "datetime_f"      : 3459-01-01T11:15:02.250,
        "smalldatetime_f" : 2007-12-01T12:01:00,
        "binary_f"        : <0badbeef>,
        "varbinary_f"     : <feedface>,
        "image_f"         : <cafebead>,
        );

    # remove fields not supported by sql server
    if (!db.is_sybase) {
        delete args.unitext_f;
        delete args.date_f;
        delete args.time_f;
    }

    string sql = "insert into data_test values (" + (foldl $1 + "," + $2, (map "%v", args.iterator())) + ")";
    
    # insert data, using the values from the hash above
    db.vexec(sql, hash_values(args));

    *hash q = db.selectRow("select * from data_test");
    if (o.verbose > 1)
        foreach string k in (q.keyIterator())
            tprintf(2, " %-16s= %-10s %N\n", k, type(q{k}), q{k});

    # remove values where we know they won't match
    # unitext_f is returned as IMAGE by the server
    delete q.unitext_f;
    delete args.unitext_f;
    # rounding errors can happen in real
    q.real_f = round(q.real_f);
    args.real_f = round(args.real_f);

    # convert "decimal_f" values to float so that the comparisons succeed
    q.decimal_f = float(q.decimal_f);
    args.decimal_f = float(q.decimal_f);
    
    # compare each value
    foreach string k in (q.keyIterator())
        test_value(q{k}, args{k}, sprintf("%s bind and retrieve", k));

    db.commit();

    statement_test(db);
}

sub statement_test(Datasource db) {
    any res1;
    any res2;
    any res3;
    string query = "select * from people";
    {
    res1 = db.selectRows(query);
    SQLStatement stmt(db);
    stmt.prepare(query);
    res2 = list();
    while (stmt.next()) {
        any row = stmt.fetchRow();
        push res2, row;
    }
    stmt.commit();
    }

    SQLStatement stmt(db);
    stmt.prepare(query);
    stmt.next();
    res3 = stmt.fetchRows();
    
    test_value(res1, res2, "select by statement");
    #test_value(res1, res3, "select by statement fetchRows");
    test_value(res1.size(), 7, "selected cnt");
    stmt.commit();
}

sub main() {
    hash test_map = (
        "sybase": \sybase_test(),
        "freetds": \freetds_test(),
        );

    parse_command_line();
    Datasource db = getDS();

    my driver = db.getDriverName();
    printf("testing %s driver\n", driver);
    my sv = db.getServerVersion();
    if (o.verbose > 1)
        tprintf(2, "client version=%n\nserver version=%n\n", db.getClientVersion(), sv);

    # determine if the server is a sybase or sql server dataserver
    if (driver == "freetds")
        if (sv !~ /microsoft/i)
            db.is_sybase = True;

    create_datamodel(db);

    context_test(db);
    transaction_test(db);
    *code test = test_map.(db.getDriverName());
    if (test)
        test(db);

    if (!o.leave)
        drop_test_datamodel(db);
    printf("%d/%d tests OK\n", test_count - errors, test_count);
}

main();
