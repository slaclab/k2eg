# Snapshots over Kafka

This document explains how the repeating/timed snapshots are published over Kafka by the Continuous Snapshot manager implementation (see `ContinuousSnapshotManager`). It describes topic naming, message lifecycle, message types, serialization, and consumer expectations.

## High-level flow

- A repeating or triggered snapshot command is received by the controller. The snapshot has a `snapshot_name`, a set of PVs and a `serialization` setting (e.g. JSON or MessagePack).
- The manager normalizes the snapshot name into a queue/topic name using the same normalization used by the code (alphanumeric and `-` allowed, other characters replaced with `_` and lowercased). In code this is implemented by the helper `GET_QUEUE_FROM_SNAPSHOT_NAME(snapshot_name)`.
- The manager creates a Kafka queue/topic for the snapshot using the normalized queue name (publisher creates the queue with appropriate retention settings).
- Periodically (or on trigger) a Snapshot iteration is performed and the manager publishes three logical kinds of messages for an iteration: Header, Data (0..N), Tail (completion).
- Each publication uses the configured `serialization` from the snapshot command and the publisher adds a metadata header `k2eg-ser-type` with the serialization name.

## Topic / queue naming

- Topic name = normalized snapshot name. Use the same normalization method as `GET_QUEUE_FROM_SNAPSHOT_NAME()` so producers and consumers match.
- Example: snapshot name `My:Snapshot/01` becomes something like `my_snapshot_01`.

## Message lifecycle and ordering

Each snapshot iteration produces a logical sequence that consumers should expect and handle:

1. Header message (logical start of iteration)
   - Sent once per iteration.
   - Contains: snapshot name, iteration id, snapshot timestamp (millis since epoch), and a message type indicator.
   - Consumers should use the header to open or prepare any per-iteration state.

2. Zero or more Data messages
   - Each Data message corresponds to a PV update captured in the snapshot window.
   - Data messages include: PV channel data (pv name, value, timestamp, metadata), the iteration id and the snapshot timestamp.
   - Data messages can be produced concurrently from multiple tasks (the implementation runs data publishing on a thread pool), but all belong to the same logical iteration id.

3. Tail (Completion) message
   - Sent once at the end of the iteration.
   - Signals that the iteration is complete; includes iteration id and snapshot timestamp.
   - When consumers see Tail for an iteration, they can finalize aggregation for that iteration.

Important notes about ordering:
- The implementation ensures a Header is published before Data/Tail for the same iteration. Data tasks may publish concurrently, and the Tail is used to mark completion once all Data tasks are done.
- Iterations are synchronized with an iteration manager (`SnapshotIterationSynchronizer`) that assigns a sequential iteration id per snapshot name. Consumers should rely on the iteration id to disambiguate and order logical iterations.

## Message types and fields

The implementation uses three logical message types with an associated numerical type indicator (used in the internal serialization structures):

- Header (type 0)
  - Fields: type indicator (0), `snapshot_name`, `snap_ts` (milliseconds), `iteration_id`

- Data (type 1)
  - Fields: type indicator (1), `snap_ts`, `iteration_id`, `channel_data` (a representation of PV data)

- Completion / Tail (type 2)
  - Fields: type indicator (2), optional status fields, `snapshot_name`, `snap_ts`, `iteration_id`

The exact field names and wire format depend on the chosen serialization (JSON, MessagePack, etc.). The code creates typed structures (e.g. `RepeatingSnaptshotHeader`, `RepeatingSnaptshotData`, `RepeatingSnaptshotCompletion`) and serializes them with the requested `serialization` setting.

## Serialization and metadata

- Snapshot commands include a `serialization` field. The manager uses that to serialize header/data/tail structures.
- Each published message includes a publisher metadata header `k2eg-ser-type` with the serialization name string. Consumers should inspect this header to know how to deserialize the payload.
- Supported serialization names are the same as elsewhere in the codebase (e.g. `json`, `msgpack`, `unknown`).

## Publisher details (how messages are sent)

- The manager uses an `IPublisher` implementation:
  - It calls `publisher->createQueue(...)` when starting the snapshot to ensure the queue exists.
  - It uses `publisher->pushMessage(MakeReplyPushableMessageUPtr(queue_name, "repeating-snapshot-events", snapshot_name, serialized_message), {{"k2eg-ser-type", serialization_to_string(serialization)}})` to publish messages.
- The `MakeReplyPushableMessageUPtr(...)` arguments include:
  - the queue/topic (`queue_name`)
  - an event-type string (in code: `"repeating-snapshot-events"`)
  - the `key` or logical name (in code: the snapshot name)
  - the serialized message payload

## Consumer expectations

- Subscribe to the topic named by the normalized snapshot name.
- Read the `k2eg-ser-type` metadata to determine serialization format and deserialize accordingly.
- Use the iteration id to group messages belonging to the same logical snapshot iteration.
- Expect: Header -> many Data -> Tail. Use Tail or iteration id changes to know when an iteration completes.
- Be prepared to handle out-of-order arrival of Data messages within the iteration (they may be produced concurrently). Rely on iteration id and Tail to finalize processing.

## Error handling and monitoring

- The publisher callback `publishEvtCB` logs `OnError` events (implementation logs the error message at `ERROR` level).
- The manager increments Prometheus metrics for snapshot events processed: `SnapshotEventCounter` is incremented per Data batch submission.
- Consumers and operators should monitor these metrics plus publisher logs to detect publishing problems.

## Snapshot lifecycle and configuration

- Snapshots are started with a `RepeatingSnapshotCommand` (or triggered with a trigger/stop command). The manager validates the command and associates a normalized queue name with the snapshot.
- The manager registers PVs in `pv_snapshot_map_` to route EPICS events into the snapshot buffers.
- Each iteration has a `snap_ts` (milliseconds since epoch) computed at publish time; this value is included in Header, Data, and Tail.
- When a snapshot is stopped, the manager stops monitoring PVs, removes tracking entries and publishes final Tail(s) as needed.

## Implementation references (where to look)

- `ContinuousSnapshotManager::startSnapshot(...)` — snapshot validation and start path.
- `SnapshotSubmissionTask::operator()()` — where Header, Data and Tail messages for an iteration are serialized and published.
- `GET_QUEUE_FROM_SNAPSHOT_NAME(...)` — snapshot name normalization logic.
- `SnapshotIterationSynchronizer` — iteration id allocation and synchronization between Header/Data/Tail tasks.
- `publisher->pushMessage(...)` usage — how message and metadata are passed to the publisher.

## Notes for developers

- Keep the queue naming normalization in sync between producers and consumers.
- Consumers should not assume strict per-message ordering beyond the guarantee that Header is emitted before Data/Tail for the same iteration; Data messages themselves can be concurrent.
- When changing the wire format or serialization options, update both the publisher serialization and consumer deserialization, and preserve the `k2eg-ser-type` header semantics.

---

This document is based on the snapshot publication implementation in `ContinuousSnapshotManager` and `SnapshotSubmissionTask`.

If you want, I can also add a small consumer example that subscribes to a snapshot topic and reconstructs iterations (JSON deserialization example).
