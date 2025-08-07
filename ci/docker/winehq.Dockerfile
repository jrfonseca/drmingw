FROM ubuntu:24.04
ENV container=docker

# See also:
# - https://dev.to/flpslv/running-winehq-inside-a-docker-container-52ej
# - https://gitlab.winehq.org/wine/wine/-/wikis/Debian-Ubuntu

ENV DEBIAN_FRONTEND=noninteractive

RUN \
 apt-get update && apt-get install -qq -y \
  ansible mingw-w64 ninja-build cmake python3 xinit xvfb && \
 rm -rf /var/lib/apt/lists/*

COPY ansible/winehq.yml /ansible/
RUN ansible-playbook -c local -i localhost, /ansible/winehq.yml && rm -rf /var/lib/apt/lists/*

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
