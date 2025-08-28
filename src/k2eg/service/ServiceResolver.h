#ifndef __SERVICERESOLVER_H__
#define __SERVICERESOLVER_H__

#include <memory>
#include <any>

namespace k2eg::service
{
    template <typename T>
    class ServiceResolver
    {
        // Currently resolved/shared singleton instance
        static std::shared_ptr<T> registered_instance;
        // Opaque configuration associated to this service family
        static std::any configuration;

    public:
        /**
         * @brief Register a concrete service instance to be used as the shared/default one.
         */
        static void registerService(std::shared_ptr<T> object)
        {
            registered_instance = std::move(object);
        }

        /**
         * @brief Get the currently registered shared/default instance.
         */
        static std::shared_ptr<T> resolve()
        {
            return registered_instance;
        }

        /**
         * @brief Store/replace the configuration for this service family.
         */
        template <typename Cfg>
        static void setConfiguration(Cfg cfg)
        {
            configuration = std::move(cfg);
        }

        /**
         * @brief Retrieve a pointer to the stored configuration casted to Cfg, or nullptr.
         */
        template <typename Cfg>
        static const Cfg* getConfiguration()
        {
            return std::any_cast<Cfg>(&configuration);
        }

        /**
         * @brief Reset the registered instance and clear configuration.
         */
        static void reset()
        {
            registered_instance.reset();
            configuration.reset();
        }
    };

    template <typename T>
    std::shared_ptr<T> ServiceResolver<T>::registered_instance;
    template <typename T>
    std::any ServiceResolver<T>::configuration;
}

#endif // __SERVICERESOLVER_H__
