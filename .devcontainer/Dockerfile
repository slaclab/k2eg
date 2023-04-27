ARG GCC_VERSION=12.2.0-bullseye
FROM gcc:$GCC_VERSION
LABEL maintainer="bisegni@slac.stanford.edu"

ARG REINSTALL_CMAKE_VERSION_FROM_SOURCE="3.23.5"

RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
     && apt-get -y update \
     && apt-get -y install --no-install-recommends software-properties-common ninja-build tclsh gdb libssl-dev

COPY ./reinstall-cmake.sh /tmp/

RUN if [ "${REINSTALL_CMAKE_VERSION_FROM_SOURCE}" != "none" ]; then \
        chmod +x /tmp/reinstall-cmake.sh && /tmp/reinstall-cmake.sh ${REINSTALL_CMAKE_VERSION_FROM_SOURCE}; \
    fi \
    && rm -f /tmp/reinstall-cmake.sh

RUN update-alternatives --install /usr/bin/gcc gcc /usr/local/bin/gcc 100
RUN update-alternatives --install /usr/bin/g++ g++ /usr/local/bin/g++ 100
# Install llvm
# COPY tools/llvm.sh /tmp/
# RUN chmod +x /tmp/llvm.sh && /tmp/llvm.sh
# RUN update-alternatives --install /usr/bin/cc cc /usr/bin/clang-14 100
# RUN update-alternatives --install /usr/bin/c++ c++ /usr/bin/clang++-14 100
# RUN update-alternatives --install /usr/bin/clang cc /usr/bin/clang-14 100
# RUN update-alternatives --install /usr/bin/clang++ c++ /usr/bin/clang++-14 100
# RUN update-alternatives --install /usr/bin/lldb lldb /usr/bin/lldb-14 100

RUN useradd -rm -d /home/vscode -s /bin/bash -g root -G sudo -u 1001 developer
USER developer
WORKDIR /home/vscode