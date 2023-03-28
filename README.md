# Kafka to Epics Gateway
A c++ implementation of a two way gateway from kafka and [EPICS](https://epics.anl.gov) Control System.


## Getting started
This project aim to realize an [EPICS](https://epics.anl.gov) gateway for interact with epics IOCs using kafka. It uses a input topic from a kafka cluster for receive json encoded commands that permit to execute IO operation on the IOCs.

## Parameter
k2eg rely upon boost rpogram options to manage the startup option configuration. Below the complete list of the parameter:

```console
k2eg --help
Epics k2eg:
  --help                                Produce help information
  --conf-file arg (=0)                  Specify if we need to load 
                                        configuration from file
  --conf-file-name arg                  Specify the configuration file
  --log-level arg (=info)               Specify the log level[error, info, 
                                        debug, fatal]
  --log-on-console                      Specify when the logger print in 
                                        console
  --log-on-file                         Specify when the logger print in file
  --log-file-name arg                   Specify the log file path
  --log-file-max-size arg (=1)          Specify the maximum log file size in 
                                        mbyte
  --log-on-syslog                       Specify when the logger print in syslog
                                        server
  --syslog-server arg                   Specify syslog hotsname
  --syslog-port arg (=514)              Specify syslog server port
  --cmd-input-topic arg                 Specify the messages bus queue where 
                                        the k2eg receive the configuration 
                                        command
  --cmd-max-fecth-element arg (=10)     The max number of command fetched per 
                                        consume operation
  --cmd-max-fecth-time-out arg (=100)   Specify the timeout for waith the 
                                        command in microseconds
  --pub-server-address arg              Publisher server address
  --pub-impl-kv arg                     The key:value list for publisher 
                                        implementation driver
  --sub-server-address arg              Subscriber server address
  --sub-group-id arg (=k2eg-default-group)
                                        Subscriber group id
  --sub-impl-kv arg                     The key:value list for subscriber 
                                        implementation driver
  --storage-path arg (=/workspace/k2eg.sqlite)
                                        The path where the storage files are 
                                        saved
```

There are two ohter different way to configure the application other to the common command line option and are:
* [configuration file](#configuration-file)
* [environment variable](#environment-variable)

### Configuration File
The uses of the configuration file is activated using the command line option:
```console
k2eg --conf-file --conf-file-name <path/to/configuration/file>
```
the content of the file need to folow the rules:
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
The uses of the environment variable si automatically managed, each variable with the prefix *EPICS_k2eg_* is valuated, for example the enable to use  the config file can be done via ENV variable with:
``` console
export EPICS_k2eg_conf-file
export EPICS_k2eg_conf-file-name=<path/to/configuration/file>
```

## Commands

### Monitor Command
This implements the base camonitor|pvamonitor function of epics client, create a monitor thread into the gateway that send over the destination topic the received values.

Monitor Activation
```json
{
    "command": "monitor",
    "activate": true,
    "channel_name": "channel name",
    "protocol": "pva|ca",
    "dest_topic": "destination topic"
}
```

Monitor deactivation
```json
{
    "command": "monitor",
    "activate": false,
    "channel_name": "channel name",
    "dest_topic": "destination topic"
}
```

### Get Command
This implemets the base caget|pvaget fucntion of epics command, if possible will use a client from a monitor thread, otherwhise allcoate new client and perform a get operation
```json
{
    "command": "get",
    "protocol": "pva|ca",
    "channel_name": "channel::a",
    "dest_topic": "destination_topic"
}
```