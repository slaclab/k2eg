#ifndef K2EG_COMMON_OBJECTFACTORY_H_
#define K2EG_COMMON_OBJECTFACTORY_H_

#include <map>
#include <memory>

namespace k2eg::common {
    // Factory that return a shared ptr of an object class associated to a type
    template <typename T, typename C>
    class ObjectByTypeFactory
    {
        std::map<T, std::shared_ptr<C>> typed_instances;
    public:
        void registerObjectInstance(const T type, std::shared_ptr<C> object)
        {
            if(!object){
                throw std::runtime_error("registerObjectInstance Object is null");
            }
            typed_instances.insert(std::pair<T, std::shared_ptr<C>>(type, object));
        }
        std::shared_ptr<C> resolve(const T type)
        {   
            return typed_instances[type];
        }

        bool hasType(T type) {
            auto i = typed_instances.find(type);
            return i != std::end(typed_instances);
        }
    };

}

#endif // K2EG_COMMON_OBJECTFACTORY_H_