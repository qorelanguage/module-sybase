#ifndef SYBASE_SRC_UTILS_H
#define SYBASE_SRC_UTILS_H

#include <string>
#include <sstream>
#include <memory>
#include <vector>

#include "error.h"

namespace ss {

template<typename T>
std::string string_cast(T val) {
    std::ostringstream oss;
    oss << val;
    return oss.str();
}

inline bool is_empty(const std::string &s) {
    return s.empty();
}

inline bool is_empty(const char *s) {
    if (!s) return true;
    if (*s == 0) return true;
    return false;
}

template<typename It>
void delete_range(It it, It eit) {
    for (;it != eit; ++it) {
        delete *it;
    }
}

template<typename Con>
void delete_container(Con &c) {
    delete_range(c.begin(), c.end());
}

template<typename T>
class SafePtr {
  public:
    SafePtr() {}

    explicit SafePtr(T *t) : p(t) {}

    void reset(T *t = 0) {
        p.reset(t);
    }

    T * release() {
        return p.release();
    }

    T * get() { return p.get(); }

    T * safe_get() {
        if (!p.get()) {
            throw ss::Error("TDS-SYBASE", "Not initialized");
        }
        return p.get();
    }

    T * operator->() {
        return safe_get();
    }

    T & operator*() {
        return *safe_get();
    }

  private:
    std::unique_ptr<T> p;
};



class RefHolderVector {
    typedef std::vector<AbstractQoreNode *> Vector;
public:
    RefHolderVector(ExceptionSink *xsink) :
        xsink(xsink)
    {}


    typedef Vector::iterator iterator;
    typedef Vector::const_iterator const_iterator;

    iterator begin() { return v.begin(); }
    iterator end() { return v.end(); }
    const_iterator begin() const { return v.begin(); }
    const_iterator end() const { return v.end(); }

    ~RefHolderVector() {
        clear();
    };

    void clear() {
        for (iterator it = begin(); it != end(); ++it) {
           discard((*it), xsink);
        }
        v.clear();
    }

    size_t size() const { return v.size(); }
    bool empty() const { return v.empty(); }

    void push_back(AbstractQoreNode *n) {
        v.push_back(n);
    }

    template<typename Fn>
    QoreHashNode * release_to_hash(Fn keygen) {
        int i = 0;
        ReferenceHolder<QoreHashNode> h(xsink);
        h = new QoreHashNode;
        for (iterator it = begin(); it != end(); ++it) {
            std::string key = keygen(i++);
            h->setKeyValue(key.c_str(), *it, xsink);
        }
        v.clear();
        return h.release();
    }

    template<typename Fn>
    AbstractQoreNode * release_smart(Fn keygen) {
        if (empty()) return 0;
        if (size() == 1) {
            AbstractQoreNode *rv = v[0];
            v.clear();
            return rv;
        }
        return release_to_hash(keygen);
    }

private:
    ExceptionSink *xsink;
    Vector v;
};

} // namespace ss

#endif /* SYBASE_SRC_UTILS_H */
