#!/bin/bash

# Usage: ./collect_metrics.sh METRIC_NAME OUTPUT_FILE
METRIC="$1"
OUTPUT_FILE="$2"

if [ -z "$METRIC" ] || [ -z "$OUTPUT_FILE" ]; then
    echo "Usage: $0 METRIC_NAME OUTPUT_FILE"
    exit 1
fi

while true; do
    echo "### $(date '+%Y-%m-%d %H:%M:%S') ###" >> "$OUTPUT_FILE"
    python tools/fetch_metric.py --metric "$METRIC" >> "$OUTPUT_FILE"
    echo "" >> "$OUTPUT_FILE"
    sleep 1
done