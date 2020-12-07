param (
    [ValidateSet('mingw64','mingw32')][string]$target = 'mingw64',
    [string]$buildRoot = 'build'
)

# https://stackoverflow.com/a/48999101
Set-StrictMode -Version latest
$ErrorActionPreference = "Stop"
function Exec {
    param(
        [Parameter(Position=0,Mandatory=1)][scriptblock]$cmd
    )
    Write-Host "> " + $cmd.ToString().Trim()
    & $cmd
    if ($LastExitCode -ne 0) {
        throw
    }
}

#
# Download and extract MinGW-w64
#
if ($target -eq 'mingw64') {
    $MINGW_URL = 'https://downloads.sourceforge.net/project/mingw-w64/Toolchains%20targetting%20Win64/Personal%20Builds/mingw-builds/7.3.0/threads-win32/seh/x86_64-7.3.0-release-win32-seh-rt_v5-rev0.7z'
} else {
    $MINGW_URL = 'https://downloads.sourceforge.net/project/mingw-w64/Toolchains%20targetting%20Win32/Personal%20Builds/mingw-builds/7.3.0/threads-win32/dwarf/i686-7.3.0-release-win32-dwarf-rt_v5-rev0.7z'
}
$MINGW_ARCHIVE = Split-Path -leaf $MINGW_URL
if (!(Test-Path $MINGW_ARCHIVE -PathType Leaf)) {
    Write-Host "Downloading $MINGW_URL"
    Invoke-WebRequest -Uri $MINGW_URL -OutFile $MINGW_ARCHIVE -UserAgent NativeHost
    Get-Item $MINGW_ARCHIVE
}
if (!(Test-Path $target -PathType Container)) {
    Write-Host "Extracting $MINGW_ARCHIVE"
    Exec { 7z x -y $MINGW_ARCHIVE | Out-Null }
}

#
# Setup environment
#
$cwd = Get-Location
try {
    Get-Command python.exe -CommandType Application | Out-Null
} catch {
    $Env:Path = "${Env:ProgramFiles(x86)}\Microsoft Visual Studio\Shared\Python37_64;$Env:Path"
}
$Env:Path = "$cwd\$target\bin;$Env:Path"

Exec { g++ --version }
Exec { mingw32-make --version }
Exec { cmake --version }
Exec { python --version }

if ($target -eq 'mingw64') {
    $WINDBG_DIR = "${Env:ProgramFiles(x86)}\Windows Kits\10\Debuggers\x64"
} else {
    $WINDBG_DIR = "${Env:ProgramFiles(x86)}\Windows Kits\10\Debuggers\x86"
}
(Get-Item "$WINDBG_DIR\dbghelp.dll").VersionInfo

#
# Configure
#
if ($Env:APPVEYOR_REPO_TAG -eq "true") {
    $CMAKE_BUILD_TYPE = 'Release'
} else {
    $CMAKE_BUILD_TYPE = 'Debug'
}
if (Test-Path Env:ENABLE_COVERAGE) {
    $ENABLE_COVERAGE = ${Env:ENABLE_COVERAGE}
} else {
    $ENABLE_COVERAGE = "no"
}
$buildDir = "$buildRoot\$target"
Exec { cmake "-H." "-B$buildDir" -G "MinGW Makefiles" "-DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE" "-DENABLE_COVERAGE=$ENABLE_COVERAGE" "-DWINDBG_DIR=$WINDBG_DIR" }

#
# Build
#
Exec { cmake --build $buildDir --use-stderr --target all "--" "-j${Env:NUMBER_OF_PROCESSORS}" }

#
# Test
#
$Env:Path = "$WINDBG_DIR;$Env:Path"
Exec { cmake --build $buildDir --use-stderr --target test }
Exec { cmake -Htests\apps "-B$buildRoot\msvc32" -G "Visual Studio 16 2019" -A Win32 "-DCMAKE_SYSTEM_VERSION=10.0.16299.0" }
Exec { cmake --build "$buildRoot\msvc32" --config Debug "--" /verbosity:minimal /maxcpucount }
if ($target -eq "mingw64") {
    Exec { cmake -Htests\apps "-B$buildRoot\msvc64" -G "Visual Studio 16 2019" -A x64 "-DCMAKE_SYSTEM_VERSION=10.0.16299.0" }
    Exec { cmake --build "$buildRoot\msvc64" --config Debug "--" /verbosity:minimal /maxcpucount }
    Exec { python tests\apps\test.py $buildDir\bin\catchsegv.exe "$buildRoot\msvc32\Debug" "$buildRoot\msvc64\Debug" }
} else {
    Exec { python tests\apps\test.py $buildDir\bin\catchsegv.exe "$buildRoot\msvc32\Debug" }
}

#
# Package
#
if (Test-Path Env:APPVEYOR) {
    Exec { cmake --build $buildDir --use-stderr --target package }
}
