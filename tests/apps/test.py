#!/usr/bin/env python3
###########################################################################
#
# Copyright 2015-2018 Jose Fonseca
# All Rights Reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sub license, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. NO EVENT SHALL
# THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
# DAMAGES OR OTHER LIABILITY, WHETHER AN ACTION OF CONTRACT, TORT OR
# OTHERWISE, ARISING FROM, OF OR CONNECTION WITH THE SOFTWARE OR THE
# USE OR OTHER DEALINGS THE SOFTWARE.
#
# The above copyright notice and this permission notice (including the
# next paragraph) shall be included in all copies or substantial portions
# of the Software.
#
###########################################################################


import glob
import sys
import subprocess
import os.path
import re
import optparse
import tempfile
import threading
import multiprocessing.dummy as multiprocessing

from multiprocessing import cpu_count


assert sys.version_info.major >= 3


stdoutLock = threading.Lock()

def writeStdout(s):
    with stdoutLock:
        sys.stdout.write(s)
        sys.stdout.flush()


def checkString(needle, haystack):
    if needle.startswith('/'):
        assert needle.endswith('/')
        needle = needle[1:-1]
    else:
        needle = '^' + re.escape(needle) + '$'
    needleRe = re.compile(needle, re.MULTILINE)
    return needleRe.search(haystack) is not None


def haveAnsiEscapes():
    if sys.platform != 'win32':
        return True

    # Set output mode to handle virtual terminal sequences
    # https://msdn.microsoft.com/en-us/library/windows/desktop/mt638032.aspx
    import ctypes.wintypes
    STD_OUTPUT_HANDLE = -11
    hConsoleHandle = ctypes.windll.kernel32.GetStdHandle(STD_OUTPUT_HANDLE)
    if hConsoleHandle:
        dwMode = ctypes.wintypes.DWORD()
        if ctypes.windll.kernel32.GetConsoleMode(hConsoleHandle, ctypes.byref(dwMode)):
            ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x0004
            dwMode = ctypes.wintypes.DWORD(dwMode.value | ENABLE_VIRTUAL_TERMINAL_PROCESSING)
            if ctypes.windll.kernel32.SetConsoleMode(hConsoleHandle, dwMode):
                return True

    if os.environ.get('APPVEYOR', 'False') == 'True':
        # https://help.appveyor.com/discussions/suggestions/197-support-ansi-color-codes
        return True;

    return False


if haveAnsiEscapes():
    _csi = '\33['
    NORMAL = _csi + '0m'
    RED = _csi + '31m'
    GREEN = _csi + '32m'
else:
    NORMAL = ''
    RED = ''
    GREEN = ''


def test(args):
    catchsegvExe, testExe, testSrc = args

    result = True

    cmd = [
        catchsegvExe,
        '-v',
        '-t', '30',
        testExe
    ]

    if sys.platform != 'win32':
        cmd = [options.wine] + cmd
        if options.verbose:
            os.environ['WINEDEBUG'] = '+debugstr'

    writeStdout('# ' + ' '.join(cmd) + '\n')

    # XXX: Popen.communicate takes a lot of time with wine, so avoid it
    stdout = tempfile.TemporaryFile()
    stderr = tempfile.TemporaryFile()

    # Isolate this python script from console events
    creationflags = 0
    if sys.platform == 'win32':
        testName, _ = os.path.splitext(os.path.basename(testSrc))
        if testName.startswith('ctrl_'):
            creationflags |= subprocess.CREATE_NEW_CONSOLE

    p = subprocess.Popen(cmd, stdout=stdout, stderr=stderr, creationflags=creationflags)
    p.wait()

    stdout.seek(0)
    stderr.seek(0)

    stdout = stdout.read()
    stderr = stderr.read()

    stdout = stdout.replace(b'\r\n', b'\n')
    stderr = stderr.replace(b'\r\n', b'\n')

    stdout = stdout.decode(errors='replace')
    stderr = stderr.decode(errors='replace')

    if options.verbose:
        with stdoutLock:
            sys.stderr.write(stderr)
            sys.stderr.write(stdout)

    # Adjust return code
    exitCode = p.returncode
    if exitCode < 0:
        exitCode += (1 << 32)
    if exitCode == 0x4000001f:
        exitCode = 0x80000003

    if exitCode == 125:
        # skip
        writeStdout('%sok - %s # skip%s\n' % (GREEN, testExe, NORMAL))
    else:
        # Search the source file for '// CHECK_...' annotations and process
        # them.

        checkCommentRe = re.compile(r'^// CHECK_([_0-9A-Z]+):\s+(.*)$')
        for line in open(testSrc, 'rt'):
            line = line.rstrip('\n')
            mo = checkCommentRe.match(line)
            if mo:
                checkName = mo.group(1)
                checkExpr = mo.group(2)
                if checkName == 'EXIT_CODE':
                    inverse = False
                    if checkExpr.startswith('!'):
                        inverse = True
                        checkExpr = checkExpr[1:]
                    if checkExpr.startswith('0x'):
                        checkExitCode = int(checkExpr, 16)
                    else:
                        checkExitCode = int(checkExpr)
                    if sys.platform != 'win32':
                        checkExitCode = checkExitCode % 256
                    ok = exitCode == checkExitCode 
                    if inverse:
                        ok = not ok
                    if not ok:
                        writeStdout('# exit code was 0x%08x\n' % exitCode)
                elif checkName == 'STDOUT':
                    ok = checkString(checkExpr, stdout)
                elif checkName == 'STDERR':
                    ok = checkString(checkExpr, stderr)
                else:
                    assert False

                ok_or_not = [RED + 'not ok', GREEN + 'ok']
                checkExpr = mo.group(2)
                writeStdout('%s - %s %s %s%s\n' % (ok_or_not[int(bool(ok))], testExe, 'CHECK_' + checkName, checkExpr, NORMAL))
                if not ok:
                    result = False
    
    if not result and not options.verbose:
        sys.stderr.write(stderr)
        sys.stderr.write(stdout)

    return testExe, result


def main():
    optparser = optparse.OptionParser( usage="%prog [options] [path/to/catchsegv.exe] [path/to/test/apps] ..")
    optparser.add_option(
        '-w', '--wine', metavar='WINE_PROGRAM',
        type="string", dest="wine",
        default = 'wine')
    optparser.add_option(
        '-R', '--regex', metavar='REGEX',
        type="string", dest="regex",
        default = '.*')
    optparser.add_option(
        '-v', '--verbose',
        action="store_true",
        dest="verbose", default=False)
    
    global options
    (options, args) = optparser.parse_args(sys.argv[1:])

    if len(args) >= 1:
        catchsegvExe = args[0]
    else:
        catchsegvExe = os.path.join('bin', 'catchsegv.exe')
    if not os.path.isfile(catchsegvExe):
        optparser.error('error: %s does not exist\n' % catchsegvExe)
        sys.exit(1)

    if len(args) >= 2:
        testsExeDirs = args[1:]
    else:
        testsExeDirs = [os.path.normpath(os.path.join(os.path.dirname(catchsegvExe), '..', 'tests', 'apps'))]

    testsSrcDir = os.path.dirname(__file__)

    testNameRe = re.compile(options.regex)

    if sys.platform == 'win32':
        import ctypes
        SEM_FAILCRITICALERRORS     = 0x0001
        SEM_NOALIGNMENTFAULTEXCEPT = 0x0004
        SEM_NOGPFAULTERRORBOX      = 0x0002
        SEM_NOOPENFILEERRORBOX     = 0x8000
        uMode = ctypes.windll.kernel32.SetErrorMode(0)
        uMode |= SEM_FAILCRITICALERRORS \
              |  SEM_NOALIGNMENTFAULTEXCEPT \
              |  SEM_NOGPFAULTERRORBOX \
              |  SEM_NOOPENFILEERRORBOX
        ctypes.windll.kernel32.SetErrorMode(uMode)

    failedTests = []

    numJobs = cpu_count()
    pool = multiprocessing.Pool(numJobs)

    testSrcFiles = os.listdir(testsSrcDir)
    testSrcFiles.sort()

    testArgs = []
    for testSrcFile in testSrcFiles:
        testName, ext = os.path.splitext(testSrcFile)

        if ext not in ('.c', '.cpp'):
            continue

        if not testNameRe.search(testName):
            continue

        if sys.platform != 'win32' and testName in ('ctrl_break', 'ctrl_c'):
            continue

        testSrc = os.path.join(testsSrcDir, testSrcFile)

        for testsExeDir in testsExeDirs:
            testExe = os.path.join(testsExeDir, testName + '.exe')
            if not os.path.isfile(testExe):
                sys.stderr.write('fatal: %s does not exist\n' % testExe)
                sys.exit(1)

            testArgs.append((catchsegvExe, testExe, testSrc))

    if numJobs <= 1 or len(testArgs) <= 1:
        imap = map
    else:
        imap = pool.imap_unordered

    for testName, testResult in imap(test, testArgs):
        if not testResult:
            failedTests.append(testName)

    #sys.stdout.write('1..%u\n' % numTests)
    if failedTests:
        sys.stdout.write('# %u tests failed\n' % len(failedTests))
        for failedTest in failedTests:
            sys.stdout.write('# - %s\n' % failedTest)
        sys.exit(1)

    sys.exit(0)


if __name__ == '__main__':
    main()
