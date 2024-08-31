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

if ! docker buildx version
then
   echo 'error: docker buildx plugin required (sudo apt install docker-buildx)'
   exit 1
fi

docker buildx build -t $docker_tag -f ci/docker/$docker_file.Dockerfile ci/docker

docker_run ci/build.sh "$@"
