PACKAGE = DrMingw
VERSION = 0.4.3

bin_PROGRAMS = drmingw

#lib_LTLIBRARIES = libexchndl.la

drmingw_SOURCES = \
	debugger.c \
	debugger.h \
	debugx.c \
	dialog.c \
	dialog.h \
	log.c \
	log.h \
	main.c \
	misc.c \
	misc.h \
	module.c \
	module.h \
	prdbg.c \
	resource.h \
	symbols.c \
	symbols.h \
	ieee.c \
	ieee.h \
	rdcoff.c \
	rddbg.c \
	stabs.c \
	budbg.h \
	debug.h \
	debug.c \
	include/aout/stab_gnu.h \
	include/aout/aout64.h \
	include/aout/stab.def \
	include/coff/internal.h \
	include/demangle.h \
	include/dis-asm.h \
	include/getopt.h \
	include/libcoff.h \
	include/libiberty.h

dist_EXTRA_drmingw_SOURCES = resource.rc icon.ico

INCLUDES = -I$(srcdir)/include

drmingw_LDFLAGS = -mwindows

drmingw_LDADD = resource.o -lbfd -liberty

#libexchndl_la_SOURCES = exchndl.c
#libexchndl_la_LDFLAGS = -no-undefined -version-info 0:0:0

RC = @WINDRES@
RCFLAGS = --use-temp-file --verbose -O COFF
resource.o: resource.rc resource.h icon.ico
	$(RC) $(RCFLAGS) -o $@ --include-dir=$(srcdir) $<

