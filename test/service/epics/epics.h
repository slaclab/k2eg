#include <k2eg/common/MsgpackSerialization.h>
#include <msgpack/adaptor/string.hpp>
#include <msgpack/v1/adaptor/map.hpp>
#include <msgpack/v1/adaptor/vector.hpp>

#define INIT_PVA_PROVIDER()                                                                                                      \
  epics::pvAccess::Configuration::shared_pointer test_pva_conf     = epics::pvAccess::ConfigurationBuilder().push_env().build(); \
  std::unique_ptr<pvac::ClientProvider>          test_pva_provider = std::make_unique<pvac::ClientProvider>("pva", test_pva_conf);

#define INIT_CA_PROVIDER()                                                                                                      \
  epics::pvAccess::Configuration::shared_pointer test_ca_conf     = epics::pvAccess::ConfigurationBuilder().push_env().build(); \
    std::unique_ptr<pvac::ClientProvider> test_ca_provider = std::make_unique<pvac::ClientProvider>("ca", \
     epics::pvAccess::ConfigurationBuilder().add("PATH", "build/local/bin/linux-x86_64").push_map().push_env().build());

#define WHILE_OP(x, v) \
  do { std::this_thread::sleep_for(std::chrono::milliseconds(250)); } while (x->isDone() == v)

#define WHILE_MONITOR(monitor_op, cond) \
 do { monitor_op->poll(); } while (cond)

#define TIMEOUT(x, v, retry, sleep_m_sec) \
  int r = retry; \
  do { std::this_thread::sleep_for(std::chrono::milliseconds(sleep_m_sec)); r--;} while (x == v && r > 0)

template <typename Type>
std::unique_ptr<k2eg::common::MsgpackObject> msgpack_from_scalar(const std::string& key, const Type& value)
{
    std::map<std::string, Type> val = {{key, value}};
    // Create a MsgpackObjectWithZone to hold the packed data
    return std::make_unique<k2eg::common::MsgpackObjectWithZone>(val);
}

template <typename Type>
std::unique_ptr<k2eg::common::MsgpackObject> msgpack_from_object(const Type& value)
{
    // Create a MsgpackObjectWithZone to hold the packed data
    return std::make_unique<k2eg::common::MsgpackObjectWithZone>(value);
}

#define MOVE_MSGPACK_TYPED(f, t, x) std::move(msgpack_from_scalar<t>(f, x))
#define MOVE_MSGPACK_TYPED_OBJECT(t, x) std::move(msgpack_from_object<t>(x))

using msgpack_variant = msgpack::type::variant;

/**
 * @brief Utility for building an NTTable‐style MsgPack object in header‐only form.
 */
class NTTableBuilder
{
public:
    /// Add one more column name
    void addLabel(const std::string& label)
    {
        _labels.emplace_back(label);
    }

    /// Add multiple column names at once
    void addLabels(const std::vector<std::string>& labels)
    {
        for (auto const& lbl : labels)
        {
            addLabel(lbl);
        }
    }

    /**
     * @brief Add or replace the array of values for a given column
     * @param field   The column name
     * @param values  A vector of msgpack_variant holding scalars or strings
     */
    void addValue(const std::string& field, const std::vector<msgpack_variant>& values)
    {
        _valueMap[msgpack_variant(field)] = msgpack_variant(values);
    }

    /**
     * @brief Construct a msgpack::object representing { "labels": […], "value": { … } }
     */
    std::map<msgpack_variant, msgpack_variant> root() const
    {
        std::map<msgpack_variant, msgpack_variant> root;
        root[msgpack_variant("labels")] = msgpack_variant(_labels);
        root[msgpack_variant("value")] = msgpack_variant(_valueMap);
        return root;
    }

private:
    std::vector<msgpack_variant>               _labels;
    std::map<msgpack_variant, msgpack_variant> _valueMap;
};