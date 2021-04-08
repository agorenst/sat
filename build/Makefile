CXX=clang++
CC=clang++
INCLUDES=-I ./../src/repr -I ./../src/solver
CXXFLAGS=-Wall -std=c++17 -g -O2 -ffast-math $(INCLUDES)
DBGFLAGS=-Wall -std=c++17 -g -DSAT_DEBUG_MODE $(INCLUDES)
AFL=afl-clang-fast++
SHELL=/bin/bash

SOLVERDIR=../src/solver
DRIVERDIR=../src/driver
REPRDIR=../src/repr
BUILDDIR=.

# This was a big help: https://stackoverflow.com/questions/16924333/makefile-compiling-from-directory-to-another-directory
# The driver library is not something we'll build with different flags.
DRIVERSOURCES=$(wildcard $(DRIVERDIR)/*.cpp)
DRIVEROBJECTS=$(patsubst $(DRIVERDIR)/%.cpp,$(BUILDDIR)/%.o,$(DRIVERSOURCES))
$(DRIVEROBJECTS) : $(BUILDDIR)/%.o : $(DRIVERDIR)/%.cpp
	$(CC) $(CXXFLAGS) -c $< -o $@

REPRSOURCES=$(wildcard $(REPRDIR)/*.cpp)
REPROBJECTS=$(patsubst $(REPRDIR)/%.cpp,$(BUILDDIR)/%.o,$(REPRSOURCES))
$(REPROBJECTS) : $(BUILDDIR)/%.o : $(REPRDIR)/%.cpp
	$(CC) $(CXXFLAGS) -c $< -o $@

SOLVERSOURCES=$(wildcard $(SOLVERDIR)/*.cpp)
SOLVEROBJECTS=$(patsubst $(SOLVERDIR)/%.cpp,$(BUILDDIR)/%.o,$(SOLVERSOURCES))
$(SOLVEROBJECTS) : $(BUILDDIR)/%.o : $(SOLVERDIR)/%.cpp
	$(CC) $(CXXFLAGS) -c $< -o $@
$(BUILDDIR)/sat : $(SOLVEROBJECTS) $(DRIVEROBJECTS) $(REPROBJECTS)
	$(CC) $(CXXFLAGS) $^ -o $@

SOLVERSANOBJECTS=$(patsubst $(SOLVERDIR)/%.cpp,$(BUILDDIR)/%.san.o,$(SOLVERSOURCES))
$(SOLVERSANOBJECTS) : $(BUILDDIR)/%.san.o : $(SOLVERDIR)/%.cpp
	$(CC) $(CXXFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer -c $< -o $@
$(BUILDDIR)/sat.san : $(SOLVERSANOBJECTS) $(DRIVEROBJECTS)  $(REPROBJECTS)
	$(CC) $(CXXFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer $^ -o $@

SOLVERAFLOBJECTS=$(patsubst $(SOLVERDIR)/%.cpp,$(BUILDDIR)/%.afl.o,$(SOLVERSOURCES))
$(SOLVERAFLOBJECTS) : $(BUILDDIR)/%.afl.o : $(SOLVERDIR)/%.cpp
	$(AFL) $(CXXFLAGS) -c $< -o $@
$(BUILDDIR)/sat.afl : $(SOLVERAFLOBJECTS) $(DRIVEROBJECTS) $(REPROBJECTS)
	$(AFL) $(CXXFLAGS) $^ -o $@

SOLVERDBGOBJECTS=$(patsubst $(SOLVERDIR)/%.cpp,$(BUILDDIR)/%.dbg.o,$(SOLVERSOURCES))
$(SOLVERDBGOBJECTS) : $(BUILDDIR)/%.dbg.o : $(SOLVERDIR)/%.cpp
	$(CC) $(DBGFLAGS) -c $< -o $@
$(BUILDDIR)/sat.dbg : $(SOLVERDBGOBJECTS) $(DRIVEROBJECTS) $(REPROBJECTS)
	$(CC) $(DBGFLAGS) $^ -o $@

clean:
	rm -f $(SOLVERSANOBJECTS) $(SOLVEROBJECTS) $(SOLVERAFLOBJECTS) $(SOLVERDBGOBJECTS) $(DRIVEROBJECTS) sat.dbg sat.afl sat sat.san

# $(info $(VARNAME))

# This doesn't actually produce the file, but teaches us how to 
# GENTESTS=$(shell seq 1000 | awk '{print $$1".minisat_diff"}')
# tests: $(GENTESTS)
# %.minisat_diff:
# 	python3 ./../src/gen/satgen.py --vars 100 -s $(basename $@) | ./sat | diff - <(python3 ./../src/gen/satgen.py --vars 100 -s $(basename $@) | minisat | tail -n 1)

GENTESTS1000=$(shell seq 1000 | awk '{print $$1".1000minisat_diff"}')
GENTESTS100=$(shell seq 100 | awk '{print $$1".100minisat_diff"}')
GENTESTS10=$(shell seq 10 | awk '{print $$1".10minisat_diff"}')
tests: $(GENTESTS1000) $(GENTESTS100) $(GENTESTS10)
%.1000minisat_diff:
	python3 ./../src/gen/satgen.py --vars 10 -s $(basename $@) | ./sat --debug-max | diff - <(python3 ./../src/gen/satgen.py --vars 10 -s $(basename $@) | minisat | tail -n 1)
%.100minisat_diff:
	python3 ./../src/gen/satgen.py --vars 100 -s $(basename $@) | ./sat --debug-max | diff - <(python3 ./../src/gen/satgen.py --vars 100 -s $(basename $@) | minisat | tail -n 1)
%.10minisat_diff:
	python3 ./../src/gen/satgen.py --vars 250 -s $(basename $@) | ./sat | diff - <(python3 ./../src/gen/satgen.py --vars 250 -s $(basename $@) | minisat | tail -n 1)