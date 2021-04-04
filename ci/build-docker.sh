#!/bin/sh

set -e
set -x

docker_file=${docker_file:-winehq}
docker_tag=drmingw-$docker_file

test -t 0 && interactive=true || interactive=false
uid=$(id -u)

docker_run () {
    docker run \
	    --rm \
	    -i=$interactive \
	    --tty=$interactive \
	    -v "$PWD:$PWD" \
	    -w "$PWD" \
	    -e "BUILD_DIR=$PWD/build/docker-$docker_file" \
	    -u "$uid" \
	    $docker_tag \
	    "$@"
}

docker build -t $docker_tag -f ci/docker/$docker_file.Dockerfile ci/docker

docker_run ci/build.sh "$@"
