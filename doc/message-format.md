# Message format and serialization

# Message type
Every message managed by k2eg has a destination topic that is the topic where the message, generated for that command, are sent. Usually a client that send comamnd to k2eg use a single topic for receive that message. K2EG can manage different type of serialization, so each message has different structure depending to the type of the serailziaiton choosen by the client. The serializaation type for the answer is choosen by the client when the command is sent to the k2eg.

# Command Structure
Each command has a minimum set of attributes that are the ones below:
```json
{
    "command": "command string desceription",
    "serialization": "0-json|1-msgpack"
}
```
The serialization field is keeped in consideration only for the command that generate data, or have a read behaviour. For the command kind that have a write behaviour this field is keepd in consideration only if the write operation require a message for an answer.

Bellowe there is the description, for each command of the message type regardi the availabe serialization type.

# Get Command

## Description
The 'get' command permit to retrive the current value of an epics channel, so it geneerate a single answer message of the type:
```
---------------------------------------------------------
+ answer id = 0 + channel_name (string) + channel_value +
---------------------------------------------------------
```
* ***answer_id***, has the value of '0' that identify the it is the message that cam from a 'get' command;
* ***channel_name***, is the key that express the channel name
* ***channel_value***, is the key that express the current value of the channel