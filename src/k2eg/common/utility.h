#ifndef k2eg_COMMON_UTILITY_H_
#define k2eg_COMMON_UTILITY_H_

#include <memory>
#include <boost/format.hpp>
namespace k2eg
{
    namespace common
    {
        template <typename T>
        std::shared_ptr<T> toShared(std::weak_ptr<T> w)
        {
            std::shared_ptr<T> s = w.lock();
            if (!s)
                throw std::runtime_error("Error getting shared ptr for : " + std::string(typeid(w).name()));
            return s;
        }
    }
}

// global stirng format utility using boost format waiting for std implementation
#define STRING_FORMAT(m, p) (boost::format(m) % p).str()
#endif // k2eg_COMMON_UTILITY_H_
