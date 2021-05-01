CXX=clang++
CC=clang++
WARNINGS=-Wall -Werror -Wno-unused-parameter
CXXFLAGS=$(WARNINGS) -std=c++17 -g -flto -O2 -ffast-math $(INCLUDES)
DBGFLAGS=$(WARNINGS) -std=c++17 -g -flto -DSAT_DEBUG_MODE $(INCLUDES)
AFL=afl-clang-fast++
SHELL=/bin/bash

SOURCEDIR=../src
BUILDDIR=.

all: sat

# This was a big help: https://stackoverflow.com/questions/16924333/makefile-compiling-from-directory-to-another-directory
# The driver library is not something we'll build with different flags. Well ok, hit a complication with flto
# This is the "release" mode, basically. Note -DREL flag, it removes checks.
SOLVERSOURCES=$(wildcard $(SOURCEDIR)/*.cpp)
SOLVEROBJECTS=$(patsubst $(SOURCEDIR)/%.cpp,$(BUILDDIR)/%.o,$(SOLVERSOURCES))
$(SOLVEROBJECTS) : $(BUILDDIR)/%.o : $(SOURCEDIR)/%.cpp
	$(CC) $(CXXFLAGS) -DREL -c $< -o $@
$(BUILDDIR)/sat : $(SOLVEROBJECTS)
	$(CC) $(CXXFLAGS) $^ -o $@

SOLVERSANOBJECTS=$(patsubst $(SOURCEDIR)/%.cpp,$(BUILDDIR)/%.san.o,$(SOLVERSOURCES))
$(SOLVERSANOBJECTS) : $(BUILDDIR)/%.san.o : $(SOURCEDIR)/%.cpp
	$(CC) $(CXXFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer -c $< -o $@
$(BUILDDIR)/sat.san : $(SOLVERSANOBJECTS)
	$(CC) $(CXXFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer $^ -o $@

SOLVERFUZZOBJECTS=$(patsubst $(SOURCEDIR)/%.cpp,$(BUILDDIR)/%.fuzz.o,$(SOLVERSOURCES))
$(SOLVERFUZZOBJECTS) : $(BUILDDIR)/%.fuzz.o : $(SOURCEDIR)/%.cpp
	$(CC) $(CXXFLAGS) -fsanitize=fuzzer,address -fno-omit-frame-pointer -c $< -o $@
$(BUILDDIR)/sat.fuzz : $(SOLVERFUZZOBJECTS)  $(REPROBJECTS)
	$(CC) $(CXXFLAGS) -fsanitize=fuzzer,address -fno-omit-frame-pointer $^ -o $@

SOLVERAFLOBJECTS=$(patsubst $(SOURCEDIR)/%.cpp,$(BUILDDIR)/%.afl.o,$(SOLVERSOURCES))
$(SOLVERAFLOBJECTS) : $(BUILDDIR)/%.afl.o : $(SOURCEDIR)/%.cpp
	$(AFL) $(CXXFLAGS) -c $< -o $@
$(BUILDDIR)/sat.afl : $(SOLVERAFLOBJECTS)
	$(AFL) $(CXXFLAGS) $^ -o $@

SOLVERDBGOBJECTS=$(patsubst $(SOURCEDIR)/%.cpp,$(BUILDDIR)/%.dbg.o,$(SOLVERSOURCES))
$(SOLVERDBGOBJECTS) : $(BUILDDIR)/%.dbg.o : $(SOURCEDIR)/%.cpp
	$(CC) $(DBGFLAGS) -c $< -o $@
$(BUILDDIR)/sat.dbg : $(SOLVERDBGOBJECTS)
	$(CC) $(DBGFLAGS) $^ -o $@

clean:
	rm -f $(SOLVERSANOBJECTS) $(SOLVEROBJECTS) $(SOLVERAFLOBJECTS) $(SOLVERDBGOBJECTS) $(REPROBJECTS) $(DRIVEROBJECTS) sat.dbg sat.afl sat sat.san

# $(info $(VARNAME))

GENTESTS1000=$(shell seq 1000 | awk '{print $$1".1000minisat_diff"}')
GENTESTS100=$(shell seq 100 | awk '{print $$1".100minisat_diff"}')
GENTESTS10=$(shell seq 10 | awk '{print $$1".10minisat_diff"}')

tests: $(GENTESTS1000) $(GENTESTS100) $(GENTESTS10)
%.1000minisat_diff: sat.san
	python3 ./../tools/satgen.py --vars 10 -s $(basename $@) | ./sat.san --debug-max | diff - <(python3 ./../tools/satgen.py --vars 10 -s $(basename $@) | minisat | tail -n 1)
%.100minisat_diff: sat.san
	python3 ./../tools/satgen.py --vars 100 -s $(basename $@) | ./sat.san --debug-max | diff - <(python3 ./../tools/satgen.py --vars 100 -s $(basename $@) | minisat | tail -n 1)
%.10minisat_diff: sat
	python3 ./../tools/satgen.py --vars 250 -s $(basename $@) | ./sat | diff - <(python3 ./../tools/satgen.py --vars 250 -s $(basename $@) | minisat | tail -n 1)

benchmark1: sat
	echo "Minisat"
	python3 ./../tools/cnfsearch.py -lower-bound 0 -upper-bound 25 -scale 2.5 benchmark -solver minisat
	echo "My sat"
	python3 ./../tools/cnfsearch.py -lower-bound 0 -upper-bound 25 -scale 2.5 benchmark -solver "./sat"