#include <k2eg/controller/node/worker/snapshot/SnapshotOpInfo.h>

using namespace k2eg::controller::node::worker::snapshot;

SnapshotOpInfo::SnapshotOpInfo(const std::string& queue_name, k2eg::controller::command::cmd::ConstRepeatingSnapshotCommandShrdPtr cmd)
    : WorkerAsyncOperation(std::chrono::milliseconds(cmd->time_window_msec)), queue_name(queue_name), cmd(cmd), is_triggered(cmd->triggered)
{
}

SnapshotOpInfo::~SnapshotOpInfo() = default;

bool SnapshotOpInfo::isTimeout()
{
    // For triggered snapshots, timeout occurs if a trigger is requested or the snapshot is stopped.
    if (is_triggered)
    {
        if (!is_running)
        {
            // If stopped, reset trigger request and expire immediately.
            request_to_trigger = false;
            return true;
        }
        if (request_to_trigger)
        {
            // If triggered, reset and expire.
            request_to_trigger = false;
            return true;
        }
        // Not triggered and not stopped: do not expire.
        return false;
    }
    return WorkerAsyncOperation::isTimeout();
}

const epics::pvData::PVStructure::const_shared_pointer SnapshotOpInfo::filterPVField(const epics::pvData::PVStructure::const_shared_pointer& src, 
                                                                                     const std::unordered_set<std::string>& fields_to_include)
{
    using namespace epics::pvData;
    if (!src)
        return PVStructure::const_shared_pointer();

    FieldCreatePtr  fieldCreate = getFieldCreate();
    FieldBuilderPtr builder = fieldCreate->createFieldBuilder();

    // Add only the requested fields
    for (const auto& field : fields_to_include)
    {
        auto pvField = src->getSubFieldT<const PVField>(field);
        if (pvField)
        {
            builder = builder->add(field, pvField->getField());
        }
    }

    StructureConstPtr filteredStruct = builder->createStructure();
    PVStructurePtr    filteredPV = getPVDataCreate()->createPVStructure(filteredStruct);

    // Copy values
    for (const auto& field : fields_to_include)
    {
        auto srcField = src->getSubFieldT<const PVField>(field);
        auto dstField = filteredPV->getSubField(field);
        if (srcField && dstField)
        {
            dstField->copy(*srcField);
        }
    }

    return filteredPV;
}