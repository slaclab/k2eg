#include <k2eg/service/epics/EpicsMonitorOperation.h>
#include <pv/createRequest.h>

#include <memory>
#include <mutex>

#include "k2eg/service/epics/EpicsData.h"
#include "k2eg/service/epics/EpicsGetOperation.h"

using namespace k2eg::service::epics_impl;
namespace pvd = epics::pvData;

MonitorOperationImpl::MonitorOperationImpl(std::shared_ptr<pvac::ClientChannel> channel, const std::string& pv_name, const std::string& field)
    : channel(channel), pv_name(pv_name), field(field), received_event(std::make_shared<EventReceived>()), has_data(false)
{
    channel->addConnectListener(this);
}

MonitorOperationImpl::~MonitorOperationImpl()
{
    if (mon)
    {
        mon.cancel();
    }
    channel->removeConnectListener(this);
}

void MonitorOperationImpl::poll(uint element_to_fetch) const
{
    if (!has_data)
        return;
    int fetched = 0;
    while (mon.poll() /*&& (++fetched <= element_to_fetch)*/)
    {
        ++fetched;
        auto tmp_data = std::make_shared<epics::pvData::PVStructure>(mon.root->getStructure());
        tmp_data->copy(*mon.root);
        received_event->event_data->push_back(std::make_shared<MonitorEvent>(MonitorEvent{EventType::Data, "", {pv_name, tmp_data}}));
    }
    if (fetched == 0 && force_update)
    {
        if (!get_op)
        {
            // force to update using a get operation
            get_op = std::make_unique<SingleGetOperation>(channel, pv_name, field);
        }
        else if (get_op->isDone())
        {
            // the data is ready
            auto data_from_get = get_op->getChannelData();
            received_event->event_data->push_back(
                std::make_shared<MonitorEvent>(MonitorEvent{EventType::Data, "", {pv_name, data_from_get->data}}));
            get_op.reset();
            force_update = false;
        }
    }
    else
    {
        // destroy the get operation
        get_op.reset();
    }

    has_data = !mon.complete();
}

void MonitorOperationImpl::connectEvent(const pvac::ConnectEvent& evt)
{
    if (evt.connected)
    {
        mon = channel->monitor(this, pvd::createRequest(field));
    }
    else
    {
        // pv not found manage has disconnected
        std::lock_guard<std::mutex> l(ce_mtx);
        received_event->event_fail->push_back(std::make_shared<MonitorEvent>(MonitorEvent{EventType::Fail, "PV is not reachable", {pv_name, nullptr}}));
    }
}

void MonitorOperationImpl::monitorEvent(const pvac::MonitorEvent& evt)
{
    // running on internal provider worker thread
    // minimize work here.
    unsigned                    fetched = 0;
    std::lock_guard<std::mutex> l(ce_mtx);
    switch (evt.event)
    {
    // Subscription network/internal error
    case pvac::MonitorEvent::Fail:
        received_event->event_fail->push_back(std::make_shared<MonitorEvent>(MonitorEvent{EventType::Fail, pv_name, evt.message, nullptr}));
        break;
    // explicit call of 'mon.cancel' or subscription dropped
    case pvac::MonitorEvent::Cancel:
        if (mon.valid())
        {
            // mon is valid so we can continue because this class is valid
            received_event->event_cancel->push_back(std::make_shared<MonitorEvent>(MonitorEvent{EventType::Cancel, pv_name, evt.message, nullptr}));
        }
        break;
    // Underlying channel becomes disconnected
    case pvac::MonitorEvent::Disconnect:
        received_event->event_disconnect->push_back(std::make_shared<MonitorEvent>(MonitorEvent{EventType::Disconnec, pv_name, evt.message, nullptr}));
        break;
    // Data queue becomes not-empty
    case pvac::MonitorEvent::Data:
        has_data = true;
        // We drain event FIFO completely
        // while (mon.poll()) {
        //   auto tmp_data = std::make_shared<epics::pvData::PVStructure>(mon.root->getStructure());
        //   tmp_data->copy(*mon.root);
        //   received_event->event_data->push_back(std::make_shared<MonitorEvent>(MonitorEvent{EventType::Data,
        //   evt.message, {pv_name, tmp_data}}));
        // }
        break;
    }
}

EventReceivedShrdPtr MonitorOperationImpl::getEventData() const
{
    std::lock_guard<std::mutex> l(ce_mtx);
    auto                        result = received_event;
    received_event = std::make_shared<EventReceived>();
    return result;
}

bool MonitorOperationImpl::hasData() const
{
    std::lock_guard<std::mutex> l(ce_mtx);
    return received_event->event_data->size() > 0;
}

bool MonitorOperationImpl::hasEvents() const
{
    // try to check if we need to fetch somenthing
    std::lock_guard<std::mutex> l(ce_mtx);
    return received_event->event_data->size() > 0 || received_event->event_cancel->size() > 0 ||
           received_event->event_disconnect->size() > 0 || received_event->event_fail->size() > 0 ||
           received_event->event_timeout->size() > 0;
}

const std::string& MonitorOperationImpl::getPVName() const
{
    return pv_name;
}

//----------------------------- CombinedMonitorOperation --------------------------------
CombinedMonitorOperation::CombinedMonitorOperation(std::shared_ptr<pvac::ClientChannel> channel, const std::string& pv_name, const std::string& principal_request, const std::string& additional_request)
    : monitor_principal_request(MakeMonitorOperationImplUPtr(channel, pv_name, principal_request)), monitor_additional_request(MakeMonitorOperationImplUPtr(channel, pv_name, additional_request)), structure_merger(std::make_unique<PVStructureMerger>()), evt_received(std::make_shared<EventReceived>())
{
}

void CombinedMonitorOperation::poll(uint element_to_fetch) const
{
    monitor_principal_request->poll(element_to_fetch);
    monitor_additional_request->poll(element_to_fetch);
}

EventReceivedShrdPtr CombinedMonitorOperation::getEventData() const
{
    std::lock_guard<std::mutex> l(evt_mtx);
    // wait to receive data from the two monitor
    EventReceivedShrdPtr joined_evt = std::make_shared<EventReceived>();
    if (!monitor_principal_request->hasEvents() && (!monitor_additional_request->hasData() && !last_additional_evt_received))
        return joined_evt;
    auto a_evt_received = monitor_principal_request->getEventData();
    // get last event from additional data
    if (monitor_additional_request->hasData())
    {
        // get received additional data and take the only last
        auto add_evt_data = monitor_additional_request->getEventData();
        last_additional_evt_received = add_evt_data->event_data->at(add_evt_data->event_data->size() - 1);
    }

    // copy all other event except data from the first request
    joined_evt->event_cancel = a_evt_received->event_cancel;
    joined_evt->event_disconnect = a_evt_received->event_disconnect;
    joined_evt->event_fail = a_evt_received->event_fail;
    // merge all data from principal request to the last event
    for (auto& a_data : *a_evt_received->event_data)
    {
        // join all the event data from the principal request, with the last from additional request
        // event from principal are more important than from additional one request
        auto merge_event_data = structure_merger->mergeStructureAndValue(
            {a_data->channel_data.data, last_additional_evt_received->channel_data.data});
        joined_evt->event_data->push_back(MakeMonitorEventShrdPtr(a_data->type, "", ChannelData(a_data->channel_data.pv_name, merge_event_data)));
    }
    return joined_evt;
}

bool CombinedMonitorOperation::hasData() const
{
    return monitor_principal_request->hasData() && (monitor_additional_request->hasData() || last_additional_evt_received);
}

bool CombinedMonitorOperation::hasEvents() const
{
    return monitor_principal_request->hasEvents() && (monitor_additional_request->hasData() || last_additional_evt_received);
}

const std::string& CombinedMonitorOperation::getPVName() const
{
    return monitor_principal_request->getPVName();
}

void CombinedMonitorOperation::forceUpdate() const
{
    monitor_principal_request->forceUpdate();
    monitor_additional_request->forceUpdate();
}