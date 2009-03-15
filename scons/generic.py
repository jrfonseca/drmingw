"""generic

Generic tool that provides a commmon ground for all platforms.

"""

#
# Copyright 2008 Tungsten Graphics, Inc., Cedar Park, Texas.
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
# The above copyright notice and this permission notice (including the
# next paragraph) shall be included in all copies or substantial portions
# of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
# IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
# ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#


import os
import os.path
import re
import platform as _platform
import sys

import SCons.Action
import SCons.Builder
import SCons.Scanner


_machine_map = {
	'x86': 'x86',
	'i386': 'x86',
	'i486': 'x86',
	'i586': 'x86',
	'i686': 'x86',
	'ppc': 'ppc',
	'x86_64': 'x86_64',
}


_bool_map = {
    'y': 1, 
    'yes': 1,
    't': 1, 
    'true': 1, 
    '1': 1,
    'on': 1,
    'all': 1, 
    'n': 0, 
    'no': 0, 
    'f': 0, 
    'false': 0, 
    '0': 0,
    'off': 0,
    'none': 0,
}


def num_jobs():
    try:
        return int(os.environ['NUMBER_OF_PROCESSORS'])
    except (ValueError, KeyError):
        pass

    try:
        return os.sysconf('SC_NPROCESSORS_ONLN')
    except (ValueError, OSError, AttributeError):
        pass

    try:
        return int(os.popen2("sysctl -n hw.ncpu")[1].read())
    except ValueError:
        pass

    return 1


def generate(env):
    """Common environment generation code"""

    from SCons.Script import ARGUMENTS

    # Machine
    try:
        env['machine'] = ARGUMENTS['machine']
    except KeyError:
        env['machine'] = _machine_map.get(os.environ.get('PROCESSOR_ARCHITECTURE', _platform.machine()), 'generic')

    # Toolchain
    try:
        env['toolchain'] = ARGUMENTS['toolchain']
    except KeyError:
        if sys.platform != 'win32':
            env['toolchain'] = 'crossmingw'
        else:
            env['toolchain'] = 'default'
    if env['toolchain'] == 'crossmingw' and env['machine'] not in ('generic', 'x86'):
            env['machine'] = 'x86'

    # Build type
    env['debug'] = _bool_map[ARGUMENTS.get('debug', 'no')]
    env['profile'] = _bool_map[ARGUMENTS.get('profile', 'no')]

    # Put build output in a separate dir, which depends on the current
    # configuration. See also http://www.scons.org/wiki/AdvancedBuildExample
    try:
        env['variant_dir'] = ARGUMENTS['variant_dir']
    except KeyError:
        build_topdir = 'build'
        build_subdir = env['toolchain']
        if env['machine'] != 'generic':
            build_subdir += '-' + env['machine']
        if env['debug']:
            build_subdir += "-debug"
        if env['profile']:
            build_subdir += "-profile"
        env['variant_dir'] = os.path.join(build_topdir, build_subdir)
    # Place the .sconsign file in the build dir too, to avoid issues with
    # different scons versions building the same source file
    #env.VariantDir(env['variant_dir']
    #env.SConsignFile(os.path.join(env['variant_dir'], '.sconsign'))

    # Parallel build
    if env.GetOption('num_jobs') <= 1:
        env.SetOption('num_jobs', num_jobs())

    # Summary
    print
    print '  toolchain=%s' % env['toolchain']
    print '  machine=%s' % env['machine']
    print '  debug=%s' % ['no', 'yes'][env['debug']]
    print '  profile=%s' % ['no', 'yes'][env['profile']]
    #print '  variant_dir=%s' % env['variant_dir']
    print

    # Load tool chain
    env.Tool(env['toolchain'])

    # shortcuts
    debug = env['debug']
    machine = env['machine']
    x86 = env['machine'] == 'x86'
    ppc = env['machine'] == 'ppc'
    gcc = 'gcc' in env['CC'].split('-')
    msvc = 'cl' in env['CC']

    # C preprocessor options
    cppdefines = []
    if debug:
        cppdefines += ['DEBUG']
    else:
        cppdefines += ['NDEBUG']
    if env['profile']:
        cppdefines += ['PROFILE']
    cppdefines += [
        'WIN32',
        '_WINDOWS',
        #'_UNICODE',
        #'UNICODE',
        ('_WIN32_WINNT', '0x0501'), # minimum required OS version
        ('WINVER', '0x0501'),
    ]
    if msvc:
        cppdefines += [
            # http://msdn2.microsoft.com/en-us/library/6dwk3a1z.aspx,
            'VC_EXTRALEAN',
            '_CRT_SECURE_NO_DEPRECATE',
        ]
    if debug:
        cppdefines += ['_DEBUG']
    env.Append(CPPDEFINES = cppdefines)

    # C compiler options
    cflags = [] # C
    cxxflags = [] # C++
    ccflags = [] # C & C++
    if gcc:
        if debug:
            ccflags += ['-O0', '-g3']
        else:
            ccflags += ['-O3', '-g0']
        if env['profile']:
            ccflags += ['-pg']
        if env['machine'] == 'x86':
            ccflags += [
                '-m32',
            ]
        if env['machine'] == 'x86_64':
            ccflags += ['-m64']
        ccflags += [
            '-Wall',
            '-Wno-long-long',
            '-fmessage-length=0', # be nice to Eclipse
        ]
        cflags += [
            '-Wmissing-prototypes',
        ]
    if msvc:
        # See also:
        # - http://msdn.microsoft.com/en-us/library/19z1t1wy.aspx
        # - cl /?
        if debug:
            ccflags += [
              '/Od', # disable optimizations
              '/Oi', # enable intrinsic functions
              '/Oy-', # disable frame pointer omission
            ]
        else:
            ccflags += [
              '/Ox', # maximum optimizations
              '/Oi', # enable intrinsic functions
              '/Ot', # favor code speed
              #'/fp:fast', # fast floating point 
            ]
        if env['profile']:
            ccflags += [
                '/Gh', # enable _penter hook function
                '/GH', # enable _pexit hook function
            ]
        ccflags += [
            '/W3', # warning level
            #'/Wp64', # enable 64 bit porting warnings
        ]
        if env['machine'] == 'x86':
            ccflags += [
                #'/QIfist', # Suppress _ftol
                #'/arch:SSE2', # use the SSE2 instructions
            ]
        # Automatic pdb generation
        # See http://scons.tigris.org/issues/show_bug.cgi?id=1656
        env.EnsureSConsVersion(0, 98, 0)
        env['PDB'] = '${TARGET.base}.pdb'
    env.Append(CCFLAGS = ccflags)
    env.Append(CFLAGS = cflags)
    env.Append(CXXFLAGS = cxxflags)

    if msvc:
        # Choose the appropriate MSVC CRT
        # http://msdn.microsoft.com/en-us/library/2kzt1wy3.aspx
        if env['debug']:
            env.Append(CCFLAGS = ['/MTd'])
            env.Append(SHCCFLAGS = ['/LDd'])
        else:
            env.Append(CCFLAGS = ['/MT'])
            env.Append(SHCCFLAGS = ['/LD'])
    
    # Assembler options
    if gcc:
        if env['machine'] == 'x86':
            env.Append(ASFLAGS = ['-m32'])
        if env['machine'] == 'x86_64':
            env.Append(ASFLAGS = ['-m64'])

    # Linker options
    linkflags = []
    if gcc:
        if env['machine'] == 'x86':
            linkflags += ['-m32']
        if env['machine'] == 'x86_64':
            linkflags += ['-m64']
    if msvc:
        # See also:
        # - http://msdn2.microsoft.com/en-us/library/y0zzbyt4.aspx
        if env['profile']:
            linkflags += [
                '/MAP', # http://msdn.microsoft.com/en-us/library/k7xkk3e2.aspx
            ]
    env.Append(LINKFLAGS = linkflags)

    # Default libs
    env.Append(LIBS = [])

    # for debugging
    #print env.Dump()


def exists(env):
    return 1
