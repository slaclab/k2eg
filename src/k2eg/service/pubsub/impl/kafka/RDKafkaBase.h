#ifndef RDKafkaBase_H
#define RDKafkaBase_H

#pragma once

#include <k2eg/common/types.h>

#include <librdkafka/rdkafkacpp.h>

#include <string>
#include <memory>

// macro utility
#define RDK_LOG_AND_THROW(msg) \
    throw std::runtime_error("Error configuring kafka (" + errstr + ")");

#define RDK_CONF_SET(conf, prop, value)                               \
    {                                                                 \
        std::string errstr;                                           \
        if (conf->set(prop, value, errstr) != RdKafka::Conf::CONF_OK) \
        {                                                             \
            RDK_LOG_AND_THROW(errstr)                                 \
        }                                                             \
    }

namespace k2eg::service::pubsub::impl::kafka
{
    class RDKafkaBase
    {
    protected:
        std::unique_ptr<RdKafka::Conf> conf;
        std::unique_ptr<RdKafka::Conf> t_conf;

    public:
        RDKafkaBase();
        ~RDKafkaBase();
        void applyCustomConfiguration(k2eg::common::MapStrKV custom_impl_parameter);
        int setOption(const std::string &key, const std::string &value);
    };
}

#endif