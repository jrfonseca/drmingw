ifeq ($(OS),Windows_NT)
	PREFIX ?=
	RM = del
else
	PREFIX ?= i686-w64-mingw32-
endif


CC = $(PREFIX)gcc
CXX = $(PREFIX)g++

CFLAGS = -ggdb3
CXXFLAGS = $(CFLAGS)

all: sample.exe

sample.exe: sample.cxx exchndl2.cxx

%.exe: %.c
	$(CC) $(CFLAGS) -o $@ $^

%.exe: %.cxx
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	-$(RM) sample.exe

.PHONE: all clean
