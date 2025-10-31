# Message Format and Serialization

# Message Type
Every message managed by K2EG has a destination topic where replies are sent. Usually a client uses a single topic to receive replies. K2EG supports multiple serializations, so each message can have a different structure depending on the serialization chosen by the client. The reply serialization is chosen by the client when the command is sent to K2EG.

# Command Structure
Each command includes the following minimum attributes (commands are always sent as JSON; `serialization` selects the reply format):
```json
{
    "command": "<command>",
    "serialization": "json|msgpack",
    "pv_name": "(pva|ca)://<pv name>",
    "reply_topic": "reply-destination-topic",
    "reply_id": "reply id"
}
```
The `serialization` field applies to commands that produce data (read behavior, e.g., get/monitor/snapshot) and selects the reply serialization only. Commands themselves are always JSON. For write commands (e.g., put), it applies only to the optional reply message.
K2EG supports JSON and MsgPack for replies. Below is the description of EPICS value payloads for each serialization. The MsgPack structure is shown using JSON-like notation for clarity.

# EPICS Value Serialization
EPICS values are serialized differently depending on the reply serialization type:

## JSON Serialization (json)
```json
{
    "<pv name>":
    {
        "value":"< scalar | scalar array>",
        "alarm":{
            "severity":0,
            "status":0,
            "message":""
            },
        "timeStamp":{
            "secondsPastEpoch":0,
            "nanoseconds":0,
            "userTag":0
            },
        "display":{
            "limitLow":0,
            "limitHigh":0,
            "description":"",
            "units":"",
            "precision":0,
            "form":{
                "index":0
                }
            },
        "control":{
            "limitLow":0,
            "limitHigh":0,
            "minStep":0
            },
        "valueAlarm":{
            "active":0,
            "lowAlarmLimit":0,
            "lowWarningLimit":0,
            "highWarningLimit":0,
            "highAlarmLimit":0,
            "lowAlarmSeverity":0,
            "lowWarningSeverity":0,
            "highWarningSeverity":0,
            "highAlarmSeverity":0,
            "hysteresis":0
        }
    }
}
```

## MsgPack serialization (msgpack)
MsgPack uses a map mirroring the JSON structure. At level 0 there is a map where the key is the PV name and the value is another map containing the sub-keys value, alarm, timeStamp, display, control, valueAlarm:
```
MAP( "<pv name>", MAP(
    "value": scalar | scalar array,
    "alarm" : MAP(
        "severity":0,
        "status":0,
        "message":""
    ),
     "timeStamp": MAP(
        "secondsPastEpoch":0,
        "nanoseconds":0,
        "userTag":0
    ),
    "display": MAP(
            "limitLow":0,
            "limitHigh":0,
            "description":"",
            "units":"",
            "precision":0,
            "form":MAP(
                "index":0
            )
    ),
    "control": MAP(
            "limitLow":0,
            "limitHigh":0,
            "minStep":0
    ),
    "valueAlarm": MAP(
            "active":0,
            "lowAlarmLimit":0,
            "lowWarningLimit":0,
            "highWarningLimit":0,
            "highAlarmLimit":0,
            "lowAlarmSeverity":0,
            "lowWarningSeverity":0,
            "highWarningSeverity":0,
            "highAlarmSeverity":0,
            "hysteresis":0
    )
))
```


# Get Command
The get command retrieves the current value of an EPICS PV and generates a single reply message.
## JSON Structure (request)
```json
{
    "command": "get",
    "serialization": "json|msgpack",
    "pv_name": "(pva|ca)://<pv name>",
    "reply_topic": "reply-destination-topic",
    "reply_id": "reply id"
}
```

# Monitor Command
The monitor command enables streaming updates for one or more EPICS PVs to a Kafka topic. The reply messages have the same schema as the get command.

## JSON Structure (request)
Single PV:
```json
{
  "command": "monitor",
  "serialization": "json|msgpack",
  "pv_name": "(pva|ca)://<pv name>",
  "reply_topic": "reply-destination-topic",
  "reply_id": "reply id",
  "monitor_destination_topic": "optional-topic-for-monitor-events"
}
```

Multiple PVs:
```json
{
  "command": "monitor",
  "serialization": "json|msgpack",
  "pv_name": ["(pva|ca)://<pv1>", "(pva|ca)://<pv2>", "..."],
  "reply_topic": "reply-destination-topic",
  "reply_id": "reply id",
  "monitor_destination_topic": "optional-topic-for-monitor-events"
}
```

Notes:
- If `monitor_destination_topic` is omitted, the system uses `reply_topic` for event delivery.
- Deactivation is handled by controller logic and configuration; there is no `activate` field in the request.
# Put Command
The put command is similar to EPICS caput/pvput. It applies one or more field updates to a PV.

Important: the `value` is a base64-encoded MsgPack payload. The decoded MsgPack object must be a MAP whose keys match the PV fields to update. For simple scalar or waveform PVs this is typically `{ "value": <scalar|array> }`. For structures, nested maps can be used to update subfields.

## JSON Structure (request)
```json
{
  "command": "put",
  "pv_name": "(pva|ca)://<pv name>[.<field>]",
  "value": "<base64 of msgpack MAP>",
  "reply_topic": "reply-destination-topic",
  "reply_id": "reply id"
}
```

## MsgPack content examples (conceptual)
- Scalar PV: base64(msgpack({"value": 42}))
- Waveform PV: base64(msgpack({"value": [1, 2, 3]}))
- Structured PV: base64(msgpack({"fieldA": 1, "inner": {"subfield": 2}}))

Validation rules:
- The decoded payload must be a MsgPack MAP; otherwise the command fails.
- Keys must correspond to existing (and mutable) EPICS fields.
- Multiple fields can be updated in a single request.

### Snapshot Command

The **snapshot** command performs a one-time or continuous data acquisition (DAQ) of multiple EPICS PVs. Its structure is as follows:

```json
{
  "command": "snapshot",
  "snapshot_id": "<custom user-defined ID>",
  "pv_name_list": ["[pva|ca]://<pv name>", ...],
  "reply_topic": "<Kafka reply topic>",
  "reply_id": "<correlation ID>",
  "serialization": "json|msgpack",
  "is_continuous": true | false,
  "repeat_delay_msec": 1000,
  "time_window_msec": 1000,
  "snapshot_name": "snapshot_name"
}
```

**Behavior:**
- A monitor is allocated for each PV in the `pv_name_list`.
- The EPICS monitor captures at least one event per PV within the defined `time_window_msec`.
- If any PV does not produce an event within the window, a fallback `get` operation retrieves its current value.
- Each PV's data is sent asynchronously to the client as it arrives—no need to wait for the full time window.
- A final **completion message** is sent after the time window elapses.

**Reply Format:**
- **One message per PV**:
  ```json
  {
    "error": 0,
    "reply_id": "<reply id>",
    "<pv name>": { "value": 0, ... }
  }
  ```
- **Completion message**:
  ```json
  {
    "error": 1,
    "reply_id": "<reply id>"
  }
  ```

**Continuous Mode (`is_continuous: true`)**:
- The snapshot is repeatedly triggered.
- After each snapshot completes, it waits for `repeat_delay_msec` before starting a new one.
- `snapshot_name` is used as a unique identifier and also defines the topic where data is published.

**Field Descriptions**:
- `repeat_delay_msec`: Delay between two snapshots (in milliseconds).
- `time_window_msec`: Duration of the window to gather PV updates after a snapshot is triggered.
- `snapshot_name`: Unique name used for identification and topic routing.

**Reply Format (Continuous Mode):**
- **Header message**:
  ```json
  {
    "type": 0,  ==> identify the header
    "iter_index": <number of current snapshot trigger starting from 0>
    "timestamp": "<snapshot timestamp>",
    "snapshot_name": "the name of the snapshot"
  }
  ```
- **Data message** (one for each pv data):
  ```json
  {
    "type": 1,  ==> identify the data event
    "iter_index": <number of current snapshot trigger starting from 0>,
    "timestamp": "<snapshot timestamp>",
    "pv_name": {pv data object}
  }
  ```
- **Completion message**:
  ```json
  {
    "type": 2,  ==> identify the completion
    "error": 0,
    "error_message":"in case there has been some issuer during snapshot",
    "iter_index": <number of current snapshot trigger starting from 0>,
    "timestamp": "<snapshot timestamp>",
    "snapshot_name": "the name of the snapshot"
  }
  ```
