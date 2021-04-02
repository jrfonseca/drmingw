param (
    [ValidateSet('mingw64','mingw32')][string]$target = 'mingw64',
    [string]$buildRoot = 'build',
    [switch]$coverage = $false  # https://stackoverflow.com/q/5079413
)

# https://stackoverflow.com/a/48999101
Set-StrictMode -Version latest
$ErrorActionPreference = "Stop"
function Exec {
    param(
        [Parameter(Position=0,Mandatory=1)][scriptblock]$cmd
    )
    Write-Host ("> " + $cmd.ToString().Trim())
    & $cmd
    if ($LastExitCode -ne 0) {
        throw
    }
}

#
# Download and extract MinGW-w64 toolchain
#
New-Item -ItemType Directory -Force -Path downloads | Out-Null
if ($target -eq 'mingw64') {
    $MINGW_URL = 'https://sourceforge.net/projects/mingw-w64/files/Toolchains%20targetting%20Win64/Personal%20Builds/mingw-builds/8.1.0/threads-win32/seh/x86_64-8.1.0-release-win32-seh-rt_v6-rev0.7z'
} else {
    $MINGW_URL = 'https://sourceforge.net/projects/mingw-w64/files/Toolchains%20targetting%20Win32/Personal%20Builds/mingw-builds/8.1.0/threads-win32/dwarf/i686-8.1.0-release-win32-dwarf-rt_v6-rev0.7z'
}
$MINGW_ARCHIVE = Split-Path -leaf $MINGW_URL
$MINGW_ARCHIVE = "downloads\$MINGW_ARCHIVE"
if (!(Test-Path $MINGW_ARCHIVE -PathType Leaf)) {
    Write-Host "Downloading $MINGW_URL ..."
    Invoke-WebRequest -Uri $MINGW_URL -OutFile $MINGW_ARCHIVE -UserAgent NativeHost
    Get-Item $MINGW_ARCHIVE
}
New-Item -ItemType Directory -Force -Path "$buildRoot\toolchain" | Out-Null
$toolchain = "$buildRoot\toolchain\$target"
$toolchain = [System.IO.Path]::GetFullPath($toolchain)
if (!(Test-Path $toolchain -PathType Container)) {
    Write-Host "Extracting $MINGW_ARCHIVE to $toolchain ..."
    Exec { 7z x -y "-o$buildRoot\toolchain" $MINGW_ARCHIVE | Out-Null }
}
$Env:Path = "$toolchain\bin;$Env:Path"
(Get-Command 'g++.exe').Source
Exec { g++ --version }

#
# Setup environment
#
$cwd = Get-Location
try {
    Get-Command python.exe -CommandType Application | Out-Null
} catch [System.Management.Automation.CommandNotFoundException] {
    $Env:Path = "${Env:ProgramFiles(x86)}\Microsoft Visual Studio\Shared\Python37_64;$Env:Path"
}
Exec { python --version }

try {
    Get-Command ninja.exe -CommandType Application | Out-Null
    Exec { ninja --version }
    $generator = "Ninja"
} catch [System.Management.Automation.CommandNotFoundException] {
    Exec { mingw32-make --version }
    $generator = "MinGW Makefiles"
    $Env:MAKEFLAGS = "-j${Env:NUMBER_OF_PROCESSORS}"
}

Exec { cmake --version }

if ($target -eq 'mingw64') {
    $WINDBG_DIR = "${Env:ProgramFiles(x86)}\Windows Kits\10\Debuggers\x64"
} else {
    $WINDBG_DIR = "${Env:ProgramFiles(x86)}\Windows Kits\10\Debuggers\x86"
}
(Get-Item "$WINDBG_DIR\dbghelp.dll").VersionInfo.FileVersion

#
# Configure
#
$CMAKE_BUILD_TYPE = 'Debug'
if ($Env:GITHUB_EVENT_NAME -eq "push" -And $Env:GITHUB_REF.StartsWith('refs/tags/')) {
    $CMAKE_BUILD_TYPE = 'Release'
    $coverage = $false
}
$buildDir = "$buildRoot\$target"
Exec { cmake "-H." "-B$buildDir" -G $generator "-DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE" "-DENABLE_COVERAGE=$coverage" "-DWINDBG_DIR=$WINDBG_DIR" }

#
# Build
#
Exec { cmake --build $buildDir --use-stderr --target all }

#
# Test
#
$Env:Path = "$WINDBG_DIR;$Env:Path"
$Env:CTEST_OUTPUT_ON_FAILURE = '1'
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
# Code coverage
#
if ($coverage) {
    Exec { gcov --version }
    if (Test-Path Env:CODECOV_TOKEN) {
        Exec { gcovr --object-directory $buildDir --xml -o "cobertura.xml" }
        Exec { codecov --file cobertura.xml -X gcov search }
    } else {
        Exec { gcovr --object-directory $buildDir --html-details -o "$buildDir\coverage.html" }
    }
}

#
# Package
#
if (Test-Path Env:CI) {
    Exec { cmake --build $buildDir --use-stderr --target package }
}
