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
    # See:
    # - https://dev.to/dtinth/caching-docker-builds-in-github-actions-which-approach-is-the-fastest-a-research-18ei
    # - https://github.com/dtinth/github-actions-docker-layer-caching-poc/blob/master/.github/workflows/dockerimage.yml
    docker pull docker.pkg.github.com/$GITHUB_REPOSITORY/build-cache || true
    docker buildx build -t $docker_tag --cache-from=docker.pkg.github.com/$GITHUB_REPOSITORY/build-cache -f ci/docker/$docker_file.Dockerfile ci/docker
    docker tag $docker_tag docker.pkg.github.com/$GITHUB_REPOSITORY/build-cache && docker push docker.pkg.github.com/$GITHUB_REPOSITORY/build-cache || true
else
    docker buildx build -t $docker_tag -f ci/docker/$docker_file.Dockerfile ci/docker
fi

# Set `build_script=ci/build-clang.sh` to build and run Clang tests.
docker_run ${build_script:-ci/build.sh} "$@"
