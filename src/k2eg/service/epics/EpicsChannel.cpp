#include <k2eg/service/epics/EpicsChannel.h>
#include <pv/caProvider.h>
#include <pv/clientFactory.h>


using namespace k2eg::service::epics_impl;

namespace pvd = epics::pvData;
namespace pva = epics::pvAccess;

PutTracker::PutTracker(pvac::ClientChannel& channel, const pvd::PVStructure::const_shared_pointer& pvReq, const std::string& value)
    : op(channel.put(this, pvReq))  // put() starts here
      ,
      value(value) {}
PutTracker::~PutTracker() { op.cancel(); }
void
PutTracker::putBuild(const epics::pvData::StructureConstPtr& build, pvac::ClientChannel::PutCallback::Args& args) {
  // At this point we have the user provided value string 'value'
  // and the server provided structure (with types).
  // note: an exception thrown here will result in putDone() w/ Fail
  // allocate a new structure instance.
  // we are one-shot so don't bother to re-use
  pvd::PVStructurePtr root(pvd::getPVDataCreate()->createPVStructure(build));
  // we only know about writes to scalar 'value' field
  pvd::PVScalarPtr valfld(root->getSubFieldT<pvd::PVScalar>("value"));
  // attempt convert string to actual field type
  valfld->putFrom(value);
  args.root = root;  // non-const -> const
  // mark only 'value' field to be sent.
  // other fields w/ default values won't be sent.
  args.tosend.set(valfld->getFieldOffset());
  std::cout << "Put value " << valfld << " sending=" << args.tosend << "\n";
}

void
PutTracker::putDone(const pvac::PutEvent& evt) {
  switch (evt.event) {
    case pvac::PutEvent::Fail: std::cerr << op.name() << " Error: " << evt.message << "\n"; break;
    case pvac::PutEvent::Cancel: std::cerr << op.name() << " Cancelled\n"; break;
    case pvac::PutEvent::Success: std::cout << op.name() << " Done\n";
  }
}

EpicsChannel::EpicsChannel(const std::string& provider_name, const std::string& channel_name, const std::string& address)
    : channel_name(channel_name), address(address) {
  provider = std::make_unique<pvac::ClientProvider>(provider_name, conf);
}

EpicsChannel::~EpicsChannel() {
  if (channel) { channel->reset(); }
  if (provider) { provider->disconnect(); }
}

void
EpicsChannel::init() {
  // "pva" provider automatically in registry
  // add "ca" provider to registry
  pva::ca::CAClientFactory::start();
}

void
EpicsChannel::deinit() {
  // "pva" provider automatically in registry
  // add "ca" provider to registry
  pva::ca::CAClientFactory::stop();
}

void
EpicsChannel::connect() {
  pvac::ClientChannel::Options opt;
  if (!address.empty()) { opt.address = address; }
  channel = std::make_unique<pvac::ClientChannel>(provider->connect(channel_name, opt));
}

ConstChannelDataUPtr
EpicsChannel::getChannelData() const {
  ConstChannelDataUPtr result;
  try {
    result = std::make_unique<ChannelData>(ChannelData{channel_name, channel->get()});
  } catch (pvac::Timeout to) {
    // ltimeout error
  }
  return result;
}

pvd::PVStructure::const_shared_pointer
EpicsChannel::getData() const {
  return channel->get();
}

void
EpicsChannel::putData(const std::string& name, const epics::pvData::AnyScalar& new_value) const {
  channel->put().set(name, new_value).exec();
}

void
EpicsChannel::startMonitor() {
  mon = channel->monitor();
}

MonitorEventVecShrdPtr
EpicsChannel::monitor() {
  auto result = std::make_shared<MonitorEventVec>();
  if (!mon.wait(0.100)) {
    // updates mon.event
    return result;
  }

  switch (mon.event.event) {
    // Subscription network/internal error
    case pvac::MonitorEvent::Fail:
      result->push_back(std::make_shared<MonitorEvent>(MonitorEvent{MonitorType::Fail, channel_name, mon.event.message, nullptr}));
      break;
    // explicit call of 'mon.cancel' or subscription dropped
    case pvac::MonitorEvent::Cancel:
      result->push_back(std::make_shared<MonitorEvent>(MonitorEvent{MonitorType::Cancel, channel_name, mon.event.message, nullptr}));
      break;
    // Underlying channel becomes disconnected
    case pvac::MonitorEvent::Disconnect:
      result->push_back(std::make_shared<MonitorEvent>(MonitorEvent{MonitorType::Disconnec, channel_name, mon.event.message, nullptr}));
      break;
    // Data queue becomes not-empty
    case pvac::MonitorEvent::Data:
      // We drain event FIFO completely
      while (mon.poll()) {
        auto tmp_data = std::make_shared<epics::pvData::PVStructure>(mon.root->getStructure());
        tmp_data->copy(*mon.root);
        result->push_back(std::make_shared<MonitorEvent>(MonitorEvent{MonitorType::Data, mon.event.message, {channel_name, tmp_data}}));
      }
      break;
  }
  return result;
}

void
EpicsChannel::stopMonitor() {
  if (mon) { mon.cancel(); }
}