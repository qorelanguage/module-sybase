#ifndef SYBASE_SRC_RESULTFACTORY_H
#define SYBASE_SRC_RESULTFACTORY_H

#include <sstream>

#include "qore/Qore.h"

namespace ss {

class ResultFactory {
    static std::string keygen(int i) {
        std::ostringstream oss;
        oss << "query" << i;
        return oss.str();
    }

    enum LastType {
        NONE, PARAM, QUERY
    };

public:
    ResultFactory(ExceptionSink *xsink) :
        params(xsink),
        reslist(xsink),
        xsink(xsink),
        lasttype(NONE)
    { }

    void add(ValueHolder& rh, bool list = true) {
        add(rh.release(), list);
    }

    void add_params(ValueHolder& rh) {
        add_params(rh.release());
    }

    void add_params(QoreValue tt) {
        lasttype = PARAM;
        params.push_back(tt);
        last = tt;
    }

    void add(QoreValue tt, bool list = true) {
        lasttype = QUERY;
        reslist.push_back(tt);
        last = tt;
    }

    // call on CS_CMD_DONE
    void done(int rowcount) {
        if (rowcount <= 0) {
            last = QoreValue();
            return;
        }

        if (!last) {
            reslist.push_back(rowcount);
            return;
        }

        last = QoreValue();
    }

    QoreValue res() {
        if (params.empty()) {
            return reslist.release_smart(keygen);
        }

        if (reslist.empty()) {
            return params.release_smart(keygen);
        }

        ReferenceHolder<QoreHashNode> rv(xsink);
        rv = new QoreHashNode(autoTypeInfo);

        rv->setKeyValue("query", reslist.release_smart(keygen), xsink);
        rv->setKeyValue("params", params.release_smart(keygen), xsink);
        return rv.release();
    }

private:
    RefHolderVector params;
    RefHolderVector reslist;
    // true if some resuld was added before DONE
    QoreValue last;
    ExceptionSink *xsink;
    LastType lasttype;
};

} // nemespace ss

#endif /* SYBASE_SRC_RESULTFACTORY_H */
