
#ifndef TYPES_H
#define TYPES_H
#include <string>
#include <vector>
#include <map>
#include <memory>
namespace k2eg::common
{
#define DEFINE_PTR_TYPES(x) \
typedef std::unique_ptr<x> x##UPtr; \
typedef std::unique_ptr<const x> Const##x##UPtr; \
typedef std::shared_ptr<x> x##ShrdPtr; \
typedef std::shared_ptr<const x> Const##x##ShrdPtr; \
template<typename... _Args> \
inline x##UPtr Make##x##UPtr(_Args&&... __args) \
{ \
    return std::make_unique<x>(__args...); \
} \
template<typename... _Args> \
inline x##ShrdPtr Make##x##ShrdPtr(_Args&&... __args) \
{ \
    return std::make_shared<x>(__args...); \
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

    DEFINE_VECTOR_FOR_TYPE(std::string, StringVector);

    DEFINE_MAP_FOR_TYPE(std::string, std::string, MapStrKV);
}
#endif // __TYPES_H__