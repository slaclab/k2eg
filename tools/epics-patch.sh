#!/bin/bash
OS=$(uname)
CXX_COMPILER=$2
C_COMPILER=$3
echo $CXX_COMPILER
echo $C_COMPILER
if [[ ${OS} == "Darwin" ]]
then
    echo "OSX patch"
    sed -i '' "s/OPT_CFLAGS_YES += -g/OPT_CFLAGS_YES += -g -std=c11/" $1/epics/src/epics/configure/os/CONFIG.darwinCommon.darwinCommon
    sed -i '' "s/OPT_CXXFLAGS_YES += -g/OPT_CXXFLAGS_YES += -g -std=c++11/" $1/epics/src/epics/configure/os/CONFIG.darwinCommon.darwinCommon 
elif [[ ${OS} == "Linux" ]] && [[ $COMPILER == *clang* ]] then
    echo "Patch for clang on linux"
    sed -i -e "s/OPT_CFLAGS_YES += -g/OPT_CFLAGS_YES += -g -std=c11/" $1/epics/src/epics/configure/os/CONFIG_SITE.Common.linuxCommon
    sed -i -e "s/OPT_CXXFLAGS_YES += -g/OPT_CXXFLAGS_YES += -g -std=c++11/" $1/epics/src/epics/configure/os/CONFIG_SITE.Common.linuxCommon
    sed -i -e "s/#GNU         = NO/GNU         = NO/" $1/epics/src/epics/configure/os/CONFIG_SITE.Common.linux-x86_64
    sed -i -e "s/#CMPLR_CLASS = clang/CMPLR_CLASS = $C_COMPILER/" $1/epics/src/epics/configure/os/CONFIG_SITE.Common.linux-x86_64
    sed -i -e "s/#CC          = clang/CC          = $C_COMPILER/" $1/epics/src/epics/configure/os/CONFIG_SITE.Common.linux-x86_64
    sed -i -e "s/#CCC         = clang++/CCC         = $CXX_COMPILER/" $1/epics/src/epics/configure/os/CONFIG_SITE.Common.linux-x86_64
fi

