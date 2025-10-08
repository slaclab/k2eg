# Message Format and Serialization

## Message Types
Every message produced by K2EG is sent to a destination topic. A client typically uses a single reply topic to receive responses. K2EG supports multiple serialization formats; the wire shape of messages depends on the chosen serialization. The serialization for responses is specified by the client in the command.

## Command Structure
Each command has a common set of attributes:
```json
{
    "command": "<command name>",
    "serialization": "json|msgpack",
    "pv_name": "<pv name>",
    "reply_topic": "<reply topic>",
    "reply_id": "<correlation id>"
}
```
Notes:
- `serialization` applies to commands that produce EPICS data (read-like operations). For write-like operations, it matters only if a payload is returned.
- K2EG supports JSON and MessagePack payloads. Commands are submitted in JSON. For brevity, MessagePack is illustrated using JSON-like notation below.

### Common Fields and Base Reply

- `reply_topic`: Kafka topic where the gateway publishes the response or stream of events for the request.
- `reply_id`: Correlation id chosen by the client; echoed back on replies to allow matching.
- `protocol`: EPICS protocol for PV access (`pva` or `ca`) when applicable.
- `pv_name`: EPICS process variable name (without protocol prefix unless otherwise noted).

Base reply envelope (used for acks and simple responses):
```json
{
  "error": 0,
  "reply_id": "<correlation id>",
  "message": "<optional info>"
}
```
Where `error` is `0` on success; non-zero indicates an error with details in `message`.

## EPICS Value Serialization
EPICS values are serialized differently depending on the format:

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

## MessagePack serialization (msgpack)
MessagePack uses a map analogous to the JSON structure. At level-0 the key is the PV name and the value is a map with sub-keys `value`, `alarm`, `timeStamp`, `display`, `control`, `valueAlarm`:
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
 

## Get Command
Retrieve the current value of an EPICS PV; produces a single reply message.
### JSON Structure
```json
{
    "command": "get",
    "serialization": "json|msgpack",
    "protocol": "pva|ca",
    "pv_name": "channel::a",
    "reply_topic": "reply-destination-topic",
    "reply_id": "<reply id>"
}
```
Reply example (JSON serialization):
```json
{
  "error": 0,
  "reply_id": "<reply id>",
  "BPMS:LTUH:250:X": {
    "value": 0.123,
    "timeStamp": { "secondsPastEpoch": 1750509, "nanoseconds": 123000000 },
    "alarm": { "severity": 0, "status": 0, "message": "NO_ALARM" }
  }
}
```

## Monitor Command
Enable or disable update notifications for a specific EPICS PV on a Kafka topic. The monitor event payload matches the Get response schema.
### JSON Structure — Activation
```json
{
    "command": "monitor",
    "serialization": "json|msgpack",
    "protocol": "pva|ca",
    "pv_name": "<pv name>",
    "reply_topic": "reply-destination-topic",
    "reply_id": "<reply id>",
    "activate": true
}
```
### JSON Structure — Deactivation

```json
{
    "command": "monitor",
    "pv_name": "<pv name>",
    "reply_topic": "reply-destination-topic",
    "reply_id": "<reply id>",
    "activate": false
}
```
Ack example:
```json
{ "error": 0, "reply_id": "<reply id>" }
```
Event example (published to `reply_topic` while active):
```json
{
  "BPMS:LTUH:250:X": {
    "value": 0.127,
    "timeStamp": { "secondsPastEpoch": 1750510, "nanoseconds": 321000000 },
    "alarm": { "severity": 0, "status": 0, "message": "NO_ALARM" }
  }
}
```
## Put Command
Apply a value to a PV (similar to caput/pvput). For scalar arrays, provide values separated by spaces.
### JSON Structure
```json
{
    "command": "put",
    "protocol": "pva|ca",
    "pv_name": "<pv name>",
    "value": "<value>" | "<v1> <v2> <v3>",
    "reply_topic": "reply-destination-topic",
    "reply_id": "<reply id>",
}
```
Ack example:
```json
{ "error": 0, "reply_id": "<reply id>", "message": "applied" }
```

## Snapshot Command

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

Reply Format (one-shot):
- One message per PV:
  ```json
  {
    "error": 0,
    "reply_id": "<reply id>",
    "<pv name>": { "value": 0, ... }
  }
  ```
- Completion message:
  ```json
  {
    "error": 0,
    "reply_id": "<reply id>"
  }
  ```

Continuous Mode (`is_continuous: true`):
- The snapshot is repeatedly triggered.
- After each snapshot completes, it waits for `repeat_delay_msec` before starting a new one.
- `snapshot_name` is used as a unique identifier and also defines the topic where data is published.

Field Descriptions:
- `repeat_delay_msec`: Delay between two snapshots (in milliseconds).
- `time_window_msec`: Duration of the window to gather PV updates after a snapshot is triggered.
- `snapshot_name`: Unique name used for identification and topic routing.

Reply Format (Continuous Mode):
- Header message:
  ```json
  {
    "message_type": 0,
    "iter_index": <iteration index starting from 0>,
    "timestamp": <header timestamp (ms since epoch)>,
    "snapshot_name": "<snapshot name>"
  }
  ```
- Data message (one per PV sample):
  ```json
  {
    "message_type": 1,
    "iter_index": <iteration index>,
    "timestamp": <pv sample timestamp (ms since epoch)>,
    "header_timestamp": <header timestamp (ms since epoch)>,
    "pv_data": { /* EPICS value object as above */ }
  }
  ```
  - Completion (Tail) message:
  ```json
  {
    "message_type": 2,
    "error": 0,
    "error_message": "<optional error description>",
    "iter_index": <iteration index>,
    "timestamp": <completion timestamp (ms since epoch)>,
    "header_timestamp": <header timestamp (ms since epoch)>,
    "snapshot_name": "<snapshot name>"
  }
  ```

Notes:
- The exact wire layout for MessagePack follows the same field semantics; JSON above is illustrative.
- A metadata header `k2eg-ser-type` accompanies each published message to indicate the serialization type.

### Examples

Header (type 0):
```json
{
  "message_type": 0,
  "snapshot_name": "my_snapshot",
  "timestamp": 1750509290000,
  "iter_index": 7
}
```

Data (type 1):
```json
{
  "message_type": 1,
  "timestamp": 1750509290123,
  "header_timestamp": 1750509290000,
  "iter_index": 7,
  "pv_data": {
    "BPMS:LTUH:250:X": {
      "value": 0.123,
      "timeStamp": { "secondsPastEpoch": 1750509, "nanoseconds": 123000000 },
      "alarm": { "severity": 0, "status": 0, "message": "NO_ALARM" }
    }
  }
}
```

Completion / Tail (type 2):
```json
{
  "message_type": 2,
  "error": 0,
  "error_message": "",
  "snapshot_name": "my_snapshot",
  "timestamp": 1750509290999,
  "header_timestamp": 1750509290000,
  "iter_index": 7
}
```
