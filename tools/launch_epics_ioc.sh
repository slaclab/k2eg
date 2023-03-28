#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$SCRIPT_DIR/../build/local/lib:$SCRIPT_DIR/../build/local/lib/linux-x86_64

$SCRIPT_DIR/../build/local/bin/linux-x86_64/softIocPVA -d $1