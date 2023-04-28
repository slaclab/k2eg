#include <k2eg/service/epics/EpicsGetOperation.h>
#include <pv/createRequest.h>
#include <pvData.h>
#include "client.h"

using namespace k2eg::service::epics_impl;

GetOperation::GetOperation(std::shared_ptr<pvac::ClientChannel> channel, const std::string& pv_name)
    : channel(channel), pv_name(pv_name), is_done(false) {
  channel->addConnectListener(this);
}

GetOperation::~GetOperation() {
  channel->removeConnectListener(this);
  op.cancel();
}

void
GetOperation::getDone(const pvac::GetEvent& event) {
  switch (event.event) {
    case pvac::GetEvent::Fail: break;
    case pvac::GetEvent::Cancel: break;
    case pvac::GetEvent::Success: {
      break;
    }
  }
  evt     = event;
  is_done = true;
}

void
GetOperation::connectEvent(const pvac::ConnectEvent& evt) {
  if (evt.connected) {
    op = channel->get(this);
  } else {
    // std::cout << "Disconnect " << name << "\n";
  }
}

bool
GetOperation::isDone() const {
  return is_done;
}

const pvac::GetEvent& GetOperation::getState() const {
    return evt;
}

ConstChannelDataUPtr
GetOperation::getChannelData() const {
  return std::make_unique<ChannelData>(ChannelData{pv_name, evt.value});
}