#include "../NodeUtilities.h"
#include "k2eg/common/BaseSerialization.h"
#include "k2eg/controller/command/cmd/SnapshotCommand.h"
#include "k2eg/common/uuid.h"
#include "gtest/gtest.h"

#include <unistd.h>
#include <unordered_map>

int k2eg_controller_storage_snapshot_test_port = 20600;

using namespace k2eg::common;
using namespace k2eg::service;
;
using namespace k2eg::service::pubsub;
using namespace k2eg::service::storage;
using namespace k2eg::controller::node;
using namespace k2eg::controller::node::worker;
using namespace k2eg::controller::command::cmd;

#define REPLY_TOPIC "app_reply_topic"
#define SNAPSHOT_NAME "snapshot_name"

TEST(NodeControllerStorageSnapshotTest, StartRecording)
{
    int64_t                          error = -1;
    std::string                      topic = "";
    SubscriberInterfaceElementVector received_msg;
    auto                             k2eg = startK2EG(
        k2eg_controller_storage_snapshot_test_port,
        NodeType::FULL,
        true,
        true,
        // override storage worker group id to be randomly generate to not get the old not acquired data
        std::unordered_map<std::string, std::string>{
            {"EPICS_k2eg_storage-worker-consumer-group-id", UUID::generateUUIDLite()}
        }
    );
    ASSERT_NE(k2eg, nullptr) << "Failed to create K2EG instance";
    ASSERT_TRUE(k2eg->isRunning()) << "K2EG instance is not started";

    sleep(2);

    auto& node_controller = k2eg->getNodeControllerReference();

    auto publisher = k2eg->getPublisherInstance();
    ASSERT_NE(publisher, nullptr) << "Failed to get publisher instance";

    auto subscriber_reply = k2eg->getSubscriberInstance(REPLY_TOPIC);
    ASSERT_NE(subscriber_reply, nullptr) << "Failed to get subscriber instance for reply";

    // auto subscriber_snapshot = k2eg->getSubscriberInstance(SNAPSHOT_NAME);
    // ASSERT_NE(subscriber_snapshot, nullptr) << "Failed to get subscriber instance for snapshot";

    auto storage_service = k2eg->getStorageServiceInstance();
    ASSERT_NE(storage_service, nullptr) << "Failed to get storage service instance";

    // remove all data
    storage_service->clearAllData();

    // start a snapshot
    auto start_snapshot_cmd = MakeRepeatingSnapshotCommandShrdPtr(
        SerializationType::Msgpack,
        REPLY_TOPIC,
        "rep-id",
        SNAPSHOT_NAME,
        std::unordered_set<std::string>{"pva://variable:a", "pva://variable:b"},
        0,
        1000,
        0,
        false,
        SnapshotType::NORMAL,
        std::unordered_set<std::string>{});

    // send snapshot request
    k2eg->sendCommand(publisher, std::make_unique<CMDMessage<RepeatingSnapshotCommandShrdPtr>>(k2eg->getGatewayCMDTopic(), start_snapshot_cmd));

    // wait for ack
    auto reply_msg_start_snapshot = k2eg->waitForReplyID(subscriber_reply, "rep-id", SerializationType::Msgpack, 60000);
    ASSERT_NE(reply_msg_start_snapshot, nullptr) << "Failed to get reply message";
    // get json object
    auto result_obj_start_snapshot = k2eg->getMsgpackObject(*reply_msg_start_snapshot);
    // check that the snapshot has been started
    std::cout << "Start snapshot reply: " << result_obj_start_snapshot.handle.get() << std::endl;
    ASSERT_EQ(result_obj_start_snapshot.map.size(), 4) << "Msgpack object size is not 4";
    {
        auto it = result_obj_start_snapshot.map.find("error");
        ASSERT_NE(it, result_obj_start_snapshot.map.end()) << "Missing 'error' field";
        it->second.convert_if_not_nil(error);
    }
    ASSERT_EQ(error, 0) << "JSON object 'error' is not 0";
    {
        auto it = result_obj_start_snapshot.map.find("publishing_topic");
        ASSERT_NE(it, result_obj_start_snapshot.map.end()) << "Missing 'publishing_topic' field";
        it->second.convert_if_not_nil(topic);
    }
    ASSERT_EQ(topic, SNAPSHOT_NAME) << "JSON object 'publishing_topic' is not 'snapshot_name'";

    // give a ticken to the maintanace task
    node_controller.performManagementTask();

    // give some time for acquire data
    sleep(30);

    // stop the snapshot
    auto stop_snapshot_cmd = MakeRepeatingSnapshotStopCommandShrdPtr(
        SerializationType::Msgpack,
        REPLY_TOPIC,
        "rep-id-1",
        SNAPSHOT_NAME);
    // send stop snapshot request
    k2eg->sendCommand(publisher, std::make_unique<CMDMessage<RepeatingSnapshotStopCommandShrdPtr>>(k2eg->getGatewayCMDTopic(), stop_snapshot_cmd));

    // wait for ack
    auto reply_msg_stop_snapshot = k2eg->waitForReplyID(subscriber_reply, "rep-id-1", SerializationType::Msgpack, 60000);
    ASSERT_NE(reply_msg_stop_snapshot, nullptr) << "Failed to get reply message";

    // for each found snapshot, check that two value has been recorded
    auto found_ids = k2eg->waitForSnapshotIdsInRange(storage_service);
    ASSERT_FALSE(found_ids.empty()) << "No snapshots found";

    // calculate the total number of records across snapshots and per-PV occurrences
    std::unordered_map<std::string, std::size_t> per_pv_counts{{"variable:a", 0}, {"variable:b", 0}};
    std::size_t                                   total_records = 0;
    for (const auto& snapshot_id : found_ids)
    {
        for (const auto& pv_name : {std::string("variable:a"), std::string("variable:b")})
        {
            ArchiveQuery query;
            query.pv_name = pv_name;
            query.snapshot_id = snapshot_id;
            query.limit = 10;

            auto result = storage_service->query(query);
            per_pv_counts[pv_name] += result.records.size();
            total_records += result.records.size();

            for (const auto& rec : result.records)
            {
                ASSERT_EQ(rec.pv_name, pv_name) << "Record PV mismatch";
                ASSERT_TRUE(rec.data != nullptr) << "Missing stored payload";
                auto payload = rec.data->data();
                ASSERT_TRUE(payload != nullptr) << "Null payload view";
                ASSERT_GT(payload->size(), 0u) << "Empty payload";
            }
        }
    }

    // Expect one record per PV per snapshot
    ASSERT_EQ(per_pv_counts["variable:a"], found_ids.size()) << "PV variable:a should appear once per snapshot";
    ASSERT_EQ(per_pv_counts["variable:b"], found_ids.size()) << "PV variable:b should appear once per snapshot";
    ASSERT_EQ(total_records, found_ids.size() * 2u) << "Total records should be two per snapshot";

    ASSERT_NO_THROW(k2eg.reset();) << "Failed to reset K2EG instance";
}
