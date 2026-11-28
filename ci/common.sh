cmake_ () {
	set +x
	test "$1" = "-B"
	if test -f "$2/CMakeCache.txt"
	then
		# Reconfigure existing build trees only if ENABLE_COVERAGE changed
		local arg
		for arg in "$@"
		do
			case "$arg" in
			-DENABLE_COVERAGE=*)
				if ! grep -qx "ENABLE_COVERAGE:BOOL=${arg#-DENABLE_COVERAGE=}" "$2/CMakeCache.txt"
				then
					set -x
					cmake -B "$2" "$arg"
					set +x
				fi
				;;
			esac
		done
	else
		set -x
		cmake "$@"
	fi
	set -x
}


#
# Setup WINE
#

if [[ -v WSL_DISTRO_NAME ]]
then
	WINE=/usr/bin/env

	xwfb_run() {
		"$@"
	}
else
	WINE=${WINE:-$(which wine)}

	xwfb_run() {
		# https://gitlab.freedesktop.org/ofourdan/xwayland-run
		xwfb-run -n 8 -c weston -s \\-geometry -s 2024x768 -- "$@"
	}
fi

BUILD_DIR=${BUILD_DIR:-$PWD/build}

mkdir -p $BUILD_DIR

if [[ ! -v WSL_DISTRO_NAME ]]
then
	$WINE --version

	# Prevent Gecko/Mono installation dialogues
	# https://forum.winehq.org/viewtopic.php?f=2&t=16320#p78458
	export WINEDLLOVERRIDES="mscoree,mshtml="

	export WINEPREFIX=$BUILD_DIR/wine
	if ! test -d $WINEPREFIX
	then
		xwfb_run $WINE wineboot.exe --init
		$WINE reg.exe ADD 'HKCU\Software\Wine\winedbg' /v ShowCrashDialog /t REG_DWORD /d 0 /f
	fi

	export WINEDEBUG="${WINEDEBUG:--all,+debugstr}"
fi


CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE:-Debug}
if [ "${GITHUB_EVENT_NAME:-}" = "push" ]
then
	case "${GITHUB_REF:-}" in
	refs/tags/*)
		CMAKE_BUILD_TYPE=Release
		;;
	esac
fi


#
# Code coverage (enable with COVERAGE=1)
#

COVERAGE=${COVERAGE:-0}
if [ "$CMAKE_BUILD_TYPE" = "Release" ]
then
	COVERAGE=0
fi
if [ "$COVERAGE" -gt 0 ]
then
	gcovr --version
fi

# Discard counters from previous runs
coverage_reset() {
	if [ "$COVERAGE" -gt 0 ]
	then
		find "$1" -name '*.gcda' -delete
	fi
}

# coverage_report <build-dir> <gcov-executable>
coverage_report() {
	if [ "$COVERAGE" -gt 0 ]
	then
		rm -f "$1"/coverage*.html "$1"/coverage.css
		gcovr \
			--gcov-executable "$2" \
			--exclude-unreachable-branches \
			--exclude-throw-branches \
			--root "$PWD" \
			--exclude 'downloads/' \
			--exclude-directories '(.*/)?wine$' \
			--exclude-directories '/tmp/claude-[0-9]+(/.*)?$' \
			--object-directory "$1" \
			--html-details -o "$1/coverage.html" \
			--print-summary \
			"$1"
	fi
}
