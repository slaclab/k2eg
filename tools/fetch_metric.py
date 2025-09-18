#!/usr/bin/env python3
"""Fetch and print a Prometheus metric exposed over HTTP."""

import argparse
import re
import sys
import urllib.error
import urllib.request
from typing import Dict, Iterable, Optional, Tuple


METRIC_LINE_RE = re.compile(
    r"^(?P<name>[a-zA-Z_:][a-zA-Z0-9_:]*)"
    r"(?:\{(?P<labels>[^}]*)\})?\s+"
    r"(?P<value>[-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?)"
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Download Prometheus metrics and print the value for a specific metric."
    )
    parser.add_argument(
        "--url",
        default="http://localhost:8080/metrics",
        help="Metrics endpoint to query (default: %(default)s)",
    )
    parser.add_argument(
        "--metric",
        help="Metric name to print (e.g. k2eg_epics_pv_processing_duration_us). If omitted, print all metrics.",
    )
    parser.add_argument(
        "--label",
        action="append",
        default=[],
        metavar="KEY=VALUE",
        help="Optional label filter; repeat for multiple labels",
    )
    return parser.parse_args()


def parse_label_filters(raw_labels: Iterable[str]) -> Dict[str, str]:
    filters: Dict[str, str] = {}
    for raw in raw_labels:
        if "=" not in raw:
            raise ValueError(f"Invalid label filter '{raw}'. Expected KEY=VALUE format.")
        key, value = raw.split("=", 1)
        filters[key.strip()] = value.strip()
    return filters


def parse_labels_block(block: str) -> Dict[str, str]:
    labels: Dict[str, str] = {}
    if not block:
        return labels
    for item in block.split(","):
        item = item.strip()
        if not item:
            continue
        if "=" not in item:
            continue
        key, value = item.split("=", 1)
        if len(value) >= 2 and value[0] == value[-1] == '"':
            value = value[1:-1]
        labels[key] = value
    return labels


def load_metrics(url: str) -> str:
    try:
        with urllib.request.urlopen(url) as response:
            return response.read().decode("utf-8", errors="replace")
    except urllib.error.URLError as exc:  # pragma: no cover - defensive guard
        raise SystemExit(f"Failed to fetch metrics from {url}: {exc}")


def find_metric(
    metrics_body: str, metric_name: Optional[str], label_filters: Dict[str, str]
) -> Iterable[Tuple[str, Dict[str, str], str]]:
    for line in metrics_body.splitlines():
        if not line or line.startswith("#"):
            continue
        match = METRIC_LINE_RE.match(line)
        if not match:
            continue
        name = match.group("name")
        if metric_name and name != metric_name:
            continue
        labels = parse_labels_block(match.group("labels"))
        if all(labels.get(k) == v for k, v in label_filters.items()):
            yield name, labels, match.group("value")


def main() -> None:
    args = parse_args()
    try:
        label_filters = parse_label_filters(args.label)
    except ValueError as exc:
        raise SystemExit(str(exc))

    metrics_text = load_metrics(args.url)
    matches = list(find_metric(metrics_text, args.metric, label_filters))

    if not matches:
        wanted = ", ".join(f"{k}={v}" for k, v in label_filters.items())
        label_msg = f" with labels [{wanted}]" if wanted else ""
        metric_msg = f" '{args.metric}'" if args.metric else ""
        raise SystemExit(f"Metric{metric_msg}{label_msg} not found at {args.url}")

    for name, labels, value in matches:
        label_str = ", ".join(f"{k}={v}" for k, v in sorted(labels.items()))
        if label_str:
            print(f"{name}{{{label_str}}} {value}")
        else:
            print(f"{name} {value}")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:  # pragma: no cover - graceful exit
        sys.exit(130)
