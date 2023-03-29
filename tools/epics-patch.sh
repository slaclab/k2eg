#!/bin/bash
OS=$(uname)

if [[ ${OS} == "Darwin" ]]
then
    echo "OSX patch"
    sed -i '' "s/OPT_CFLAGS_YES += -g/OPT_CFLAGS_YES += -g -std=c11/" $1/epics/src/epics/configure/os/CONFIG.darwinCommon.darwinCommon
    sed -i '' "s/OPT_CXXFLAGS_YES += -g/OPT_CXXFLAGS_YES += -g -std=c++11/" $1/epics/src/epics/configure/os/CONFIG.darwinCommon.darwinCommon 
fi

