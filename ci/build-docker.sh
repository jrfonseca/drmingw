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
   echo 'error: docker buildx plugin required (sudo apt install docker-buildx)' 1>&2
   exit 1
fi

if [ "$GITHUB_ACTIONS" = true ]
then
    # https://docs.docker.com/build/cache/backends/gha/
    docker buildx build \
        -t $docker_tag \
        --cache-from type=gha,scope=$docker_file \
        --cache-to type=gha,mode=max,scope=$docker_file \
        --load \
        -f ci/docker/$docker_file.Dockerfile ci/docker
else
    docker buildx build -t $docker_tag -f ci/docker/$docker_file.Dockerfile ci/docker
fi

# Set `build_script=ci/build-clang.sh` to build and run Clang tests.
docker_run ${build_script:-ci/build.sh} "$@"
