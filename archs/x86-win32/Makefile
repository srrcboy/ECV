include ../../common.mk

CROSS=i686-w64-mingw32-
CC=$(CROSS)gcc
CXX=$(CROSS)g++
AR=$(CROSS)ar
LD=$(CROSS)ld


ARCH_FLAGS= -msse -msse2
ARCH_OBJS=arch/yvv.o \
	  arch/sleep.o \
	  arch/system.o
OBJS += $(ARCH_OBJS)

%.o: ../../%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -c -o $@

gl/%.o: ../../gl/%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -c -o $@

arch/%.o: arch/%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -c -o $@


CFLAGS=-g -Wall -O2 -finline-functions $(ARCH_FLAGS)

CXXFLAGS=$(CFLAGS)

all: libecv.a

libecv.a: $(OBJS)
	$(AR) rvs $@ $^


SOURCES= ../../*.cpp ../../*.hpp arch/*.cpp

depend: .deps	

.deps: $(SOURCES)
	$(RM) .deps
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MM -MG -MP $(SOURCES) > .deps

clean:
	$(RM) $(OBJS) libecv.a

-include .deps

