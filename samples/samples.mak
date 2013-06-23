ifeq ($(OS),Windows_NT)
	PREFIX ?=
else
	PREFIX ?= i686-w64-mingw32-
endif


CC = $(PREFIX)gcc
CXX = $(PREFIX)g++

CFLAGS = -ggdb3
CXXFLAGS = $(CFLAGS)

all: test.exe testcpp.exe

test.exe: test.c exchndl2.cxx

testcpp.exe: testcpp.cxx exchndl2.cxx

%.exe: %.c
	$(CC) $(CFLAGS) -o $@ $^

%.exe: %.cxx
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	$(RM) test.exe testcpp.exe

.PHONE: all clean
