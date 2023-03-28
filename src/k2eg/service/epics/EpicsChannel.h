#ifndef EpicsChannel_H
#define EpicsChannel_H

#include <cadef.h>
#include <k2eg/common/types.h>
#include <memory>
#include <pv/configuration.h>
#include <pv/createRequest.h>
#include <pv/json.h>
#include <pva/client.h>
#include <sstream>
#include <string>

namespace k2eg::service::epics_impl {

enum MonitorType { Fail, Cancel, Disconnec, Data };

typedef struct {
    const std::string channel_name;
    epics::pvData::PVStructure::const_shared_pointer data;
} ChannelData;
DEFINE_PTR_TYPES(ChannelData)

inline std::string to_json(const ChannelData& channel_data, const std::string& field = "") {
    std::stringstream ss;
    ss << "{\"" << channel_data.channel_name << "\":{";
    epics::pvData::BitSet bs_mask;
    epics::pvData::JSONPrintOptions opt;
    opt.ignoreUnprintable = true;
    opt.indent = 0;
    opt.multiLine = false;
    for (size_t idx = 1, N = channel_data.data->getStructure()->getNumberFields(); idx < N; idx++) {
        bs_mask.set(idx);
    }
    epics::pvData::printJSON(ss, *channel_data.data, bs_mask, opt);
    ss << "}";
    return ss.str();
}

typedef struct {
    MonitorType type;
    const std::string message;
    ChannelData channel_data;
} MonitorEvent;
DEFINE_PTR_TYPES(MonitorEvent)

typedef std::vector<MonitorEventShrdPtr> MonitorEventVec;
typedef std::shared_ptr<MonitorEventVec> MonitorEventVecShrdPtr;

class EpicsChannel {
    const std::string channel_name;
    const std::string address;
    epics::pvData::PVStructure::shared_pointer pvReq = epics::pvData::createRequest("field()");
    epics::pvAccess::Configuration::shared_pointer conf = epics::pvAccess::ConfigurationBuilder().push_env().build();
    std::unique_ptr<pvac::ClientProvider> provider;
    std::unique_ptr<pvac::ClientChannel> channel;
    pvac::MonitorSync mon;

public:
    explicit EpicsChannel(const std::string& provider_name, const std::string& channel_name, const std::string& address = std::string());
    ~EpicsChannel();
    static void init();
    static void deinit();
    void connect();
    ConstChannelDataUPtr getChannelData() const;
    epics::pvData::PVStructure::const_shared_pointer getData() const;
    template <typename T> void putData(const std::string& name, T new_value) const { channel->put().set(name, new_value).exec(); }
    void putData(const std::string& name, const epics::pvData::AnyScalar& value) const;
    void startMonitor();
    MonitorEventVecShrdPtr monitor();
    void stopMonitor();
};

DEFINE_PTR_TYPES(EpicsChannel)

} // namespace k2eg::service::epics_impl
#endif
