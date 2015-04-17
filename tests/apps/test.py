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



if sys.platform == 'win32':
    nullDevName = 'NUL:'
else:
    nullDevName = '/dev/null'


verbose = True


def main():
    args = sys.argv[1:]
    if len(args) > 2:
        sys.stderr.write('usage: %s [path/to/catchsegv.exe] [path/to/test/apps]\n' % sys.argv[0])
        sys.exit(1)

    if len(args) >= 1:
        catchsegvExe = args[0]
    else:
        catchsegvExe = os.path.join('bin', 'catchsegv.exe')
    if not os.path.isfile(catchsegvExe):
        sys.stderr.write('error: %s does not exist\n' % catchsegvExe)
        sys.exit(1)

    if len(args) >= 2:
        testsDir = args[1]
    else:
        testsDir = os.path.normpath(os.path.join(os.path.dirname(catchsegvExe), '..', 'tests', 'apps'))
    if not os.path.isdir(testsDir):
        sys.stderr.write('error: %s does not exist\n' % testsDir)
        sys.exit(1)

    for testName in os.listdir(testsDir):
        if not testName.endswith('.exe'):
            continue

        testExe = os.path.join(testsDir, testName)

        sys.stdout.write("-"*78 + "\n")
        sys.stdout.write("%s\n" % testName)

        cmd = [catchsegvExe, '-t', '5', testExe]

        if sys.platform != 'win32':
            cmd = ['wine'] + cmd
            os.environ['WINEDEBUG'] = '+debugstr'

        if verbose:
            stdout = None
            stderr = None
        else:
            stdout = open(nullDevName, 'w')
            stderr = stdout

        sys.stdout.write(' '.join(cmd) + '\n')
        sys.stdout.flush()

        retcode = subprocess.call(cmd, stdout=stdout, stderr=stderr)

        sys.stdout.write('%i\n' % retcode)
        sys.stdout.flush()


if __name__ == '__main__':
    main()
