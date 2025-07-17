#include "k2eg/service/epics/PVStructureMerger.h"
#include <client.h>
#include <iostream>
#include <k2eg/service/epics/EpicsData.h>
#include <k2eg/service/epics/EpicsGetOperation.h>

#include <pv/createRequest.h>
#include <pvData.h>
#include <pvIntrospect.h>

#include <memory>

using namespace k2eg::service::epics_impl;

namespace pvd = epics::pvData;

CombinedGetOperation::CombinedGetOperation(GetOperationShrdPtr get_op_a, GetOperationShrdPtr get_op_b)
    : get_op_a(get_op_a), get_op_b(get_op_b), structure_merger(std::make_unique<PVStructureMerger>())
{
}

bool CombinedGetOperation::isDone() const
{
    return get_op_a->isDone() && get_op_b->isDone();
}

// return the bad one
const pvac::GetEvent& CombinedGetOperation::getState() const
{
    auto state_a = get_op_a->getState();
    auto state_b = get_op_b->getState();
    if (state_a.event == state_b.event)
    {
        return get_op_a->getState();
    }
    else if (state_a.event == pvac::PutEvent::Fail || state_a.event == pvac::PutEvent::Cancel)
    {
        return get_op_a->getState();
    }
    else
    {
        return get_op_b->getState();
    }
}

// combine all the data
ConstChannelDataUPtr CombinedGetOperation::getChannelData() const
{
    ConstChannelDataUPtr result;
    if (isDone() == false)
        return result;
    if (getState().event != pvac::PutEvent::Success)
        return result;

    // we have data so combine it
    auto merged_result = structure_merger->mergeStructureAndValue(
        {PVStructureToMerge{get_op_a->getChannelData()->data, std::dynamic_pointer_cast<SingleGetOperation>(get_op_a)->requested_fields},
         PVStructureToMerge{get_op_b->getChannelData()->data, std::dynamic_pointer_cast<SingleGetOperation>(get_op_b)->requested_fields}});
    return MakeChannelDataUPtr(get_op_a->getChannelData()->pv_name, merged_result);
}

bool CombinedGetOperation::hasData() const
{
    return get_op_a->hasData() && get_op_b->hasData();
}

//----------------- SingleGetOperation  ------------------
SingleGetOperation::SingleGetOperation(std::shared_ptr<pvac::ClientChannel> channel, const std::string& pv_name, const std::string& field)
    : channel(channel), pv_name(pv_name), field(field), is_done(false)
{
    size_t start = field.find("field(");
    if (start != std::string::npos)
    {
        start += 6; // length of "field("
        size_t end = field.find(')', start);
        if (end != std::string::npos)
        {
            std::string       fields_substr = field.substr(start, end - start);
            std::stringstream ss(fields_substr);
            std::string       item;
            while (std::getline(ss, item, ','))
            {
                // Remove whitespace
                item.erase(std::remove_if(item.begin(), item.end(), ::isspace), item.end());
                if (!item.empty())
                    requested_fields.push_back(item);
            }
        }
    }
    channel->addConnectListener(this);
}

SingleGetOperation::~SingleGetOperation()
{
    if (op)
    {
        op.cancel();
    };
    channel->removeConnectListener(this);
}

void SingleGetOperation::getDone(const pvac::GetEvent& event)
{
    switch (event.event)
    {
    case pvac::GetEvent::Fail: break;
    case pvac::GetEvent::Cancel: break;
    case pvac::GetEvent::Success:
        {
            break;
        }
    }
    evt = event;
    is_done = true;
}

void SingleGetOperation::connectEvent(const pvac::ConnectEvent& evt)
{
    if (evt.connected)
    {
        op = channel->get(this, pvd::createRequest(field));
    }
    else
    {
        // pv not found manage has disconnected
        this->evt.event = pvac::GetEvent::Fail;
        this->evt.message = "Connection Error";
    }
}

bool SingleGetOperation::isDone() const
{
    return is_done;
}

const pvac::GetEvent& SingleGetOperation::getState() const
{
    return evt;
}

ConstChannelDataUPtr SingleGetOperation::getChannelData() const
{
    return std::make_unique<ChannelData>(ChannelData{pv_name, evt.value});
}

bool SingleGetOperation::hasData() const
{
    return evt.value != nullptr;
}