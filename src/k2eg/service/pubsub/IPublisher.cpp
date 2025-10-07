#include <k2eg/service/pubsub/IPublisher.h>

using namespace k2eg::service::pubsub;

IPublisher::IPublisher(ConstPublisherConfigurationShrdPtr configuration)
    : configuration(std::move(configuration))
{}

IPublisher::IPublisher(ConstPublisherConfigurationShrdPtr               configuration,
                       const std::unordered_map<std::string, std::any>& overrides)
    : configuration(std::move(configuration)), runtime_overrides_(overrides)
{}

int IPublisher::setCallBackForReqType(const std::string req_type, EventCallback eventCallback)
{
    auto ret = eventCallbackForReqType.insert(MapEvtHndlrForReqTypePair(req_type, eventCallback));
    return ret.second == false ? -1 : 1;
}
