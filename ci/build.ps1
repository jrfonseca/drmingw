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


. .\ci\dependencies.ps1


#
# Download and extract MinGW-w64 toolchain
#
New-Item -ItemType Directory -Force -Path downloads | Out-Null
if ($target -eq 'mingw64') {
    $MINGW_URL = $MINGW_64_URL
    $MINGW_SUM = $MINGW_64_SUM
    $DBGHELP_URL = $DBGHELP_64_URL
    $DBGHELP_SUM = $DBGHELP_64_SUM
} else {
    $MINGW_URL = $MINGW_32_URL
    $MINGW_SUM = $MINGW_32_SUM
    $DBGHELP_URL = $DBGHELP_32_URL
    $DBGHELP_SUM = $DBGHELP_32_SUM
}
$MINGW_ARCHIVE = Split-Path -leaf $MINGW_URL
$MINGW_ARCHIVE = "downloads\$MINGW_ARCHIVE"
if (!(Test-Path $MINGW_ARCHIVE -PathType Leaf)) {
    Write-Host "Downloading $MINGW_URL ..."
    Invoke-WebRequest -Uri $MINGW_URL -OutFile $MINGW_ARCHIVE -UserAgent NativeHost
    $hash = (Get-FileHash $MINGW_ARCHIVE -Algorithm SHA256).Hash
    if ($hash -ne $MINGW_SUM) {
        echo "error: ${MINGW_ARCHIVE}: wrong hash: ${hash}"
        exit 1
    }
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

$DBGHELP_ARCHIVE = "downloads\dbghelp-$target.7z"
$DBGHELP_DIR = "downloads\dbghelp\$target"
if (!(Test-Path $DBGHELP_DIR -PathType Container)) {
    if (!(Test-Path $DBGHELP_ARCHIVE -PathType Leaf)) {
        Invoke-WebRequest -Uri $DBGHELP_URL -OutFile $DBGHELP_ARCHIVE -UserAgent NativeHost
        $hash = (Get-FileHash $DBGHELP_ARCHIVE -Algorithm SHA256).Hash
        if ($hash -ne $DBGHELP_SUM) {
            echo "error: ${DBGHELP_ARCHIVE}: wrong hash: ${hash}"
            exit 1
        }
    }
    Exec { 7z x -y "-o$DBGHELP_DIR" $DBGHELP_ARCHIVE | Out-Null }
}

$NINJA_ARCHIVE = 'downloads\ninja-win.zip'
$NINJA_DIR = 'downloads\ninja'
$NINJA_DIR = [System.IO.Path]::GetFullPath($NINJA_DIR)
if (!(Test-Path "$NINJA_DIR\ninja.exe" -PathType Leaf)) {
    if (!(Test-Path $NINJA_ARCHIVE -PathType Leaf)) {
        Invoke-WebRequest -Uri $NINJA_URL -OutFile $NINJA_ARCHIVE -UserAgent NativeHost
        $hash = (Get-FileHash $NINJA_ARCHIVE -Algorithm SHA256).Hash
        if ($hash -ne $NINJA_SUM) {
            echo "error: ${NINJA_ARCHIVE}: wrong hash: ${hash}"
            exit 1
        }
    }
    Expand-Archive -Path $NINJA_ARCHIVE -DestinationPath $NINJA_DIR -Force
}
$Env:Path = "$NINJA_DIR;$Env:Path"


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

Exec { ninja --version }

Exec { cmake --version }

(Get-Item "$DBGHELP_DIR\dbghelp.dll").VersionInfo.FileVersion

#
# Configure
#
$CMAKE_BUILD_TYPE = 'Debug'
if ($Env:GITHUB_EVENT_NAME -eq "push" -And $Env:GITHUB_REF.StartsWith('refs/tags/')) {
    $CMAKE_BUILD_TYPE = 'Release'
    $coverage = $false
}
$buildDir = "$buildRoot\$target"
Exec { cmake "-S." "-B$buildDir" -G "Ninja" "-DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE" "-DENABLE_COVERAGE=$coverage" "-DWINDBG_DIR=$DBGHELP_DIR" }

#
# Build
#
Exec { cmake --build $buildDir --use-stderr --target all }

Exec { python tests\check_dynamic_linkage.py --objdump=objdump --validate $buildDir\bin\*.dll $buildDir\bin\*.exe }

#
# Test
#
$Env:Path = "$DBGHELP_DIR;$Env:Path"
$Env:CTEST_OUTPUT_ON_FAILURE = '1'
Exec { cmake --build $buildDir --use-stderr --target test }

# MinGW GCC
Exec { cmake "-S" tests\apps "-B" "$buildRoot\apps\$target" -G "Ninja" "-DCMAKE_BUILD_TYPE=Debug" }
Exec { cmake --build "$buildRoot\apps\$target" }
# MSVC 32-bits
Exec { cmake "-S" tests\apps "-B" "$buildRoot\apps\msvc32" -G "Visual Studio 17 2022" -A Win32 }
Exec { cmake --build "$buildRoot\apps\msvc32" --config Debug "--" /verbosity:minimal /maxcpucount }
if ($target -eq "mingw64") {
    # MSVC 64-bits
    Exec { cmake -Stests\apps "-B$buildRoot\apps\msvc64" -G "Visual Studio 17 2022" -A x64 }
    Exec { cmake --build "$buildRoot\apps\msvc64" --config Debug "--" /verbosity:minimal /maxcpucount }

    Exec { python ci\spawndesk.py python tests\apps\test.py $buildDir\bin\catchsegv.exe "$buildRoot\apps\$target" "$buildRoot\apps\msvc32\Debug" "$buildRoot\apps\msvc64\Debug" }
} else {
    Exec { python ci\spawndesk.py python tests\apps\test.py $buildDir\bin\catchsegv.exe "$buildRoot\apps\$target" "$buildRoot\apps\msvc32\Debug" }
}

#
# Code coverage
#
if ($coverage) {
    Exec { gcov --version }
    if ($Env:GITHUB_ACTIONS -eq "true") {
        Exec { python -m gcovr --exclude-unreachable-branches --exclude-throw-branches --object-directory $buildDir --xml -o "cobertura.xml" }
    } else {
        Exec { python -m gcovr --exclude-unreachable-branches --exclude-throw-branches --object-directory $buildDir --html-details -o "$buildDir\coverage.html" }
    }
}

#
# Package
#
if (Test-Path Env:CI) {
    Exec { cmake --build $buildDir --use-stderr --target package }
}
