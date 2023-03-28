#include <k2eg/service/pubsub/IPublisher.h>

using namespace k2eg::service::pubsub;

IPublisher::IPublisher(ConstPublisherConfigurationUPtr configuration)
    : configuration(std::move(configuration)) {
    if (this->configuration->server_address.empty()) {
        throw std::runtime_error("The pub/sub server address is mandatory");
    }
}

int IPublisher::setCallBackForReqType(const std::string req_type, EventCallback eventCallback) {
    auto ret = eventCallbackForReqType.insert(MapEvtHndlrForReqTypePair(req_type, eventCallback));
    return ret.second == false ? -1 : 1;
}
