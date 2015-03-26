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

sample.exe: sample.cpp exchndl2.cpp

%.exe: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.exe: %.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

clean:
	-$(RM) sample.exe

.PHONE: all clean
