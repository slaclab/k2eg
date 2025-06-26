# K2EG Storage

**K2EG Storage** is a distributed archival and replay system designed for the [K2EG (EPICS-to-Kafka Gateway)](https://github.com/slaclab/k2eg) ecosystem. It consumes MsgPack-encoded EPICS data from Kafka topics, extracts and indexes key fields in MongoDB, and archives complete snapshot payloads in S3 or an optional local filesystem (e.g., HDF5) for scalable, replayable, long-term storage.

---

## 📊 Key Features

* ✅ Dynamic topic discovery via **Consul KV**
* ✅ Parallel Kafka consumption with **1 reader per topic**
* ✅ Multi-threaded MsgPack decoding
* ✅ Hash-sharded MongoDB writers (ordered per PV)
* ✅ Snapshot replay window based on timestamps
* ✅ Efficient, compressed snapshot storage in **S3**
* ✅ MongoDB metadata indexing for replay queries
* ✅ Fault-tolerant and horizontally scalable

---

## 📅 Use Case Overview

1. **Monitor Streams:** EPICS PV updates are published to Kafka (topic per PV).
2. **Snapshot Captures:** Periodic group snapshots published by K2EG to Kafka.
3. **Archiving:** K2EG Storage decodes, indexes, and archives:

   * Key PV fields (‘value’, ‘timestamp’, etc.) ➔ MongoDB
   * Full snapshot payloads ➔ S3 or optional local filesystem
4. **Replay:** Snapshots can be retrieved via timestamp queries and replayed to downstream systems (ML pipelines, visualizers, EPICS simulators).

---

## 🔗 System Architecture

```
[Consul]                       [Kafka Topics]
  └ enabled_topics/           ├ <pv_name> (for monitor)
  └ archiver/topics/<topic>   └ snapshot-name (for snapshot)

[K2EG Storage Node]
  └ Topic Watcher → Claim via CAS (Consul)
  └ Kafka Readers (1/topic)
  └ MsgPack Decoder Pool
  └ Writer Shards (hash(PV))
     ├ MongoDB: pv_values
     └ S3: snapshot blobs

[MongoDB]
  ├ pv_values           → one PV+timestamp per document
  └ snapshots_index     → snapshot metadata (ID, ts, S3 path)

[S3]
  └ /snapshots/YYYY/MM/DD/snap-<ts>.msgpack.gz
```

---

## 📂 MongoDB Collections

### `pv_values`

```json
{
  "pv": "BPMS:LTUH:250:X",
  "value": 3.14,
  "type": "float",
  "timestamp": ISODate("2025-06-21T12:34:56Z"),
  "source": "snapshot" | "monitor",
  "snapshot_ids": ["snap-20250621T123456Z"],
  "msgpack_s3_key": "snapshots/2025/06/21/snap-20250621T123456Z.msgpack.gz"
}
```

### `snapshots_index`

```json
{
  "snapshot_id": "snap-20250621T123456Z",
  "timestamp": ISODate("2025-06-21T12:34:56Z"),
  "pv_count": 140,
  "s3_key": "snapshots/2025/06/21/snap-20250621T123456Z.msgpack.gz",
  "source": "k2eg-archiver-1"
}
```

---

## 📚 Snapshot Replay

To replay snapshots between two timestamps:

1. Query MongoDB:

```js
db.snapshots_index.find({
  timestamp: { $gte: ISODate("2025-06-21T10:00:00Z"), $lte: ISODate("2025-06-21T12:00:00Z") }
}).sort({ timestamp: 1 })
```

2. For each result:

   * Download MsgPack blob from S3 using `s3_key`
   * Decompress + decode
   * Stream PVs to EPICS simulator or analytic pipeline

---

## 📆 Consul Key Layout

```plaintext
k2eg/
├── enabled_topics/
│   ├── monitor:BPMS:LTUH:250:X = true
│   ├── snapshot:BL1 = true
├── archiver/topics/monitor:BPMS:LTUH:250:X = k2eg-archiver-1
├── archiver/topic_weights/snapshot:BL1 = 10
```

---

## 🌐 Environment Variables

| Variable           | Description                         |
| ------------------ | ----------------------------------- |
| `ARCHIVER_NODE_ID` | Unique ID for this instance         |
| `CONSUL_ADDR`      | Consul agent address                |
| `KAFKA_BOOTSTRAP`  | Kafka broker address(es)            |
| `MONGODB_URI`      | MongoDB connection string           |
| `S3_BUCKET`        | S3 bucket name for snapshot archive |
| `S3_PREFIX`        | Prefix path in S3 bucket            |
| `WORKER_COUNT`     | Number of decoder goroutines        |
| `WRITER_SHARDS`    | Number of Mongo writer partitions   |

---

## 🚀 Performance Tips

* Use Zstandard or GZIP compression for snapshot MsgPack files
* Enable MongoDB indexes on `(pv, timestamp)` and `snapshot_id`
* Use bulk `InsertMany` in writers for better throughput
* Tune Go GC and ulimits for large deployments

---

## 💼 License

MIT License

---

## 🚜 Related Projects

* [K2EG](https://github.com/slaclab/k2eg): EPICS-to-Kafka Gateway
* [EPICS Base](https://epics-controls.org): Control system framework
