#include <k2eg/service/epics/EpicsPutOperation.h>
#include <pv/createRequest.h>

using namespace k2eg::service::epics_impl;

namespace pvd = epics::pvData;

PutOperation::PutOperation(std::shared_ptr<pvac::ClientChannel> channel, const pvd::PVStructure::const_shared_pointer& pvReq, const std::string& value)
    : channel(channel), value(value), done(false) {
  op = channel->put(this, pvReq);
}

PutOperation::~PutOperation() { op.cancel(); }
void
PutOperation::putBuild(const epics::pvData::StructureConstPtr& build, pvac::ClientChannel::PutCallback::Args& args) {
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
}

void
PutOperation::putDone(const pvac::PutEvent& evt) {
  this->evt = evt;
  done      = true;
}

const std::string
PutOperation::getOpName() const {
  return op.name();
}

const pvac::PutEvent&
PutOperation::getState() const {
  return evt;
}

const bool
PutOperation::isDone() const {
  return done;
}