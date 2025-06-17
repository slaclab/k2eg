#include <iostream>
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

void PutOperation::collectFieldOffsets(epics::pvData::PVStructurePtr root, const msgpack::object& obj, pvac::ClientChannel::PutCallback::Args& args)
{
    if (obj.type != msgpack::type::MAP)
        return;

    for (uint32_t i = 0; i < obj.via.map.size; ++i)
    {
        std::string            key = obj.via.map.ptr[i].key.as<std::string>();
        const msgpack::object& value = obj.via.map.ptr[i].val;

        auto field = root->getSubField(key);

        if (!field)
            continue;

        if (value.type == msgpack::type::MAP)
        {
            // Recurse into sub-structures, passing only the sub-structure and value
            auto subStruct = std::dynamic_pointer_cast<epics::pvData::PVStructure>(field);
            if (subStruct)
            {
                // set bit for this struct itself
                // args.tosend.set(field->getFieldOffset());
                collectFieldOffsets(subStruct, value, args);
            }
        }
        else
        {
            if(field->isImmutable())
            {
                throw std::runtime_error("Filed '" + key + "' is immutable and cannot be updated");
            }
            // Scalar or leaf: set the bit
            args.tosend.set(field->getFieldOffset());
        }
    }
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
    pvd::PVStructurePtr root(pvd::getPVDataCreate()->createPVStructure(build));

    // check if msgpack object is a map:
    if (put_object->get().type != msgpack::type::MAP)
    {
        throw std::runtime_error("Put object must be a map");
    }

    // calculate the filed bit to set all field to update
    collectFieldOffsets(root, put_object->get(),  args);
    // convert the Msgpack object to a PVStructure and copy its values into the root structure
    auto put_obj_structure = MsgpackEpicsConverter::msgpackToEpics(put_object->get(), build);
    root->copy(*put_obj_structure);
    args.root = root; // non-const -> const
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