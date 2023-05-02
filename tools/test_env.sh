#!/bin/bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/workspace/build/local/lib:/workspace/build/local/lib/linux-x86_64
export PATH=$PATH:/workspace/build/local/bin:/workspace/build/local/bin/linux-x86_64
export EPICS_CA_ADDR_LIST="134.79.151.21:5068"
export EPICS_CA_AUTO_ADDR_LIST="NO"
export EPICS_CA_REPEATER_PORT="5065"
export EPICS_CA_SERVER_PORT="5064"
export EPICS_PVA_ADDR_LIST="lcls-prod01:5068 lcls-prod01:5063 mcc-dmz mccas0.slac.stanford.edu 134.79.151.21:5068"
export EPICS_PVA_AUTO_ADDR_LIST="NO"
export EPICS_PVA_BROADCAST_PORT="5076"
export EPICS_PVA_SERVER_PORT="5075"