FROM ubuntu:22.04
MAINTAINER "Jose Fonseca" <jose.r.fonseca@gmail.com>
ENV container docker

# See also:
# - https://dev.to/flpslv/running-winehq-inside-a-docker-container-52ej
# - https://wiki.winehq.org/Ubuntu

ENV DEBIAN_FRONTEND=noninteractive

RUN \
 dpkg --add-architecture i386 && \
 apt-get update && apt-get install -y \
  apt-transport-https ca-certificates gnupg software-properties-common wget && \
 wget -q -O /etc/apt/trusted.gpg.d/winehw.asc https://dl.winehq.org/wine-builds/winehq.key && \
 echo 'deb https://dl.winehq.org/wine-builds/ubuntu/ jammy main' > /etc/apt/sources.list.d/wine.list && \
 apt-get update && apt-get install -qq -y --install-recommends \
  mingw-w64 ninja-build cmake python3 xinit xvfb \
  winehq-devel && \
 rm -rf /var/lib/apt/lists/*

# https://stackoverflow.com/questions/28405902/how-to-set-the-locale-inside-a-debian-ubuntu-docker-container
RUN \
 apt-get update && apt-get install -qq -y \
  locales && \
 rm -rf /var/lib/apt/lists/* && \
 locale-gen --no-purge en_US.UTF-8
ENV \
  LANG=en_US.UTF-8 \
  LANGUAGE=en_US \
  LC_ALL=en_US.UTF-8

CMD ["/bin/bash"]
