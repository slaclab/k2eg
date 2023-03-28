#include <k2eg/service/pubsub/impl/kafka/RDKafkaBase.h>

using namespace k2eg::service::pubsub::impl::kafka;

RDKafkaBase::RDKafkaBase(): 
    conf(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL)),
    t_conf(RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC)) {
}

RDKafkaBase::~RDKafkaBase() {
}

int RDKafkaBase::setOption(const std::string& key, const std::string& value) {
    std::string errstr;

    conf->set(key, value, errstr);
    return 0;
}
