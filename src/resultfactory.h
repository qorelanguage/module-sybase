#ifndef SYBASE_SRC_RESULTFACTORY_H
#define SYBASE_SRC_RESULTFACTORY_H

#include <sstream>
#include <qore/QoreBigIntNode.h>
namespace ss {

class ResultFactory {
public:
    ResultFactory(ExceptionSink *xsink) :
        xsink(xsink),
        last(xsink),
        reslist(xsink),
        dirty(false)
    {}

    void add(ReferenceHolder<AbstractQoreNode> &rh) {
        add(*rh);
        rh.release();
    }

    // insert new list/hash to the result
    void add(AbstractQoreNode *tt) {
        ReferenceHolder<AbstractQoreNode> t(tt, xsink);
        dirty = true;
        if (last) {
            if (!reslist) reslist = new QoreHashNode();
            std::ostringstream oss;
            oss << "query" << reslist->size();
            reslist->setKeyValue(oss.str().c_str(),
                    last.release(), xsink);
        }

        if (!t) return;
        last = t.release();
    }

    // call on CS_CMD_DONE
    void done(int rowcount) {
        if (dirty) {
            dirty = false;
            return;
        }

        // Datasource::getServerVersion workaround
        if (rowcount >= 0)
            add(new QoreBigIntNode(rowcount));
    }

    bool is_dirty() const { return dirty; }

    AbstractQoreNode * res() {
        if (!reslist) {
            return last.release();
        }
        add(0);
        return reslist.release();
    }

private:
    ExceptionSink *xsink;
    ReferenceHolder<AbstractQoreNode> last;
    ReferenceHolder<QoreHashNode> reslist;
    // true if some resuld was added before DONE
    bool dirty;
};

} // nemespace ss

#endif /* SYBASE_SRC_RESULTFACTORY_H */
