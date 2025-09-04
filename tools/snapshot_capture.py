#!/usr/bin/env python3
"""
Snapshot capture tool for K2EG.

Features
- Reads PV list from a file.
- Sends a repeating snapshot start command to a Kafka command topic.
- Waits for a command reply and extracts the publishing topic.
- Consumes snapshot events for a duration, skipping partial iterations (ignores data until next header).
- Sends a stop command and writes a CSV with PV rows and iteration columns.

Requirements
- Python 3.8+
- pip install confluent-kafka msgpack

Usage
  python tools/snapshot_capture.py \
    --bootstrap-servers localhost:9092 \
    --command-topic cmd-input \
    --reply-topic app-one-reply \
    --snapshot-name app-one-snapshot \
    --pvs-file pvs.txt \
    --duration-seconds 30 \
    --output-csv out.csv \
    --serialization msgpack \
    --time-window-msec 1000 \
    --sub-push-delay-msec 50 \
    --pv-prefix ca://

Notes
- The consumer subscribes to the snapshot topic at the time the command reply is received.
  It ignores any data until the first header (message_type==0) it sees to avoid a partial iteration.
- The CSV columns correspond to sequential iterations observed (0..N-1).
- Only the PV value field is captured; the tool requests pv_field_filter_list=["value"].
"""

import argparse
import json
import sys
import time
import uuid
from dataclasses import dataclass
from typing import Dict, List, Optional, Set, Tuple

import msgpack  # type: ignore
from confluent_kafka import Consumer, Producer, KafkaException, KafkaError


# Utilities

def normalize_pv_name(pv: str, pv_prefix: str) -> Tuple[str, str]:
    """
    Return (pv_uri, base_name) where pv_uri includes prefix when missing,
    and base_name is the PV without the protocol prefix.
    """
    if pv.startswith("ca://") or pv.startswith("pva://"):
        base = pv.split("://", 1)[1]
        return pv, base
    return f"{pv_prefix}{pv}", pv


def get_snapshot_queue_name(snapshot_name: str) -> str:
    """Mirror GET_QUEUE_FROM_SNAPSHOT_NAME in C++ for fallback when reply lacks a topic."""
    import re

    norm = re.sub(r"[^A-Za-z0-9\-]", "_", snapshot_name)
    return norm.lower()


# Data structures

@dataclass
class SnapshotCollector:
    pv_order: List[str]

    def __post_init__(self):
        # Map PV -> list of values per iteration index (append as columns grow)
        self.values: Dict[str, List[Optional[float]]] = {pv: [] for pv in self.pv_order}
        self.current_iter: Optional[int] = None
        self.seen_header: bool = False
        self.iter_start_time: Optional[float] = None

    def start_new_iteration(self, iteration_id: int):
        self.seen_header = True
        self.current_iter = iteration_id
        # Grow columns by appending placeholders for each PV
        for pv in self.pv_order:
            self.values[pv].append(None)

    def add_value(self, pv: str, value: Optional[float]):
        if not self.seen_header or self.current_iter is None:
            return
        if pv not in self.values:
            return
        col_idx = len(self.values[pv]) - 1
        # store last value wins within the iteration column
        self.values[pv][col_idx] = value

    def close_iteration(self) -> int:
        """Return the closed iteration column index (0-based)."""
        # Replace None with empty for CSV
        for pv in self.pv_order:
            if self.values[pv] and self.values[pv][-1] is None:
                # leave as None; CSV writer turns to empty cell
                pass
        done_idx = self.column_count - 1
        self.current_iter = None
        self.seen_header = False
        return done_idx

    @property
    def column_count(self) -> int:
        # columns determined by the longest PV list; all are kept in sync
        return max((len(v) for v in self.values.values()), default=0)


def build_repeating_snapshot_command(
    reply_topic: str,
    reply_id: str,
    snapshot_name: str,
    pv_uris: List[str],
    serialization: str,
    time_window_msec: int,
    sub_push_delay_msec: int,
    triggered: bool,
    snapshot_type: str,
) -> dict:
    return {
        "command": "repeating_snapshot",
        "serialization": serialization,
        "reply_topic": reply_topic,
        "reply_id": reply_id,
        "snapshot_name": snapshot_name,
        "pv_name_list": pv_uris,
        "repeat_delay_msec": 0,
        "time_window_msec": time_window_msec,
        "sub_push_delay_msec": sub_push_delay_msec,
        "triggered": triggered,
        "type": snapshot_type,
        "pv_field_filter_list": ["value"],
    }


def build_repeating_snapshot_stop_command(
    reply_topic: str, reply_id: str, snapshot_name: str, serialization: str
) -> dict:
    return {
        "command": "repeating_snapshot_stop",
        "serialization": serialization,
        "reply_topic": reply_topic,
        "reply_id": reply_id,
        "snapshot_name": snapshot_name,
    }


def produce_json(producer: Producer, topic: str, key: str, obj: dict):
    data = json.dumps(obj).encode("utf-8")
    producer.produce(topic, value=data, key=key)
    producer.flush()


def wait_for_command_reply(
    consumer: Consumer, reply_id: str, serialization: str, timeout_s: float
) -> Optional[dict]:
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        msg = consumer.poll(0.5)
        if msg is None:
            continue
        if msg.error():
            if msg.error().code() == KafkaError._PARTITION_EOF:
                continue
            raise KafkaException(msg.error())
        payload = msg.value()
        if not payload:
            continue
        try:
            if serialization.lower() == "json":
                obj = json.loads(payload)
            else:
                obj = msgpack.unpackb(payload, raw=False)
        except Exception:
            continue
        if not isinstance(obj, dict):
            continue
        if obj.get("reply_id") == reply_id:
            return obj
    return None


def extract_snapshot_topic_from_reply(reply: dict, snapshot_name: str) -> str:
    pub_topic = reply.get("publishing_topic")
    if isinstance(pub_topic, str) and pub_topic:
        return pub_topic
    return get_snapshot_queue_name(snapshot_name)


def parse_event(payload: bytes, serialization: str) -> Optional[Tuple[int, int, Dict[str, dict]]]:
    """
    Return (message_type, iteration_index, pv_map) where pv_map maps pv_name->pv_obj.
    For data messages, pv_obj should contain at least {"value": <number>}
    """
    try:
        if serialization.lower() == "json":
            obj = json.loads(payload)
        else:
            obj = msgpack.unpackb(payload, raw=False)
    except Exception:
        return None
    if not isinstance(obj, dict):
        return None
    msg_type = obj.get("message_type")
    if not isinstance(msg_type, int):
        return None
    iter_index = obj.get("iter_index")
    if not isinstance(iter_index, int):
        # For header/tail iter_index must be present; if not, treat as unknown
        iter_index = -1
    # PV map: any key not in known control fields
    control_keys = {"message_type", "timestamp", "iter_index"}
    pv_map: Dict[str, dict] = {k: v for k, v in obj.items() if k not in control_keys and isinstance(v, dict)}
    return msg_type, iter_index, pv_map


def write_csv(path: str, pv_order: List[str], values: Dict[str, List[Optional[float]]]):
    import csv

    # Determine number of columns
    ncols = max((len(v) for v in values.values()), default=0)
    header = [""] + [str(i) for i in range(ncols)]
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(header)
        for pv in pv_order:
            row = [pv]
            for val in values[pv]:
                row.append("" if val is None else (f"{val}"))
            w.writerow(row)


def main():
    ap = argparse.ArgumentParser(description="K2EG repeating snapshot capture to CSV")
    ap.add_argument("--bootstrap-servers", required=True)
    ap.add_argument("--command-topic", required=True, help="Kafka topic to send snapshot commands")
    ap.add_argument("--reply-topic", required=True, help="Kafka topic to receive command replies")
    ap.add_argument("--snapshot-name", required=True)
    ap.add_argument("--pvs-file", required=True, help="Text file with one PV per line")
    ap.add_argument("--output-csv", required=True)
    ap.add_argument("--duration-seconds", type=int, default=30)
    ap.add_argument("--serialization", choices=["json", "msgpack"], default="msgpack")
    ap.add_argument("--time-window-msec", type=int, default=1000)
    ap.add_argument("--sub-push-delay-msec", type=int, default=50)
    ap.add_argument("--snapshot-type", choices=["normal", "timedbuffered"], default="timedbuffered")
    ap.add_argument("--pv-prefix", default="ca://", help="Protocol prefix to add if missing (ca:// or pva://)")
    ap.add_argument("--group-id", default="k2eg-snapshot-capture")
    args = ap.parse_args()

    # Read PVs
    with open(args.pvs_file, "r") as pf:
        pvs_raw = [line.strip() for line in pf if line.strip() and not line.strip().startswith("#")]
    pv_uris: List[str] = []
    pv_names: List[str] = []
    for pv in pvs_raw:
        uri, base = normalize_pv_name(pv, args.pv_prefix)
        pv_uris.append(uri)
        pv_names.append(base)

    # Kafka producer for commands
    producer = Producer({"bootstrap.servers": args.bootstrap_servers})

    # Consumer for command replies
    reply_c = Consumer(
        {
            "bootstrap.servers": args.bootstrap_servers,
            "group.id": f"{args.group_id}-replies-{uuid.uuid4()}",
            "auto.offset.reset": "latest",
            "enable.auto.commit": False,
        }
    )
    reply_c.subscribe([args.reply_topic])

    # Send start command
    start_reply_id = str(uuid.uuid4())
    start_cmd = build_repeating_snapshot_command(
        reply_topic=args.reply_topic,
        reply_id=start_reply_id,
        snapshot_name=args.snapshot_name,
        pv_uris=pv_uris,
        serialization=args.serialization,
        time_window_msec=args.time_window_msec,
        sub_push_delay_msec=args.sub_push_delay_msec,
        triggered=False,
        snapshot_type=("TimedBuffered" if args.snapshot_type == "timedbuffered" else "Normal"),
    )
    produce_json(producer, args.command_topic, key=args.snapshot_name, obj=start_cmd)
    print(f"Sent start command with reply_id={start_reply_id}")

    # Wait for reply
    reply = wait_for_command_reply(reply_c, start_reply_id, args.serialization, timeout_s=15.0)
    if not reply:
        print("ERROR: No start reply received", file=sys.stderr)
        sys.exit(2)
    if int(reply.get("error", -1)) != 0:
        print(f"ERROR: Start failed: {reply}", file=sys.stderr)
        sys.exit(3)
    snapshot_topic = extract_snapshot_topic_from_reply(reply, args.snapshot_name)
    print(f"Snapshot publishing topic: {snapshot_topic}")

    # Consumer for snapshot events
    snap_c = Consumer(
        {
            "bootstrap.servers": args.bootstrap_servers,
            "group.id": f"{args.group_id}-events-{uuid.uuid4()}",
            "auto.offset.reset": "latest",
            "enable.auto.commit": False,
        }
    )
    snap_c.subscribe([snapshot_topic])

    collector = SnapshotCollector(pv_order=pv_names)
    start_time = None

    # Consume loop with duration
    deadline = time.time() + float(args.duration_seconds)
    try:
        while time.time() < deadline:
            msg = snap_c.poll(0.5)
            if msg is None:
                continue
            if msg.error():
                if msg.error().code() == KafkaError._PARTITION_EOF:
                    continue
                raise KafkaException(msg.error())
            parsed = parse_event(msg.value(), args.serialization)
            if not parsed:
                continue
            msg_type, it_index, pv_map = parsed
            # Skip any data until we see a header to avoid partial iteration
            if msg_type == 0:  # header
                collector.start_new_iteration(it_index)
                if start_time is None:
                    start_time = time.time()
            elif msg_type == 1:  # data
                if not collector.seen_header:
                    # header missed: discard until next header
                    continue
                # Extract values (after pv_field_filter_list=["value"], each pv_map[pv] should contain only {"value": x})
                for pv, obj in pv_map.items():
                    val = obj.get("value")
                    if isinstance(val, (int, float)):
                        collector.add_value(pv, float(val))
            elif msg_type == 2:  # tail
                if collector.seen_header:
                    collector.close_iteration()
            # else unknown types ignored
    finally:
        snap_c.close()

    # Send stop command
    stop_reply_id = str(uuid.uuid4())
    stop_cmd = build_repeating_snapshot_stop_command(
        reply_topic=args.reply_topic,
        reply_id=stop_reply_id,
        snapshot_name=args.snapshot_name,
        serialization=args.serialization,
    )
    produce_json(producer, args.command_topic, key=args.snapshot_name, obj=stop_cmd)
    print(f"Sent stop command with reply_id={stop_reply_id}")

    stop_reply = wait_for_command_reply(reply_c, stop_reply_id, args.serialization, timeout_s=15.0)
    reply_c.close()
    if not stop_reply:
        print("WARNING: No stop reply received (continuing)", file=sys.stderr)

    # Write CSV
    write_csv(args.output_csv, pv_order=pv_names, values=collector.values)
    print(f"CSV written to {args.output_csv} with {collector.column_count} columns")


if __name__ == "__main__":
    main()

