
#ifndef TYPES_H
#define TYPES_H
#include <boost/json.hpp>
#include <map>
#include <msgpack.hpp>
#include <string>
#include <vector>

namespace k2eg::common {

#define DEFINE_PTR_TYPES(x) \
typedef std::unique_ptr<x> x##UPtr; \
typedef std::unique_ptr<const x> Const##x##UPtr; \
typedef std::shared_ptr<x> x##ShrdPtr; \
typedef std::shared_ptr<const x> Const##x##ShrdPtr; \
template<typename... Args> \
inline x##UPtr Make##x##UPtr(Args&&... __args) \
{ \
    return std::make_unique<x>(std::forward<Args>(__args)...); \
} \
template<typename... Args> \
inline x##ShrdPtr Make##x##ShrdPtr(Args&&... __args) \
{ \
    return std::make_shared<x>(std::forward<Args>(__args)...); \
}

#define DEFINE_VECTOR_FOR_TYPE(t, n)              \
    typedef std::vector<t> n;                     \
    typedef std::vector<t>::iterator n##Iterator; \
    typedef std::vector<t>::const_iterator n##ConstIterator;

#define DEFINE_MAP_FOR_TYPE(t1, t2, n)                         \
    typedef std::map<t1, t2> n;                                \
    typedef std::map<t1, t2>::iterator n##Iterator;            \
    typedef std::map<t1, t2>::const_iterator n##ConstIterator; \
    typedef std::pair<t1, t2> n##Pair;

#define DEFINE_UOMAP_FOR_TYPE(t1, t2, n)                         \
    typedef std::unordered_map<t1, t2> n;                                \
    typedef std::unordered_map<t1, t2>::iterator n##Iterator;            \
    typedef std::unordered_map<t1, t2>::const_iterator n##ConstIterator; \
    typedef std::pair<t1, t2> n##Pair;

#define DEFINE_MMAP_FOR_TYPE(t1, t2, n)                         \
    typedef std::multimap<t1, t2> n;                                \
    typedef std::multimap<t1, t2>::iterator n##Iterator;            \
    typedef std::multimap<t1, t2>::const_iterator n##ConstIterator; \
    typedef std::pair<t1, t2> n##Pair;

DEFINE_VECTOR_FOR_TYPE(std::string, StringVector);

DEFINE_MAP_FOR_TYPE(std::string, std::string, MapStrKV);
} // namespace k2eg::common
#endif // __TYPES_H__