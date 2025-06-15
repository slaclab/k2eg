#include <k2eg/service/epics/EpicsPutOperation.h>
#include <k2eg/service/epics/MsgpackEpicsConverter.h>
#include <pv/createRequest.h>
#include <pvData.h>

#include <stdexcept>

using namespace k2eg::service::epics_impl;

namespace pvd = epics::pvData;

PutOperation::PutOperation(std::shared_ptr<pvac::ClientChannel> channel, const pvd::PVStructure::const_shared_pointer& pv_req, const std::string& field, std::unique_ptr<msgpack::object> value)
    : channel(channel), field(field), value(std::move(value)), pv_req(pv_req), done(false)
{
    
}

PutOperation::~PutOperation()
{
    if (op)
    {
        op.cancel();
    }
    channel->removeConnectListener(this);
}

void PutOperation::putBuild(const epics::pvData::StructureConstPtr& build, pvac::ClientChannel::PutCallback::Args& args)
{
    // At this point we have the user provided value string 'value'
    // and the server provided structure (with types).
    // note: an exception thrown here will result in putDone() w/ Fail
    // allocate a new structure instance.
    // we are one-shot so don't bother to re-use
    std::size_t         field_bit = 0;
    pvd::PVStructurePtr root(pvd::getPVDataCreate()->createPVStructure(build));
    // we only know about writes to scalar 'value' field
    auto fld = root->getSubFieldT<pvd::PVField>(field);
    if (!fld)
    {
        throw std::runtime_error("Field has not been found");
    }
    bool immutable = fld->isImmutable();
    if (immutable)
    {
        throw std::runtime_error("Field is immutable");
    }

    auto put_obj = MsgpackEpicsConverter::msgpackToEpics(*value, build);
    fld->copy(*put_obj);
    // attempt convert string to actual field type
    // valfld->putFrom(value);
    args.root = root; // non-const -> const
    // mark only 'value' field to be sent.
    // other fields w/ default values won't be sent.
    args.tosend.set(field_bit);
}

void PutOperation::putDone(const pvac::PutEvent& evt)
{
    this->evt = evt;
    done = true;
}

const std::string PutOperation::getOpName() const
{
    return op.name();
}

const pvac::PutEvent& PutOperation::getState() const
{
    return evt;
}

const bool PutOperation::isDone() const
{
    return done;
}

void PutOperation::connectEvent(const pvac::ConnectEvent& evt)
{
    if (evt.connected)
    {
        op = channel->put(this, pv_req);
    }
    else
    {
        // pv not found manage has disconnected
        this->evt.event = pvac::GetEvent::Fail;
        this->evt.message = "Connection Error";
    }
}