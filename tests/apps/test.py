#!/usr/bin/env python
###########################################################################
#
# Copyright 2015 Jose Fonseca
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


def test((catchsegvExe, testExe, testSrc)):
    result = True

    cmd = [
        catchsegvExe,
        '-v',
        '-t', '5',
        testExe
    ]

    if sys.platform != 'win32':
        cmd = ['wine'] + cmd
        if options.verbose:
            os.environ['WINEDEBUG'] = '+debugstr'

    writeStdout('# ' + ' '.join(cmd) + '\n')

    # XXX: Popen.communicate takes a lot of time with wine, so avoid it
    stdout = tempfile.TemporaryFile()
    stderr = tempfile.TemporaryFile()

    p = subprocess.Popen(cmd, stdout=stdout, stderr=stderr)
    p.wait()

    stdout.seek(0)
    stderr.seek(0)

    stdout = stdout.read()
    stderr = stderr.read()

    stdout = stdout.replace('\r\n', '\n')
    stderr = stderr.replace('\r\n', '\n')

    if options.verbose:
        sys.stderr.write(stderr)
        sys.stderr.write(stdout)

    # Adjust return code
    exitCode = p.returncode
    if exitCode < 0:
        exitCode += (1 << 32)
    if exitCode == 0x4000001f:
        exitCode = 0x80000003

    # Search the source file for '// CHECK_...' annotations and process
    # them.

    checkCommentRe = re.compile(r'^// CHECK_([_0-9A-Z]+): (.*)$')
    for line in open(testSrc, 'rt'):
        line = line.rstrip('\n')
        mo = checkCommentRe.match(line)
        if mo:
            checkName = mo.group(1)
            checkExpr = mo.group(2)
            if checkName == 'EXIT_CODE':
                if checkExpr.startswith('0x'):
                    checkExitCode = int(checkExpr, 16)
                else:
                    checkExitCode = int(checkExpr)
                if sys.platform != 'win32':
                    checkExitCode = checkExitCode % 256
                ok = exitCode == checkExitCode 
                if not ok:
                    writeStdout('# exit code was 0x%08x\n' % exitCode)
            elif checkName == 'STDOUT':
                ok = checkString(checkExpr, stdout)
            elif checkName == 'STDERR':
                ok = checkString(checkExpr, stderr)
            else:
                assert False

            ok_or_not = ['not ok', 'ok']
            writeStdout('%s - %s %s %s\n' % (ok_or_not[int(bool(ok))], testExe, 'CHECK_' + checkName, checkExpr))
            if not ok:
                result = False
    
    if not result and not options.verbose:
        sys.stderr.write(stderr)
        sys.stderr.write(stdout)

    return testExe, result


def main():
    optparser = optparse.OptionParser( usage="%prog [options] [path/to/catchsegv.exe] [path/to/test/apps] ..")
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

    numJobs = multiprocessing.cpu_count()
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

        testSrc = os.path.join(testsSrcDir, testSrcFile)

        for testsExeDir in testsExeDirs:
            testExe = os.path.join(testsExeDir, testName + '.exe')
            if not os.path.isfile(testExe):
                sys.stderr.write('fatal: %s does not exist\n' % testExe)
                sys.exit(1)

            testArgs.append((catchsegvExe, testExe, testSrc))

    for testName, testResult in pool.imap_unordered(test, testArgs):
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
