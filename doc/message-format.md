# Message format and serialization

# Message type
Every message managed by k2eg has a destination topic that is the topic where the message, generated for that command, are sent. Usually a client that send comamnd to k2eg use a single topic for receive that message. K2EG can manage different type of serialization, so each message has different structure depending to the type of the serailziaiton choosen by the client. The serializaation type for the answer is choosen by the client when the command is sent to the k2eg.

# Command Structure
Each command has a minimum set of attributes that are the ones below:
```json
{
    "command": "command string desceription",
    "serialization": "json|msgpack",
    "pv_name": "channel description",
    "dest_topic": "destination topic"
}
```
The 'serialization' field is keeped in consideration only for the command that generate a data flow form EPICS to kafka, or generally speaking for all those command that has a read behaviour. For the command kind that have a write behaviour this field is keepd in consideration only if the write operation require a message for an answer.
Actually k2eg support two different serialization JSON or Msgpack for message from EPICS to kafka and JSON for the command submition, bellowe there is the description of the message input and output structure related to each serialization type. <span style="color:#59afe1">The Msgpack serialization will be described using json notation for simplification.</span>

# Epics value serialization
Each epics single value is serialized in different way depending of serialization type:

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

## MSGPack serialization (msgpack)
The msgpack serializaion use a map to represent data like a json structure. At level-0 there is a map where the key is the pv name and as value there is another map that contains the sublevel keys value, alarm, timeStamp, display, control, valueAlarm:
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
## MSGPack Compact serialization (msgpack-compact)
The msgpack compact, instead of a map, use an array to serialize all the values, leaving out the key. At the first position there is the pv name the other values are positionally equal to the position on the json and msgpack structure.
```
VECTOR(<channle name>,< scalar | scalar array>, <severity>, <status>, <message>, <secondsPastEpoch>, <nanoseconds>, <userTag>, <limitLow>, <limitHigh>, <description>, <units>, <precision>, <index>, <contorl limitLow>, <constrol limitHigh>, <control minStep>, <active>, <lowAlarmLimit>, <lowWarningLimit>, <highWarningLimit>, <highAlarmLimit>, <lowAlarmSeverity>, <lowWarningSeverity>, <highWarningSeverity>, <highAlarmSeverity>, <hysteresis>)
```

# Get Command
The **get** command permit to retrive the current value of an epics pv, so it generate a single message with the following schema
## JSON Structure
```json
{
    "command": "get",
    "serialization": "json|msgpack",
    "protocol": "pva|ca",
    "pv_name": "channel::a",
    "dest_topic": "destination_topic"
}
```

# Monitor Command
The **monitor** command permits to enable or disable the update notification, for a specific EPICS pv, into a specific kafka topic. K2eg permit to enable the monitoring of the same channel and forward event message on different topics and in different serializaton format for the specific topic. The message received for the event monitor is the same as the **get** command:
## JSON Structure
### Activation
```json
{
    "command": "monitor",
    "serialization": "json|msgpack",
    "protocol": "pva|ca",
    "pv_name": "pv name",
    "dest_topic": "destination topic",
    "activate": true
}
```
### Deactivation

```json
{
    "command": "monitor",
    "pv_name": "pv name",
    "dest_topic": "destination topic",
    "activate": false
}
```
# Put Command
Put command is ismilar to caput or pvput epics command, it permit to applya a value to a pv. In case the it is a scalar array, each value need to be separated by a space, like in the example below.
## JSON Structure
```json
{
    "command": "put",
    "protocol": "pva|ca",
    "pv_name": "pv name",
    "value": "<value>|<value> <value> <value> <value>"
}
```