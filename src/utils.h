#ifndef SYBASE_SRC_UTILS_H
#define SYBASE_SRC_UTILS_H

#include <string>
#include <sstream>

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


}

#endif /* SYBASE_SRC_UTILS_H */
