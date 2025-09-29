#include <k2eg/service/pubsub/impl/kafka/RDKafkaBase.h>

using namespace k2eg::common;
using namespace k2eg::service::pubsub::impl::kafka;

RDKafkaBase::RDKafkaBase()
    : conf(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL)), t_conf(RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC)) {}

RDKafkaBase::~RDKafkaBase() {}

namespace {
static std::string anyToString(const std::any& v)
{
    if (v.type() == typeid(std::string))
        return std::any_cast<std::string>(v);
    if (v.type() == typeid(const char*))
        return std::string(std::any_cast<const char*>(v));
    if (v.type() == typeid(char*))
        return std::string(std::any_cast<char*>(v));
    if (v.type() == typeid(bool))
        return std::any_cast<bool>(v) ? "true" : "false";
    if (v.type() == typeid(int))
        return std::to_string(std::any_cast<int>(v));
    if (v.type() == typeid(long))
        return std::to_string(std::any_cast<long>(v));
    if (v.type() == typeid(long long))
        return std::to_string(std::any_cast<long long>(v));
    if (v.type() == typeid(unsigned int))
        return std::to_string(std::any_cast<unsigned int>(v));
    if (v.type() == typeid(unsigned long))
        return std::to_string(std::any_cast<unsigned long>(v));
    if (v.type() == typeid(unsigned long long))
        return std::to_string(std::any_cast<unsigned long long>(v));
    if (v.type() == typeid(size_t))
        return std::to_string(std::any_cast<size_t>(v));
    if (v.type() == typeid(float))
        return std::to_string(std::any_cast<float>(v));
    if (v.type() == typeid(double))
        return std::to_string(std::any_cast<double>(v));
    // Fallback: try to_string on pointer address for unknown types
    return "";
}
} // namespace

void RDKafkaBase::applyCustomConfiguration(MapStrKV custom_impl_parameter)
{
    for (auto& [key, value] : custom_impl_parameter)
    {
        RDK_CONF_SET(conf, key, anyToString(value))
    }
}

int RDKafkaBase::setOption(const std::string& key, const std::string& value)
{
    std::string errstr;

    conf->set(key, value, errstr);
    return 0;
}

void RDKafkaBase::applyDefaultsThenOverrides(
    const std::vector<std::pair<std::string, std::string>>& defaults,
    const MapStrKV&                                         overrides)
{
    for (const auto& kv : defaults)
    {
        RDK_CONF_SET(conf, kv.first, kv.second)
    }
    for (const auto& [k, v] : overrides)
    {
        RDK_CONF_SET(conf, k, anyToString(v))
    }
}
