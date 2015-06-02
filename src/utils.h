#ifndef SYBASE_SRC_UTILS_H
#define SYBASE_SRC_UTILS_H

#include <string>
#include <sstream>
#include <memory>

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
            throw ss::Error("DBI:SYBASE", "Not initialized");
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
    std::auto_ptr<T> p;
};


}

#endif /* SYBASE_SRC_UTILS_H */
