#!/usr/bin/env python3
#
# Copyright 2026 Jose Fonseca
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#


'''Test catchsegv.exe command line options not exercised by tests/apps/test.py.

This script runs natively, while the Windows executables it spawns are run
through the given emulator (e.g., Wine) when cross-compiling.
'''


import argparse
import os
import re
import subprocess
import sys
import tempfile


EXCEPTION_ACCESS_VIOLATION = 0xC0000005

TIMEOUT = 120


class Result:

    def __init__(self, exitCode, stdout, stderr):
        self.exitCode = exitCode
        self.stdout = stdout
        self.stderr = stderr


class Tester:

    def __init__(self, options):
        self.emulator = [arg for arg in options.emulator.split(';') if arg]
        self.catchsegvExe = os.path.abspath(options.catchsegv)
        self.app = os.path.abspath(options.app)
        self.dmp = os.path.abspath(options.dmp)
        self.verbose = options.verbose
        self.testNo = 0
        self.failures = 0

    def ok(self, condition, description):
        self.testNo += 1
        if condition:
            sys.stdout.write('ok %u - %s\n' % (self.testNo, description))
        else:
            self.failures += 1
            sys.stdout.write('not ok %u - %s\n' % (self.testNo, description))
        sys.stdout.flush()
        return condition

    def diagnostic(self, text):
        for line in text.splitlines():
            sys.stdout.write('# %s\n' % line)
        sys.stdout.flush()

    def run(self, args, cwd):
        cmd = self.emulator + args
        sys.stdout.write('# %s\n' % ' '.join(cmd))
        sys.stdout.flush()
        try:
            p = subprocess.run(cmd, cwd=cwd, stdin=subprocess.DEVNULL, capture_output=True,
                               timeout=TIMEOUT)
        except subprocess.TimeoutExpired:
            return Result(None, '', '')

        stdout = p.stdout.replace(b'\r\n', b'\n').decode('utf-8', errors='replace')
        stderr = p.stderr.replace(b'\r\n', b'\n').decode('utf-8', errors='replace')

        exitCode = p.returncode
        if exitCode < 0:
            exitCode += 1 << 32

        result = Result(exitCode, stdout, stderr)
        if self.verbose:
            self.diagnostic(stdout)
            self.diagnostic(stderr)
        return result

    def catchsegv(self, options, appArgs, cwd):
        return self.run([self.catchsegvExe] + options + ['--', self.app] + appArgs, cwd)

    def checkExitCode(self, result, expected, description):
        exitCode = result.exitCode
        if exitCode is not None and sys.platform != 'win32':
            # Only the low byte survives on Unix
            expected %= 256
        ok = self.ok(exitCode == expected, '%s: exit code' % description)
        if not ok:
            if exitCode is None:
                self.diagnostic('timed out')
            else:
                self.diagnostic('exit code was 0x%08x, expected 0x%08x' % (exitCode, expected))
            self.diagnostic(result.stdout)
            self.diagnostic(result.stderr)
        return ok

    def checkLine(self, result, pattern, description, stream='stderr', present=True):
        text = getattr(result, stream)
        found = re.search(pattern, text, re.MULTILINE) is not None
        verb = 'has' if present else 'lacks'
        ok = self.ok(found == present, '%s: %s %s /%s/' % (description, stream, verb, pattern))
        if not ok and not self.verbose:
            self.diagnostic(text)
        return ok

    def parseValues(self, text):
        values = {}
        for line in text.splitlines():
            name, sep, value = line.partition('=')
            if sep:
                values[name.strip()] = value.strip()
        return values

    def testUsage(self, cwd):
        for option in ('-h', '-?'):
            description = 'catchsegv %s' % option
            result = self.run([self.catchsegvExe, option], cwd)
            self.checkExitCode(result, 0, description)
            self.checkLine(result, r'^usage: catchsegv ', description)

    def testNoCommandLine(self, cwd):
        description = 'catchsegv without command line'
        result = self.run([self.catchsegvExe], cwd)
        self.checkExitCode(result, 1, description)
        self.checkLine(result, r'^catchsegv: error: no command line given$', description)
        self.checkLine(result, r'^usage: catchsegv ', description)

    def testCreateProcessFailure(self, cwd):
        description = 'catchsegv with non-existent program'
        result = self.run([self.catchsegvExe, '--', 'test_catchsegv_does_not_exist.exe'], cwd)
        self.checkExitCode(result, 1, description)
        self.checkLine(result, r'^catchsegv: error: failed to create the process ', description)

    def testArgumentQuoting(self, cwd):
        description = 'argument quoting'
        args = [
            'plain',
            'with space',
            'with\ttab',
            'with"quote',
            'back\\slash',
            'back\\"slash quote',
        ]
        result = self.catchsegv([], ['args'] + args, cwd)
        self.checkExitCode(result, 0, description)
        echoed = re.findall(r'^arg=<(.*)>$', result.stdout, re.MULTILINE)
        ok = self.ok(echoed == args, description)
        if not ok:
            self.diagnostic('expected %r' % args)
            self.diagnostic('got      %r' % echoed)

    def testVerbose(self, cwd):
        description = 'catchsegv -v'
        result = self.catchsegv(['-v'], ['crash'], cwd)
        self.checkExitCode(result, EXCEPTION_ACCESS_VIOLATION, description)
        for pattern in (
            r'^CREATE_PROCESS PID=\d+ TID=\d+ ',
            r'^LOAD_DLL PID=\d+ TID=\d+ ',
            r'^EXCEPTION PID=\d+ TID=\d+ ExceptionCode=0xc0000005 dwFirstChance=0$',
            r'^EXIT_PROCESS PID=\d+ TID=\d+ ',
        ):
            self.checkLine(result, pattern, description)

    def testDebug(self, cwd):
        description = 'catchsegv -d'
        result = self.catchsegv(['-d'], ['crash'], cwd)
        self.checkExitCode(result, EXCEPTION_ACCESS_VIOLATION, description)
        self.checkLine(result, r'!FaultingFunction\+0x', description)

    def testFirstChance(self, cwd):
        pattern = r'!RaiseFirstChanceException\+0x'

        description = 'catchsegv without -1'
        result = self.catchsegv([], ['first_chance'], cwd)
        self.checkExitCode(result, 0, description)
        self.checkLine(result, pattern, description, present=False)

        description = 'catchsegv -1'
        result = self.catchsegv(['-1'], ['first_chance'], cwd)
        self.checkExitCode(result, 0, description)
        self.checkLine(result, r'^test_catchsegv_app\.exe caused an Unknown \[0xE0000001\] Exception',
                       description)
        self.checkLine(result, pattern, description)
        self.checkLine(result, r'^survived first chance exception$', description, stream='stdout')

    def testQuiet(self, cwd):
        pattern = r'^test_catchsegv_app: OutputDebugString marker$'

        description = 'catchsegv without -q'
        result = self.catchsegv([], ['output_debug_string'], cwd)
        self.checkExitCode(result, 0, description)
        self.checkLine(result, pattern, description)

        description = 'catchsegv -q'
        result = self.catchsegv(['-q'], ['output_debug_string'], cwd)
        self.checkExitCode(result, 0, description)
        self.checkLine(result, pattern, description, present=False)

    def testDebugHeap(self, cwd):
        description = 'catchsegv without -H'
        result = self.catchsegv([], ['env'], cwd)
        self.checkExitCode(result, 0, description)
        self.checkLine(result, r'^_NO_DEBUG_HEAP=1$', description, stream='stdout')

        description = 'catchsegv -H'
        result = self.catchsegv(['-H'], ['env'], cwd)
        self.checkExitCode(result, 0, description)
        self.checkLine(result, r'^_NO_DEBUG_HEAP=\(unset\)$', description, stream='stdout')

    def checkMinidump(self, result, dumpDir, cwd, description):
        if not self.checkExitCode(result, EXCEPTION_ACCESS_VIOLATION, description):
            return

        app = self.parseValues(result.stdout)
        if not self.ok('pid' in app and 'tid' in app and 'func' in app,
                       '%s: app printed its IDs' % description):
            return

        dumpName = app['pid'] + '.dmp'
        dumpPath = os.path.join(dumpDir, dumpName)
        self.checkLine(result, r'^info: minidump written to .*%s$' % re.escape(dumpName),
                       description)
        if not self.ok(os.path.isfile(dumpPath), '%s: %s exists' % (description, dumpPath)):
            return

        dmpResult = self.run([self.dmp, dumpPath], cwd)
        if not self.checkExitCode(dmpResult, 0, '%s: test_catchsegv_dmp' % description):
            return

        dmp = self.parseValues(dmpResult.stdout)
        self.diagnostic(dmpResult.stdout)

        self.ok(dmp.get('ExceptionCode') == '0x%08x' % EXCEPTION_ACCESS_VIOLATION,
                '%s: minidump exception code' % description)
        self.ok(dmp.get('ThreadId') == app['tid'],
                '%s: minidump exception thread' % description)

        func = int(app['func'], 16)
        address = int(dmp.get('ExceptionAddress', '0'), 16)
        self.ok(func <= address < func + 0x100,
                '%s: minidump exception address inside FaultingFunction' % description)

        pc = int(dmp.get('ContextPC', '0'), 16)
        self.ok(pc == address,
                '%s: minidump faulting thread context matches exception address' % description)

        self.ok(dmp.get('Comment') == 'Dump generated with DrMingw',
                '%s: minidump comment' % description)

    def testMinidump(self, cwd):
        description = 'catchsegv -z'
        result = self.catchsegv(['-z'], ['crash'], cwd)
        self.checkMinidump(result, cwd, cwd, description)

    def testMinidumpDir(self, cwd):
        description = 'catchsegv -Z'
        dumpDir = os.path.join(cwd, 'dumps')
        os.mkdir(dumpDir)
        # Use a relative path, as Wine doesn't understand Unix absolute paths
        result = self.catchsegv(['-Z', 'dumps'], ['crash'], cwd)
        self.checkMinidump(result, dumpDir, cwd, description)

    def testMinidumpDirMissing(self, cwd):
        description = 'catchsegv -Z with non-existent directory'
        result = self.catchsegv(['-Z', 'missing'], ['crash'], cwd)
        self.checkExitCode(result, EXCEPTION_ACCESS_VIOLATION, description)
        self.checkLine(result, r'^error: failed to create minidump file ', description)
        self.checkLine(result, r'!FaultingFunction\+0x', description)

    def checkChildExited(self, result, cwd, description):
        app = self.parseValues(result.stdout)
        if not self.ok('child_pid' in app, '%s: app printed child process ID' % description):
            return
        # Process IDs are only meaningful while the emulator's server (e.g.,
        # wineserver) is alive, which spawning another process ensures.
        check = self.run([self.app, 'check_pid', app['child_pid']], cwd)
        self.ok(check.stdout.strip() == 'exited',
                '%s: child process %s exited' % (description, app['child_pid']))

    def testTimeOutChild(self, cwd):
        # The debuggee and its child both run forever, so both must be
        # terminated on time out.
        description = 'catchsegv -t with child process'
        result = self.catchsegv(['-t', '3'], ['spawn_child', 'sleep'], cwd)
        self.checkExitCode(result, 3, description)
        self.checkLine(result, r'^catchsegv: time out \(3 sec\) exceeded$', description)
        self.checkChildExited(result, cwd, description)

    def testTimeOutOrphan(self, cwd):
        # The debuggee exits, but its child runs forever, and is still being
        # debugged, so it must be terminated on time out.
        description = 'catchsegv -t with orphaned child process'
        result = self.catchsegv(['-t', '3'], ['spawn_child', 'exit'], cwd)
        self.checkExitCode(result, 0, description)
        self.checkLine(result, r'^catchsegv: time out \(3 sec\) exceeded$', description)
        self.checkChildExited(result, cwd, description)

    def startWineDesktop(self, cwd):
        '''Keep the emulator's server and desktop running for the session.

        Wine starts its server, along with its services, on demand, and stops
        it shortly after the last process exits.  When the server is started
        by catchsegv itself, Wine's services messages can interleave with
        catchsegv's output, so start it beforehand.

        Wine also starts its desktop process (explorer.exe) on demand, as a
        child of the first process to create a window.  Were that the
        debuggee, catchsegv would debug it too, and terminate it on time out,
        disrupting any other process using the desktop meanwhile (e.g., tests
        running in parallel.)

        So create a window from a process outside catchsegv, and keep it
        alive until all tests are done.
        '''
        # Not needed natively, nor on WSL, where the emulator merely forwards
        # to Windows, and killing it would not kill the Windows process.
        if not self.emulator or 'WSL_DISTRO_NAME' in os.environ:
            return None
        cmd = self.emulator + [self.app, 'window']
        sys.stdout.write('# %s\n' % ' '.join(cmd))
        sys.stdout.flush()
        p = subprocess.Popen(cmd, cwd=cwd, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                             stderr=subprocess.DEVNULL)
        line = p.stdout.readline()
        self.ok(line.strip() == b'ready', 'Wine desktop started')
        return p

    def testModalDialogIgnore(self, cwd):
        description = 'catchsegv -m'
        result = self.catchsegv(['-m', '-t', '3'], ['message_box'], cwd)
        self.checkExitCode(result, 3, description)
        self.checkLine(result, r'^catchsegv: time out \(3 sec\) exceeded$', description)
        self.checkLine(result, r'message dialog detected', description, present=False)

    def runAll(self):
        tests = [
            self.testUsage,
            self.testNoCommandLine,
            self.testCreateProcessFailure,
            self.testArgumentQuoting,
            self.testVerbose,
            self.testDebug,
            self.testFirstChance,
            self.testQuiet,
            self.testDebugHeap,
            self.testMinidump,
            self.testMinidumpDir,
            self.testMinidumpDirMissing,
            self.testModalDialogIgnore,
            self.testTimeOutChild,
            self.testTimeOutOrphan,
        ]
        desktop = self.startWineDesktop(os.path.dirname(self.app))
        try:
            for test in tests:
                # Run each test in a fresh directory, where minidumps get written
                with tempfile.TemporaryDirectory(prefix='test_catchsegv_') as cwd:
                    test(cwd)
        finally:
            if desktop is not None:
                desktop.kill()
                desktop.wait()

        sys.stdout.write('1..%u\n' % self.testNo)
        sys.stdout.flush()
        return self.failures == 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emulator', default='',
                        help='semicolon separated command to run Windows executables with')
    parser.add_argument('-v', '--verbose', action='store_true')
    parser.add_argument('catchsegv', help='path to catchsegv.exe')
    parser.add_argument('app', help='path to test_catchsegv_app.exe')
    parser.add_argument('dmp', help='path to test_catchsegv_dmp.exe')
    options = parser.parse_args()

    if sys.platform == 'win32':
        import ctypes
        SEM_FAILCRITICALERRORS = 0x0001
        SEM_NOGPFAULTERRORBOX = 0x0002
        SEM_NOOPENFILEERRORBOX = 0x8000
        ctypes.windll.kernel32.SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
                                            SEM_NOOPENFILEERRORBOX)

    tester = Tester(options)
    sys.exit(0 if tester.runAll() else 1)


if __name__ == '__main__':
    main()
