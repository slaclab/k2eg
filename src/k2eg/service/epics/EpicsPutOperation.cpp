#include <k2eg/service/epics/EpicsPutOperation.h>
#include <k2eg/service/epics/MsgpackEpicsConverter.h>
#include <pv/createRequest.h>
#include <pvData.h>

#include <stdexcept>

using namespace k2eg::service::epics_impl;

namespace pvd = epics::pvData;

PutOperation::PutOperation(std::shared_ptr<pvac::ClientChannel> channel, const pvd::PVStructure::const_shared_pointer& pv_req, std::unique_ptr<k2eg::common::MsgpackObject> put_object)
    : channel(channel), put_object(std::move(put_object)), pv_req(pv_req), done(false)
{
    channel->addConnectListener(this);
}

PutOperation::~PutOperation()
{
    if (op)
    {
        op.cancel();
    }
    channel->removeConnectListener(this);
}

/*
 This function updates the EPICS structure using the user-provided Msgpack object.
 It first checks that the Msgpack object is a map, extracts all keys, and verifies
 that each key corresponds to a valid field in the EPICS structure. For each valid
 field, it calculates the field bitmask to indicate which fields will be updated.
 The Msgpack object is then converted to a PVStructure matching the schema, and
 its values are copied into the root structure. Finally, the updated structure and
 field bitmask are set in the args for the put operation.
 */
void PutOperation::putBuild(const epics::pvData::StructureConstPtr& build, pvac::ClientChannel::PutCallback::Args& args)
{
    std::size_t         field_bit = 0;
    pvd::PVStructurePtr root(pvd::getPVDataCreate()->createPVStructure(build));

    // check if msgpack object is a map:
    if (put_object->get().type != msgpack::type::MAP)
    {
        throw std::runtime_error("Put object must be a map");
    }

    // get all map keys as std::strings
    std::vector<std::string> keys;
    for (uint32_t i = 0; i < put_object->get().via.map.size; ++i)
    {
        keys.push_back(put_object->get().via.map.ptr[i].key.as<std::string>());
    }

    // calculate the field_bit for all requested fields to update
    for (const auto& key : keys)
    {
        // check if the field exists in the structure
        auto fld = root->getSubFieldT<pvd::PVField>(key);
        if (!fld)
        {
            throw std::runtime_error("Field '" + key + "' has not been found");
        }
        // calculate the field_bit for the field
        field_bit |= fld->getFieldOffset();
    }

    // convert the Msgpack object to a PVStructure and copy its values into the root structure
    auto put_obj_structure = MsgpackEpicsConverter::msgpackToEpics(put_object->get(), build);

    root->copy(*put_obj_structure);

    args.root = root; // non-const -> const
    // mark only the fields specified by field_bit to be sent in the put operation
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