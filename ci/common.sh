cmake_ () {
	test "$1" = "-B"
	test -f "$2/CMakeCache.txt" || cmake "$@"
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
