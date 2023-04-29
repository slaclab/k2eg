#!/bin/sh
source /opt/env.sh
tmux new-session -d "softIoc -d $1"
sleep infinity & wait