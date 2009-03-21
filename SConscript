Import('env')

env = env.Clone()

env.Append(CPPPATH = [
    'include',
])

env.Append(LINKFLAGS = [
    '-mwindows',
])

env.Prepend(LIBS = [
    'bfd',
    'iberty',
])

env.Program(
    target = 'drmingw',
    source = [
	    'debugger.c',
	    'debugx.c',
	    'dialog.c',
	    'log.c',
	    'main.c',
	    'misc.c',
	    'module.c',
	    'symbols.c',
	    'ieee.c',
	    'rdcoff.c',
	    'rddbg.c',
	    'stabs.c',
	    'debug.c',
        env.RES('resource.rc'),
    ],
)

env.SharedLibrary(
    target = 'exchndl',
    source = ['exchndl.c']
)


env.Tool('filesystem')
env.Tool('zip')
env.Tool('packaging')

zip = env.Package(
    NAME           = 'drmingw',
    VERSION        = '0.4.4',
    PACKAGEVERSION = 0,
    PACKAGETYPE    = 'zip',
    LICENSE        = 'lgpl',
    SUMMARY        = 'Just-in-time debugger for MinGW',
    SOURCE_URL     = 'http://code.google.com/p/jrfonseca/wiki/DrMingw',
    source = [
        'COPYING',
        'COPYING.LIB',
        'drmingw.exe',
        'doc/drmingw.html',
        'doc/drmingw.reg',
        'doc/exception-nt.gif',
        'doc/install.gif',
        'doc/sample.gif',
        'samples/test.c',
        'samples/test.exe',
        'samples/testcpp.cxx',
        'samples/testcpp.exe',
    ],
)

env.Alias('zip', zip)
