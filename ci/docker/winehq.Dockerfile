FROM ubuntu:20.04
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
 wget -q -O - https://dl.winehq.org/wine-builds/winehq.key | apt-key add - && \
 echo 'deb https://dl.winehq.org/wine-builds/ubuntu/ focal main' > /etc/apt/sources.list.d/wine.list && \
 apt-get update && apt-get install -qq -y --install-recommends \
  mingw-w64 ninja-build cmake python3 xinit xvfb \
  winehq-devel && \
 rm -rf /var/lib/apt/lists/*

CMD ["/bin/bash"]
