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

LDFLAGS = -static-libgcc -static-libstdc++


all: sample.exe

sample.exe: sample.cxx exchndl2.cxx

%.exe: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.exe: %.cxx
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

clean:
	-$(RM) sample.exe

.PHONE: all clean
