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

doc = \
	doc/drmingw.html \
	doc/drmingw.reg \
	doc/exception-nt.gif \
	doc/install.gif \
	doc/sample.gif

EXTRA_DIST = exchndl.c $(doc)

all: exchndl.dll

exchndl.dll: exchndl.c
	$(COMPILE) -shared -o $@ $< -lbfd -liberty

CLEANFILES = exchndl.dll

noinst_PROGRAMS = test testcpp

test_SOURCES = samples/test.c samples/exchndl2.cxx
test_CFLAGS = -g $(AM_CFLAGS)

testcpp_SOURCES = samples/test.c samples/exchndl2.cxx
testcpp_CXXFLAGS = -g $(AM_CXXFLAGS)

install-exec-local: exchndl.dll test$(EXEEXT) testcpp$(EXEEXT)
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	$(INSTALL_PROGRAM_ENV) $(INSTALL_PROGRAM) exchndl.dll $(DESTDIR)$(bindir)/exchndl.dll
	$(mkinstalldirs) $(DESTDIR)$(prefix)/doc/drmingw
	for f in $(doc); do \
	  $(INSTALL_DATA) $(srcdir)/$$f $(DESTDIR)$(prefix)/doc/drmingw/`basename $$f`; \
	done
	$(mkinstalldirs) $(DESTDIR)$(prefix)/doc/drmingw/samples
	$(INSTALL_DATA) samples/exchndl2.cxx $(DESTDIR)$(prefix)/doc/drmingw/samples/exchndl2.cxx
	$(INSTALL_DATA) samples/test.c $(DESTDIR)$(prefix)/doc/drmingw/samples/test.c
	$(INSTALL_DATA) samples/testcpp.cxx $(DESTDIR)$(prefix)/doc/drmingw/samples/testcpp.cxx
	$(INSTALL_PROGRAM) test$(EXEEXT) $(DESTDIR)$(prefix)/doc/drmingw/samples/test$(EXEEXT)
	$(INSTALL_PROGRAM) testcpp$(EXEEXT) $(DESTDIR)$(prefix)/doc/drmingw/samples/testcpp$(EXEEXT)
