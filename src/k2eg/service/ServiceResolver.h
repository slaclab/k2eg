#ifndef __SERVICERESOLVER_H__
#define __SERVICERESOLVER_H__

#include <any>
#include <functional>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <string>

namespace k2eg::service {
template <typename T>
class ServiceResolver
{
    // Currently resolved/shared singleton instance
    static std::shared_ptr<T> registered_instance;
    // Opaque configuration associated to this service family
    static std::any configuration;
    static std::function<std::shared_ptr<T>(
        const std::any&,
        const std::unordered_map<std::string, std::any>&)> creator;

public:
    /**
     * @brief Register and construct the shared/default instance from configuration.
     * Captures concrete type D to allow future fresh instance creation without passing types.
     * @tparam Cfg The configuration type (e.g., ConstPublisherConfigurationShrdPtr).
     * @tparam D   The concrete derived type of T to instantiate.
     */
    template <typename Cfg, typename D, typename = std::enable_if_t<std::is_base_of_v<T, D>>>
    static void registerService(Cfg cfg)
    {
        configuration = cfg;
        registered_instance = std::static_pointer_cast<T>(std::make_shared<D>(cfg));
        creator = [](const std::any& stored, const std::unordered_map<std::string, std::any>& overrides) -> std::shared_ptr<T>
        {
            if (auto c = std::any_cast<Cfg>(&stored))
            {
                // compile-time check for constructor availability
                if constexpr (std::is_constructible_v<D, Cfg, const std::unordered_map<std::string, std::any>&>)
                {
                    return std::static_pointer_cast<T>(std::make_shared<D>(*c, overrides));
                }
                else
                {
                    return std::static_pointer_cast<T>(std::make_shared<D>(*c));
                }
            }
            return nullptr;
        };
    }

    /**
     * @brief Register using a zero-arg factory. Useful for factory-based services.
     * The factory is responsible for capturing any needed configuration.
     */
    static void registerFactory(std::function<std::shared_ptr<T>()> factory)
    {
        // Build and store the singleton instance via the provided factory
        registered_instance = factory();
        // Install a creator that uses the zero-arg factory for fresh instances
        creator = [factory](const std::any&, const std::unordered_map<std::string, std::any>&) -> std::shared_ptr<T>
        {
            return factory();
        };
    }

    /**
     * @brief Register using a context + member getter + builder function, no lambdas at callsite.
     * @tparam Cfg    Configuration type returned by Getter and consumed by Builder.
     * @tparam Ctx    Context type that owns the getter (e.g., ProgramOptions).
     * @tparam Getter Member function pointer or callable: Cfg (Ctx::*)() or compatible.
     * @tparam Builder Free/static function or callable: std::shared_ptr<T>(Cfg).
     */
    template <typename Cfg, typename Ctx, typename Getter, typename Builder>
    static void registerFactory(Ctx* ctx, Getter getter, Builder builder)
    {
        auto factory = [ctx, getter, builder]()
        {
            if constexpr (std::is_member_function_pointer_v<Getter>)
            {
                return builder((ctx->*getter)());
            }
            else
            {
                return builder(getter(*ctx));
            }
        };
        registerFactory(factory);
    }

    // No other registration overloads: singleton is always constructed internally from configuration

    /**
     * @brief Get the currently registered shared/default instance.
     */
    static std::shared_ptr<T> resolve()
    {
        return registered_instance;
    }

    /**
     * @brief Create a new service instance using the stored configuration.
     * @tparam Cfg The exact configuration type stored (e.g., ConstPublisherConfigurationShrdPtr).
     * @return Freshly created instance, or nullptr if configuration is missing/mismatched.
     */
    static std::shared_ptr<T> createNewInstance(const std::unordered_map<std::string, std::any>& overrides = {})
    {
        return (creator && configuration.has_value()) ? creator(configuration, overrides) : nullptr;
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
        creator = nullptr;
    }
};

template <typename T>
std::shared_ptr<T> ServiceResolver<T>::registered_instance;
template <typename T>
std::any ServiceResolver<T>::configuration;
template <typename T>
std::function<std::shared_ptr<T>(
    const std::any&,
    const std::unordered_map<std::string, std::any>&)> ServiceResolver<T>::creator;
} // namespace k2eg::service

#endif // __SERVICERESOLVER_H__
