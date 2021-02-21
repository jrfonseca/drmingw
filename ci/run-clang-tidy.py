#!/usr/bin/env python3
###########################################################################
#
# Copyright 2021 Jose Fonseca
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


import json
import os.path
import subprocess
import sys
import threading

import multiprocessing.dummy as multiprocessing
from multiprocessing import cpu_count

try:
    build_dir = sys.argv[1]
except IndexError:
    build_dir = os.path.curdir


# Set up Clang compiler flags to use MinGW runtime
# http://stackoverflow.com/a/19839946
extra_args = []
p = subprocess.Popen(
        ["x86_64-w64-mingw32-g++", "-x", "c", "-E", "-Wp,-v", '-'],
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        universal_newlines=True)
for line in p.stderr:
    if line.startswith(' '):
        include = line.strip()
        if os.path.isdir(include):
            if os.path.exists(os.path.join(include, 'ia32intrin.h')):
                # XXX: We must use Clang's intrinsic headers
                continue
            extra_args.append('-I' + os.path.normpath(include))
extra_args += [
    '-D__MINGW32__',
    '-D_WIN32',
    '-D_WIN64',
]
extra_args += [
    '--target=x86_64-pc-mingw32',
]

paths = []
for command in json.load(open(os.path.join(build_dir, 'compile_commands.json'), 'rt')):
    path = command['file']
    if path.endswith('.rc'):
        continue
    path = os.path.relpath(path, os.curdir)
    # TODO: Control this via regex option
    if 'thirdparty' in path.split(os.path.sep):
        continue
    paths.append(path)

cmd = ['clang-tidy']
cmd.append('--quiet')
cmd.append('-p=' + build_dir)
for extra_arg in extra_args:
    cmd.append('--extra-arg=' + extra_arg)


cmds = []
for path in paths:
    cmds.append(cmd + [path])


mutex = threading.Lock()

def call(cmd):
    p = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True)
    stdout, stderr = p.communicate()
    if stdout or stderr:
        with mutex:
            sys.stdout.write(' '.join(cmd) + '\n')
            sys.stdout.write(stderr)
            sys.stdout.write(stdout)
            sys.stdout.flush()
    return p.returncode == 0


numJobs = cpu_count()
pool = multiprocessing.Pool(numJobs)
if numJobs <= 1 or len(cmds) <= 1:
    imap = map
else:
    imap = pool.imap_unordered
for result in imap(call, cmds):
    pass
