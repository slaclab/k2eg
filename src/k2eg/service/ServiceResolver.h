#ifndef __SERVICERESOLVER_H__
#define __SERVICERESOLVER_H__

#include <memory>

namespace k2eg::service
{
    template <typename T>
    class ServiceResolver
    {
        static std::shared_ptr<T> registered_instance;

    public:
        static void registerService(std::shared_ptr<T> object)
        {
            registered_instance = object;
        }
        static std::shared_ptr<T> resolve()
        {
            return registered_instance;
        }
        static void reset()
        {
            registered_instance.reset();
        }
    };

    template <typename T>
    std::shared_ptr<T> ServiceResolver<T>::registered_instance;
}

#endif // __SERVICERESOLVER_H__