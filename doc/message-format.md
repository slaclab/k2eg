# Message format and serialization

# Message type
Every message managed by k2eg has a destination topic that is the topic where the message, generated for that command, are sent. Usually a client that send comamnd to k2eg use a single topic for receive that message. K2EG can manage different type of serialization, so each message has different structure depending to the type of the serailziaiton choosen by the client. The serializaation type for the answer is choosen by the client when the command is sent to the k2eg.

# Command Structure
Each command has a minimum set of attributes that are the ones below:
```json
{
    "command": "command string desceription",
    "serialization": "json|msgpack",
    "channel_name": "channel description",
    "dest_topic": "destination topic"
}
```
The 'serialization' field is keeped in consideration only for the command that generate a data flow form EPICS to kafka, or generally speaking for all those command that has a read behaviour. For the command kind that have a write behaviour this field is keepd in consideration only if the write operation require a message for an answer.
Actually k2eg support two different serialization JSON or Msgpack for message from EPICS to kafka and JSON for the command submition, bellowe there is the description of the message input and output structure related to each serialization type. <span style="color:#59afe1">The Msgpack serialization will be described using json notation for simplification.</span>

# Get Command
## JSON Structure
```json
{
    "command": "get",
    "serialization": "json|msgpack",
    "protocol": "pva|ca",
    "channel_name": "channel::a",
    "dest_topic": "destination_topic"
}
```
## Description
The 'get' command permit to retrive the current value of an epics channel, so it generate a single message with the following schema:
```json
{
    "variable:sum":
    {
        "value":0,
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
# Monitor Command

## JSON Structure
### Activation
```json
{
    "command": "monitor",
    "serialization": "json|msgpack",
    "protocol": "pva|ca",
    "channel_name": "channel name",
    "dest_topic": "destination topic",
    "activate": true
}
```
### Deactivation

```json
{
    "command": "monitor",
    "channel_name": "channel name",
    "dest_topic": "destination topic",
    "activate": false
}
```
## Description
The **monitor** command permits to enable or disable the update notification, for a specific EPICS channel, into a specific kafka topic. K2eg permit to enbale the monitoring of the same channel and forward event message on different topics and in different serializaton format for the specific topic. The message received for the event monitor is the same as the **get** command:
```json
{
    "variable:sum":
    {
        "value":0,
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