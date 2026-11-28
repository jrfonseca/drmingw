#!/bin/bash
#
# Run a command with a headless display, as needed by Wine to run tests that
# create windows (and by some that merely show message boxes.)  For example:
#
#   ci/headless.sh wine wineboot.exe --init
#   ci/headless.sh cmake --build build/mingw64 --target check
#
# Each invocation gets its own display, so several can run concurrently.
#
# On WSL, Windows executables run natively and use the Windows desktop, so the
# command is run as is.
#

set -eu

if [[ -v WSL_DISTRO_NAME ]]
then
	exec "$@"
fi

# https://gitlab.freedesktop.org/ofourdan/xwayland-run
exec xwfb-run -c weston -s \\-geometry -s 2024x768 -- "$@"
