# Kafka to Epics Gateway
## Author and Organization

**Author:** Claudio Bisegni  
**Organization:** SLAC National Accelerator Laboratory  

[![Build k2eg with CMake](https://github.com/slaclab/k2eg/actions/workflows/build.yml/badge.svg)](https://github.com/slaclab/k2eg/actions/workflows/build.yml)  
[![Build k2eg Docker Image](https://github.com/slaclab/k2eg/actions/workflows/build-docker-image.yml/badge.svg)](https://github.com/slaclab/k2eg/actions/workflows/build-docker-image.yml)  

## Overview

**Kafka to EPICS Gateway (k2eg)** is a C++ implementation of a bidirectional gateway between Kafka and the [EPICS](https://epics.anl.gov) Control System. It serves as a central processing unit for specialized DAQ requirements involving advanced algorithms.  

The gateway processes commands from a Kafka topic and enables the injection of EPICS data into other Kafka topics.

## Current Features

The implementation supports the following features:  

### Commands
- **Get Command**  
- **Monitor Command**  
- **Snapshot Command**  
- **Put Command** (Supports Scalar and ScalarArray types)  

### Functionalities
- **JSON Serialization**  
- **MSGPack Serialization** (Binary and Compact)  
- **Multithreaded EPICS Monitoring**  
- **Snapshot Functionality**  

### Pending Features
- Cluster management and advanced features are yet to be implemented.  

For details on serialization and message formats, refer to the [documentation](doc/message-format.md).

## Architecture

The application architecture consists of two primary layers:  

1. **Controller Layer**  
    - Node Controller  
    - Command Controller  
    - Cluster Controller *(to be designed)*  

2. **Service Layer**  
    - Publisher and Subscriber Services  
    - Data Storage  
    - Logging  
    - EPICS Integration  
    - Cluster Services *(to be designed)*  

![K2EG Software Layer Interaction](doc/image/scheme.png)

## Node Types

K2EG supports two operational modes that can be specified using the `--node-type` parameter:

### Gateway Mode (Default)
The standard gateway mode provides full bidirectional communication between Kafka and EPICS, including:
- Command processing (Get, Put, Monitor, Snapshot)
- EPICS data publishing to Kafka
- Real-time monitoring and control capabilities

### Storage Mode
The storage mode operates as a specialized archival and replay system for EPICS data. Key features include:

- **Distributed Data Archiving**: Consumes MsgPack-encoded EPICS data from Kafka topics
- **Intelligent Indexing**: Extracts and indexes key PV fields in MongoDB for fast queries
- **Scalable Storage**: Archives complete snapshot payloads in S3 or local filesystem
- **Dynamic Topic Discovery**: Uses Consul KV for automatic topic management
- **Parallel Processing**: Multi-threaded consumption with one reader per topic
- **Snapshot Replay**: Timestamp-based retrieval and replay capabilities
- **Fault Tolerance**: Horizontally scalable and fault-tolerant architecture

The storage node consumes data from Kafka topics, processes MsgPack payloads, and provides both metadata indexing (MongoDB) and blob storage (S3) for efficient data archival and retrieval.

**Usage:**
```bash
# Run in storage mode
./k2eg --node-type=storage

# Run in gateway mode (default)
./k2eg --node-type=gateway
```

For detailed information about storage node configuration, architecture, and usage, see the [Storage Documentation](doc/storage.md).

## Getting Started

k2eg acts as a gateway for interacting with EPICS IOCs using Kafka. It listens to a Kafka input topic for JSON-encoded commands and performs IO operations on EPICS IOCs.

## Configuration Options

k2eg uses Boost Program Options for startup configuration. Below is a summary of available parameters:  

```console
k2eg --help
```

### Command-Line Options

- `--help`: Display help information.  
- `--version`: Show application version.  
- `--conf-file`: Enable configuration file usage.  
- `--conf-file-name <path>`: Specify configuration file path.  
- `--log-level <level>`: Set log level (`trace`, `debug`, `info`, `error`, `fatal`).  
- `--cmd-input-topic <topic>`: Kafka topic for receiving commands.  
- `--pub-server-address <address>`: Publisher server address.  
- `--sub-server-address <address>`: Subscriber server address.  

For a full list of options, refer to the detailed documentation above.

### Configuration File

Enable configuration file usage with:  
```console
k2eg --conf-file --conf-file-name <path/to/configuration/file>
```

Example configuration file:  
```conf
log-file-max-size=1234
log-on-syslog=true
syslog-server=syslog-server
syslog-port=5678
sub-server-address=sub-address
sub-group-id=sub-group-id
```

### Environment Variables

Environment variables prefixed with `EPICS_k2eg_` are automatically recognized. Example:  
```bash
export EPICS_k2eg_conf-file
export EPICS_k2eg_conf-file-name=<path/to/configuration/file>
```

## Commands

### General Command Structure

Commands are sent as JSON objects to the Kafka topic. Replies are sent to a client-specific Kafka topic.  

**Base Reply Structure:**  
```json
{
  "error": 0,
  "reply_id": "rep-id-snapshot",
  "message": "optional"
}
```

- `error`: Execution status (`0` for success, negative for errors).  
- `reply_id`: Unique identifier for matching replies to requests.  
- `message`: Additional information or error details.

### Supported Commands

#### Get Command
Retrieve the value of a PV.  
```json
{
  "command": "get",
  "serialization": "json|msgpack",
  "pv_name": "(pva|ca)://<pv name>",
  "reply_topic": "reply-destination-topic",
  "reply_id": "reply id"
}
```

#### Put Command
Update the value of a PV. The `value` field must contain a base64-encoded MsgPack MAP with keys matching PV fields (for simple PVs typically `{ "value": <scalar|array> }`).  
```json
{
  "command": "put",
  "pv_name": "(pva|ca)://<pv name>.attribute",
  "value": "<base64 of msgpack MAP>",
  "reply_topic": "reply-destination-topic",
  "reply_id": "reply id"
}
```

#### Monitor Command
Monitor a PV and send updates to a Kafka topic.  
```json
{
  "command": "monitor",
  "serialization": "json|msgpack",
  "pv_name": "(pva|ca)://<pv name>",
  "reply_topic": "reply-destination-topic",
  "reply_id": "reply id",
  "monitor_destination_topic": "alternate-destination-topic"
}
```

#### Snapshot Command
Capture a one-time or continuous snapshot of PV values.  
```json
{
  "command": "snapshot",
  "snapshot_id": "<custom id>",
  "pv_name_list": ["(pva|ca)://<pv name>", ...],
  "reply_topic": "reply-destination-topic",
  "reply_id": "reply id",
  "serialization": "json|msgpack",
  "is_continuous": true|false,
  "repeat_delay_msec": 1000,
  "time_window_msec": 1000,
  "snapshot_name": "snapshot_name"
}
```

- `is_continuous`: Set to `true` for periodic snapshots.  
- `repeat_delay_msec`: Delay between snapshots.  
- `time_window_msec`: Time window for collecting PV data.  

## Additional Information

For more details, refer to the [documentation](doc/message-format.md).  
