#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-


%require-our
%new-style
    
our hash thash;
our int errors;
our hash o.verbose = True;
o.info = True;

sub main() {
    *string connstr = shift ARGV;

    if (!connstr) {
        connstr = ENV.QORE_DB_CONNSTR;
        if (!connstr) {
            string hn = gethostname();
            switch (hn) {
                case /^el6/:
                case /^el5/:
                case /^manatee/:
                case /^xbox/:
                    connstr = "freetds:test/test@mssql";
                    break;
                case /^quasar/:
                    connstr = "freetds:test/test@mssqlvm1";
                    break;
                default: {
                    stderr.printf("cannot detect DB connect string from hostname and QORE_DB_CONNSTR environment variable not set; cannot run SqlUtil tests\n");
                    return;
                }
            }
        }
    }

    if (connstr !~ /^[a-z0-9_]+:/)
        connstr = "freetds:" + connstr;
    DatasourcePool ds(connstr);

    SQLStatement insert_stmt(ds);
    insert_stmt.prepare("insert into test values (%v, %v, %v, %v)");

    SQLStatement select_stmt(ds);
    select_stmt.prepare("select * from test");

    SQLStatement select_into_stmt(ds);
    select_into_stmt.prepare("select getdate() into :dt");

    SQLStatement select_err_stmt(ds);
    select_err_stmt.prepare("select * from blah");

    SQLStatement delete_stmt(ds);
    delete_stmt.prepare("delete from test");

    SQLStatement tsql(ds);
    tsql.prepare("create procedure get_count @count int output, @status varchar(80)
select count(1) into @count from user_objects where status = @status;
");

    create_table(ds);
    test1(insert_stmt, select_stmt);
    #test2(select_into_stmt);
    test3(select_err_stmt);
    test4(delete_stmt, select_stmt);
    #test5(tsql);
}

sub info(string msg) {
    if (o.info)
        vprintf(msg + "\n", argv);
}

sub test_value(any v1, any v2, string msg) {
    if (v1 === v2) {
        if (o.verbose)
            printf("OK: %s test\n", msg);
    }
    else {
        printf("ERROR: %s test failed! (%N != %N)\n", msg, v1, v2);
        #printf("%s%s", dbg_node_info(v1), dbg_node_info(v2));
        ++errors;
    }
    thash{msg} = True;
}

sub create_table(DatasourcePool dsp) {
    SQLStatement drop_stmt(dsp);
    drop_stmt.prepare("drop table test");

    on_exit drop_stmt.commit();
    
    try {
        drop_stmt.exec();
    }
    catch (hash ex) {
        info("ignoring drop table error (%s: %s)", ex.err, ex.desc);
    }

    drop_stmt.prepare("create table test (dt datetime, d date, t time, sdt smalldatetime)");
    drop_stmt.exec();
    printf("done\n");
}

sub test1(SQLStatement insert_stmt, SQLStatement select_stmt) {
    info("test1");
    #TimeZone::setRegion("America/Chicago");

    date now = now_us();
    info("current time: %n", now);

    binary bin = binary("hello");

    {
        info("starting insert");

        code insert = sub () {
            insert_stmt.bind(now, now, now, now);
            info("bind done: %y", now);

            insert_stmt.beginTransaction();
            on_success insert_stmt.commit();
            insert_stmt.exec();
            test_value(insert_stmt.affectedRows(), 1, "affected rows after insert");
        };

        insert();

        now = now_us();
        insert();

        now = now_us();
        insert();

        now = now_us();
        insert();
    }
    info("commit done");

    on_exit select_stmt.commit();

    select_stmt.exec();
    int rows;
    while (select_stmt.next()) {
        ++rows;
        hash rv = select_stmt.fetchRow();
        info("row: %y", rv);
    }

    test_value(rows, 4, "next and fetch");

    select_stmt.close();
    info("closed");

    select_stmt.exec();
    any v = select_stmt.fetchRows(-1);
    test_value(elements v, 4, "fetch rows");
    info("rows: %N", v);

    select_stmt.close();
    info("closed");

    select_stmt.exec();
    v = select_stmt.fetchColumns(-1);
    test_value(v.size(), 4, "fetch columns");

    select_stmt.exec();
    v = select_stmt.fetchColumns(-1);
    test_value(v.size(), 4, "fetch columns 2");
    info("columns: %N", v);
}

sub test2(SQLStatement stmt) {
    info("test2");
    on_exit stmt.commit();

    stmt.bindPlaceholders(Type::Date);
    #stmt.exec();
    any v = stmt.getOutput();
    test_value(elements v, 1, "first get output");
    info("output: %n", v);

    stmt.close();
    info("closed");

    stmt.bindPlaceholders(Type::Date);
    #stmt.exec();
    v = stmt.getOutput();
    test_value(elements v, 1, "second get output");
    info("output: %n", v);
}

sub test3(SQLStatement stmt) {
    info("test3");
    try {
        stmt.beginTransaction();
        info("begin transaction");
        stmt.exec();
        test_value(True, False, "first negative select");
    }
    catch (ex) {
        info("%s: %s", ex.err, ex.desc);
        test_value(True, True, "first negative select");
    }
    try {
        on_success stmt.commit();
        on_error stmt.rollback();
        any rv = stmt.fetchColumns(-1);
        info("ERR rows=%N", rv);
        test_value(True, False, "second negative select");
    }
    catch (ex) {
        info("%s: %s", ex.err, ex.desc);
        test_value(True, True, "second negative select");
    }
    info("rollback done");
}

sub test4(SQLStatement delete_stmt, SQLStatement select_stmt) {
    info("test4");
    {
        delete_stmt.beginTransaction();
        on_success delete_stmt.commit();
        delete_stmt.exec();
        test_value(4, delete_stmt.affectedRows(), "affected rows");
        delete_stmt.close();
    }
    info("commit done");

    select_stmt.exec();
    on_exit select_stmt.commit();
    any rv = select_stmt.fetchRows();
    test_value(rv, (), "select after delete");
    #info("test: %N", stmt.fetchRows());
}

sub test5(SQLStatement plsql) {
    info("test5");
    {
	plsql.exec();
	plsql.commit();

	on_exit {
	    plsql.prepare("drop procedure get_count");
	    plsql.exec();
	    plsql.commit();
	}
	
        plsql.beginTransaction();
        on_success plsql.commit();
	plsql.prepare("exec get_count %v");
        plsql.execArgs('VALID');
        hash ret = plsql.getOutput();
        #printf("%N\n", ret);
        plsql.close();
    }
    info("fetch done");
}

main();
