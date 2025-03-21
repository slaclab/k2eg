FROM ubuntu:noble

ENV DEBIAN_FRONTEND=noninteractive
ENV GITVERSION_VERSION=5.12.0
RUN     apt update \
    &&  apt install -y build-essential cmake git tclsh ninja-build gdb lcov libssl-dev libsasl2-dev curl libcurl4-openssl-dev valgrind libicu-dev jq file sudo

#install clang
RUN apt install -y lsb-release wget software-properties-common gnupg python3
# RUN bash -c "$(wget -O - https://apt.llvm.org/llvm.sh)"
ADD https://github.com/GitTools/GitVersion/releases/download/$GITVERSION_VERSION/gitversion-linux-x64-$GITVERSION_VERSION.tar.gz /opt
RUN cd /opt && tar zxvf gitversion-linux-x64-$GITVERSION_VERSION.tar.gz && \
    mv gitversion /usr/local/bin && chmod 755 /usr/local/bin/gitversion && \
    rm gitversion-linux-x64-$GITVERSION_VERSION.tar.gz

RUN update-alternatives --install /usr/bin/python python /usr/bin/python3 3

RUN useradd -rm -d /home/vscode -s /bin/bash -g root -G sudo -u 1001 developer
RUN echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers
USER developer
WORKDIR /home/vscode