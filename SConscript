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
