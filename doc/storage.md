# K2EG Storage

This document describes how K2EG persists EPICS data and snapshots using the storage service (default MongoDB backend). It reflects the latest archiver changes: explicit serialization type tracking, snapshot lifecycle (header/data/tail), and the unique snapshot search key format.

---

## Overview

- Data path: workers write EPICS monitor updates and snapshot data as documents to MongoDB.
- Payloads: the original payload is stored as binary (e.g., MsgPack) along with the serialization type; when batching, a parsed BSON `value` is also embedded for convenient querying.
- Snapshots: each snapshot is a named, time-stamped entity; data records may link to a snapshot via `snapshot_id`.
- Search key: snapshots are discoverable via `search_key = "<snapshot_name>:<header_timestamp_ms>:<iter_index>"`.

---

## Collections and Document Shapes

Collection names are configurable (defaults shown from `MongoDB` section):

- `MongoDB.collection` (default: `epics_data`)
- `MongoDB.snapshots-collection` (default: `snapshots`)

### epics_data (data records)

One document per PV update. Created via upsert (`store`) or batch insert (`storeBatch`).

Example (MsgPack payload, linked to a snapshot):

```json
{
  "pv_name": "BPMS:LTUH:250:X",
  "topic": "snapshot:BL1",
  "timestamp": {"$date": "2025-06-21T12:34:56Z"},
  "metadata": "epics-NTScalar; unit=mm",
  "snapshot_id": "669d8a9e5ee8e8399d71c9b2",

  "raw_value": {"$binary": {"base64": "...", "subType": "00"}},
  "ser_type": "msgpack",

  "value": {
    "timeStamp": {"secondsPastEpoch": 1750509296, "nanoseconds": 123000000},
    "value": 3.14159,
    "alarm": {"severity": 0, "status": 0}
  }
}
```

Field notes:

- `pv_name`: EPICS PV name.
- `topic`: Logical source/topic (e.g., snapshot queue name or monitor topic).
- `timestamp`: Record timestamp persisted in UTC (the service normalizes to UTC).
- `metadata`: Implementation-defined string metadata saved with the record.
- `snapshot_id` (optional): Link to the snapshot document when the record originates from a snapshot.
- `raw_value`: Original payload bytes (BSON Binary).
- `ser_type`: Serialization type of `raw_value` (`msgpack`, `json`, ...).
- `value` (optional): Parsed BSON projection of the payload. Included for batch inserts to facilitate queries.

### snapshots (snapshot metadata)

One document per snapshot created by the archiver.

Example:

```json
{
  "_id": "669d8a9e5ee8e8399d71c9b2",
  "snapshot_name": "BL1",
  "created_at": {"$date": "2025-06-21T12:34:50Z"},
  "search_key": "BL1:1750509290000:7"
}
```

Field notes:

- `_id`: Snapshot identifier (MongoDB ObjectId or string; code handles both).
- `snapshot_name`: Human-readable name of the snapshot.
- `created_at`: Creation time (UTC).
- `search_key`: Unique key used for lookup (`name:header_ts_ms:iter_index`).

---

## Snapshot Ingestion Flow

The archiver consumes a snapshot stream composed of three logical message types:

1. Header
   - Extracts `snapshot_name`, `iter_index`, and `header_timestamp` (ms).
   - Computes `search_key = "<snapshot_name>:<header_timestamp>:<iter_index>"`.
   - Creates the `snapshots` document if it does not exist and caches its `_id`.

2. Data
   - For each PV data message, resolves the current snapshot `_id`.
   - Persists an `epics_data` record with:
     - `pv_name`, `topic`, `timestamp` (payload ts normalized to UTC),
     - `snapshot_id` (link),
     - `raw_value` bytes and `ser_type` (e.g., `msgpack`),
     - optional parsed `value` when batching.

3. Tail
   - Finalizes the current iteration; the archiver may clear per-iteration caches/contexts.

The archiver now explicitly tracks:

- `ser` (serialization type),
- `iter_index` (iteration counter within the snapshot),
- `payload_ts` (timestamp carried by the data message),
- `header_timestamp` (timestamp carried by the header message),

and uses these to build consistent `search_key` and timestamps in stored records.

---

## Indexes

Indexes are created automatically when `MongoDB.create-indexes=true`:

- On `epics_data`:
  - `{ pv_name: 1, timestamp: 1 }`
  - `{ timestamp: 1 }`
  - `{ topic: 1 }`
  - Unique sparse compound index `{ pv_name: 1, timestamp: 1, topic: 1, snapshot_id: 1 }`

- On `snapshots`:
  - `{ snapshot_name: 1 }`
  - Unique `{ search_key: 1 }`
  - `{ created_at: 1 }`

---

## Query Examples

Find latest N points for a PV in a time window:

```js
db.epics_data.find({
  pv_name: "BPMS:LTUH:250:X",
  timestamp: { $gte: ISODate("2025-06-21T12:00:00Z"), $lte: ISODate("2025-06-21T13:00:00Z") }
}).sort({ timestamp: -1 }).limit(100)
```

Find a snapshot by search key, then all PV records for that snapshot:

```js
const snap = db.snapshots.findOne({ search_key: "BL1:1750509290000:7" });
db.epics_data.find({ snapshot_id: snap._id }).sort({ pv_name: 1, timestamp: 1 })
```

Extract parsed values for a PV from a specific snapshot (only for records written with `value` embedded):

```js
db.epics_data.find(
  { pv_name: "BPMS:LTUH:250:X", snapshot_id: ObjectId("669d8a9e5ee8e8399d71c9b2") },
  { _id: 0, pv_name: 1, timestamp: 1, value: 1 }
)
```

---

## Configuration (MongoDB backend)

Program options (section `MongoDB`):

- `connection-string` (default: `mongodb://localhost:27017`)
- `database` (default: `k2eg_archive`)
- `collection` (default: `epics_data`)
- `snapshots-collection` (default: `snapshots`)
- `pool-size` (default: `10`)
- `timeout-ms` (default: `5000`)
- `create-indexes` (default: `true`)
- `batch-size` (default: `1000`)

CLI flags are integrated via the storage service program options; see `StorageServiceFactory` for details.

---

## Notes

- Timestamps are normalized to UTC on write.
- The storage layer records the serialization type next to the raw payload to support faithful replay.
- The `value` field is best-effort and only present when conversion is enabled (MsgPack → BSON); for other formats, use `raw_value`.
