#!/bin/bash
export PATH=$PATH:/opt/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/local/lib:/opt/local/lib/linux-x86_64
#waith for completion accordin to WAIT_HOSTS from external laucher
ls -la
/opt/wait
/opt/k2eg-test