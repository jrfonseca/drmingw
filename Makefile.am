PACKAGE = DrMingw
VERSION = 0.4.3

bin_PROGRAMS = drmingw

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

dist_EXTRA_drmingw_SOURCES = \
	resource.rc \
	icon.ico

INCLUDES = -I$(srcdir)/include

drmingw_LDFLAGS = -mwindows
drmingw_LDADD = resource.o -lbfd -liberty

RC = @WINDRES@
RCFLAGS = --use-temp-file --verbose -O COFF
resource.o: resource.rc resource.h icon.ico
	$(RC) $(RCFLAGS) -o $@ --include-dir=$(srcdir) $<


all: exchndl.dll

EXTRA_DIST = exchndl.c

exchndl.dll: exchndl.c
	$(COMPILE) -shared -s -o $@ $< -lbfd -liberty

CLEANFILES = exchndl.dll

install-exec-local: exchndl.dll
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	$(INSTALL_PROGRAM_ENV) $(INSTALL_PROGRAM) exchndl.dll $(DESTDIR)$(bindir)/exchndl.dll
	

docdir = $(prefix)/doc/drmingw

dist_doc_DATA = COPYING COPYING.LIB doc/drmingw.html doc/drmingw.reg doc/exception-nt.gif doc/install.gif doc/sample.gif


noinst_LIBRARIES = libexchndl2.a

libexchndl2_a_SOURCES = samples/exchndl2.cxx

samplesdir = $(docdir)/samples

samples_PROGRAMS = test testcpp
samples_DATA = samples/exchndl2.cxx samples/test.c samples/testcpp.cxx

test_SOURCES = samples/test.c
test_LDADD = exchndl2.o -lstdc++
test_CFLAGS = -ggdb $(AM_CFLAGS)

testcpp_SOURCES = samples/testcpp.cxx
testcpp_LDADD = exchndl2.o -lstdc++
testcpp_CXXFLAGS = -ggdb $(AM_CXXFLAGS)

