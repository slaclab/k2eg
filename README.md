# Kafka to Epics Gateway
Author: Claudio Bisegni

Company: SLAC National Accelerator Laboratory

[![Build k2eg with CMake](https://github.com/slaclab/k2eg/actions/workflows/build.yml/badge.svg)](https://github.com/slaclab/k2eg/actions/workflows/build.yml)
[![Build k2eg Docker Image](https://github.com/slaclab/k2eg/actions/workflows/build-docker-image.yml/badge.svg)](https://github.com/slaclab/k2eg/actions/workflows/build-docker-image.yml)
## Description
A C++ implementation of a bidirectional gateway between Kafka and the [EPICS](https://epics.anl.gov) Control System that aims to serve as a central processing unit for specialized DAQ requirements involving advanced algorithms.

It receives commands from a specific Kafka topic and allows the injection of EPICS data towards other topics.

## Implementation Status
The current implementation receives and interprets commands and uses only JSON serialization (described later). It lacks cluster management and other advanced features.

```[tasklist]
### Command Task List
- [x] Get Command
- [x] Monitor Command
- [x] Snapshot Command
- [X] Put Command
    - Scalar
    - ScalarArray

### Functional Task List
- [x] JSON Serialization
- [X] MSGPack Binary serialization
- [X] MSGPack compact serialization
- [X] Multithreading EPICS Monitor
- [x] Shapshot
- [ ] Cluster implementation
```
For the serialization and message format see documentation [here](doc/message-format.md)
## Application Architecture
<p>The application architecture logic is designed following the scheme below. The dotted line boxes represent elements not yet developed.</p>
<p>
There are two principal layers:

* Controller:
    * Node Controller
    * Command Controller
    * Cluster Controller (to design)
* Service:
    * Publisher and Subscriber:
    * Data storage
    * Log
    * EPICS
    * Cluster Services (to design)
</p>

![K2EG Software Layer Interaction](doc/image/scheme.png)

## Getting started
This project aims to realize an [EPICS](https://epics.anl.gov) gateway for interacting with EPICS IOCs using Kafka. It uses an input topic from a Kafka cluster to receive JSON encoded commands that allow IO operations on the IOCs.

## Parameter
k2eg relies upon boost program options to manage the startup option configuration. Below is the complete list of parameters:

```console
k2eg --help
Epics k2eg:
  --help                                Produce help information
  --version                             Print the application version
  --conf-file arg (=0)                  Specify if we need to load
                                        configuration from file
  --conf-file-name arg                  Specify the configuration file
  --log-level arg (=info)               Specify the log level[trace, debug,
                                        info, error, fatal]
  --log-on-console                      Specify when the logger prints in
                                        console
  --log-on-file                         Specify when the logger prints in file
  --log-file-name arg                   Specify the log file path
  --log-file-max-size arg (=1)          Specify the maximum log file size in
                                        mbyte
  --log-on-syslog                       Specify when the logger prints in syslog
                                        server
  --syslog-server arg                   Specify syslog hostname
  --syslog-port arg (=514)              Specify syslog server port
  --cmd-input-topic arg                 Specify the message bus queue where
                                        the k2eg receives the configuration
                                        command
  --cmd-max-fecth-element arg (=10)     The max number of commands fetched per
                                        consume operation
  --cmd-max-fecth-time-out arg (=250)   Specify the timeout for waiting for the
                                        command in microseconds
  --nc-monitor-expiration-timeout arg (=3600)
                                        Specify the amount of time with no
                                        consumer on a queue after which monitor
                                        can be stopped
  --nc-purge-queue-on-exp-timeout arg (=1)
                                        Specify when a queue is purged when
                                        the monitor that pushes data onto it
                                        is stopped
  --nc-monitor-consumer-filterout-regex arg
                                        Specify regular expression to be used to
                                        filter out consumer groups from those
                                        used to calculate the number of active
                                        consumers of the monitor queue
  --pub-server-address arg              Publisher server address
  --pub-impl-kv arg                     The key:value list for publisher
                                        implementation driver
  --sub-server-address arg              Subscriber server address
  --sub-group-id arg (=k2eg-default-group)
                                        Subscriber group id
  --sub-impl-kv arg                     The key:value list for subscriber
                                        implementation driver
  --storage-path arg (=/opt/app/k2eg.sqlite)
                                        The path where the storage files are
                                        saved
  --monitor-worker-cron-schedule arg    The cron string for configuring the
                                        monitor checking scheduler
  --scheduler-check-delay-seconds arg (=60)
                                        The number of seconds for which the
                                        scheduler thread is going to sleep
  --scheduler-thread-number arg (=1)    The number of the scheduler workers
  --metric-enable arg (=0)              Enable metric management
  --metric-server-http-port arg (=8080) The port used for publishing the http
                                        metric server
```

There are two other different ways to configure the application other than the common command line option:
* [configuration file](#configuration-file)
* [environment variable](#environment-variable)

### Configuration File
The use of the configuration file is activated using the command line option:
```console
k2eg --conf-file --conf-file-name <path/to/configuration/file>
```
The content of the file needs to follow the rules:
```conf
...
log-file-max-size=1234
log-on-syslog=true
syslog-server=syslog-server
syslog-port=5678
sub-server-address=sub-address
sub-group-id=sub-group-id
...
```

### Environment variable
The use of the environment variable is automatically managed, each variable with the prefix *EPICS_k2eg_* is evaluated, for example enabling the use of the config file can be done via ENV variable with:
``` console
export EPICS_k2eg_conf-file
export EPICS_k2eg_conf-file-name=<path/to/configuration/file>
```

## Commands

Generaly each command return a reply to the client on a specifc destination where the client is readind data. Each client has his own kafka topic where it receive reply form k2eg. The general structure of the command with the base field is:
``` json
{"error":"int32","reply_id":"rep-id-snapshot", "message":"optional", ...}
```
* `error`: Indicates the status of the command execution. A value of `0` means the command was successfully executed. `Negative` values indicate an error occurred, while `positive` values may represent specific types of messages as defined in the documentation for each command.
* `reply_id`: A unique identifier sent by the client along with the command request. It is included in the reply to allow the client to match the response to the corresponding request. This is particularly useful because commands are processed asynchronously, and replies may not be received in the same order as the requests were sent. This mechanism enables clients to send multiple asynchronous requests and correctly associate each reply with its respective command.
* `message`: Describes errors or other information sent by the broker as a reply to the client. This provides details about the execution status of the associated command. The presence of this field depends on the specific command's description.

### Get Command
This implements the base caget|pvaget function of EPICS command. If possible, it will use a client from a monitor thread; otherwise, a new client is allocated to perform a get operation. The ***reply_id*** field is not mandatory and it is forwarded into the reply message.
```json
{
    "command": "get",
    "serialization": "json|msgpack",
    "pv_name": "(pva|ca)://PV_NAME",
    "reply_topic": "reply-destination-topic",
    "reply_id": "reply id"
}
```

### Put command
Put command permits changing the value field of a PV. The value needs to be a string that will be automatically converted to the type of the PV. The put command can send a reply with the result of the put operation. In this case, the ***dest_topic*** field is mandatory and the ***reply_id*** field is not mandatory and it is forwarded into the reply message.
```json
{
    "command": "monitor",
    "pv_name": "(pva|ca)://PV_NAME.attribute",
    "value": "pv new value",
    "reply_topic": "reply-destination-topic",
    "reply_id":"reply id"
}
```

### Monitor Command
This implements the base camonitor|pvamonitor function of EPICS client, creating a monitor thread in the gateway that sends the received values over the destination topic. The field **monitor_destination_topic** should be used to specify the monitor event topic. If not specified, the event will be forwarded to the reply topic.

Monitor Activation
```json
{
    "command": "monitor",
    "serialization": "json|msgpack",
    "pv_name": "(pva|ca)://PV_NAME",
    "reply_topic": "reply-destination-topic",
    "reply_id":"reply id",
    "monitor_destination_topic":"alternate-destination-topic"
}
```

Multi Monitor Activation
```json
{
    "command": "multi-monitor",
    "serialization": "json|msgpack",
    "pv_name": ["(pva|ca)://PV_NAME", ....],
    "reply_topic": "reply-destination-topic",
    "reply_id":"reply id",
    "monitor_destination_topic":"alternate-destination-topic"
}
```
Note: The Monitor deactivation is managed by k2eg itself or specific timeout when no other process is consuming data.

### Snapshot Command
This implements a one time DAQ snapshot funcationality, the comman has this tempalte below:
```JSON
{
    "command": "snapshot",
    "serialization": "json|msgpack",
    "pv_name_list": ["(pva|ca)://PV_NAME", ....],
    "reply_topic": "reply-destination-topic",
    "reply_id":"reply id",
}
```
This command allocates a monitor watcher for each PV. Currently, it uses a one-second time window to acquire the values of all specified PVs. The EPICS monitor ensures at least one event is received for each PV. If, at the end of the timeout, some PVs have not been received, a fallback `get` operation is performed to fetch the missing PV values, as last chance.
The data received for each PVs is asyncrhonously sent, by the broker,  to the client without waiting for the time window timeout. The client receive a completion message that inform it the the snapshot has been completed.
The JSON version of the reply is:
One messgae for each PV in the ```pv_name_list``` list:
```json
{"error":0,"reply_id":"<reply id>","<pv name>":{"value":0, ...}}
```
on completion message
```json
{"error":1,"reply_id":"rep-id-snapshot"}
```