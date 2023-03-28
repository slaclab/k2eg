#!/bin/bash

if [ "$(uname)" == "Darwin" ]
then
    sed -i '' "s/OPT_CFLAGS_YES += -g/OPT_CFLAGS_YES += -g -std=c11/" $1/epics/src/epics/configure/os/CONFIG.darwinCommon.darwinCommon
    sed -i '' "s/OPT_CXXFLAGS_YES += -g/OPT_CXXFLAGS_YES += -g -std=c++11/" $1/epics/src/epics/configure/os/CONFIG.darwinCommon.darwinCommon 
elif [ "$(expr substr $(uname -s) 1 5)" == "Linux" ]
then
  echo Linux don\'t need any patch
elif [ -n "$COMSPEC" -a -x "$COMSPEC" ]
then 
  echo $0: this script does not support Windows
fi

