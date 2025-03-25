#!/bin/bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/workspace/build/local/lib:/workspace/build/local/lib/linux-x86_64:/workspace/build/local/lib/linux-aarch64
export PATH=$PATH:/workspace/build/local/bin:/workspace/build/local/bin/linux-x86_64:/workspace/build/local/bin/linux-aarch64
# export EPICS_CA_ADDR_LIST="134.79.151.21:5068 172.21.40.10:5064 172.21.40.11:5064 172.21.40.12:5064 172.21.40.13:5064 172.21.40.14:5064 172.21.40.15:5064 134.79.216.44"
# export EPICS_CA_AUTO_ADDR_LIST=NO
# export EPICS_CA_REPEATER_PORT=5065
# export EPICS_CA_SERVER_PORT=5064
# export EPICS_PVA_ADDR_LIST="172.27.3.255 172.27.131.255 172.27.43.255 172.21.40.63 134.79.151.36 192.168.34.1 172.24.8.204:15076 134.79.216.44"
# export EPICS_PVA_AUTO_ADDR_LIST=NO
# export EPICS_PVA_BROADCAST_PORT=5076
# export EPICS_PVA_SERVER_PORT=5075