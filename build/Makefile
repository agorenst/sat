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

# https://clang.llvm.org/docs/UsersManual.html#profile-guided-optimization
# LLVM flag version is fprofile-instr-generate, but we specify a directory here
SOLVERPGIOBJECTS=$(patsubst $(SOURCEDIR)/%.cpp,$(BUILDDIR)/%.pgi.o,$(SOLVERSOURCES))
$(SOLVERPGIOBJECTS) : $(BUILDDIR)/%.pgi.o : $(SOURCEDIR)/%.cpp
	$(CC) $(CXXFLAGS) -DREL -fprofile-instr-generate="sat-%m.profraw" -c $< -o $@
$(BUILDDIR)/sat.pgi : $(SOLVERPGIOBJECTS)
	$(CC) $(CXXFLAGS) -DREL -fprofile-instr-generate="sat-%m.profraw"  $^ -o $@

#$(CC) $(CXXFLAGS) -DREL -fprofile-generate=profdir $^ -o $@

sat.profdata: sat.pgi
	python3 ./../tools/cnfsearch.py -lower-bound 0 -upper-bound 10 -scale 3.0 benchmark -solver "./sat.pgi"
	mv *.profraw profdir/one.profraw
	python3 ./../tools/cnfsearch.py -lower-bound 0 -upper-bound 10 -scale 3.0 benchmark -solver "./sat.pgi" -flags="--only-stamped"
	mv *.profraw profdir/two.profraw
	llvm-profdata merge -output=sat.profdata ./profdir/*.profraw

#llvm-profdata merge -output=sat.profdata profdir/

SOLVERPGOOBJECTS=$(patsubst $(SOURCEDIR)/%.cpp,$(BUILDDIR)/%.pgo.o,$(SOLVERSOURCES))
$(SOLVERPGOOBJECTS) : $(BUILDDIR)/%.pgo.o : $(SOURCEDIR)/%.cpp | sat.profdata
	$(CC) $(CXXFLAGS) -DREL -fprofile-use=sat.profdata -c $< -o $@
$(BUILDDIR)/sat.pgo : $(SOLVERPGOOBJECTS) | sat.profdata
	$(CC) $(CXXFLAGS) -DREL -fprofile-use=sat.profdata $^ -o $@

clean:
	rm -f $(SOLVERPGOOBJECTS) $(SOLVERPGIOBJECTS) $(SOLVERSANOBJECTS) $(SOLVEROBJECTS) $(SOLVERAFLOBJECTS) $(SOLVERDBGOBJECTS) $(REPROBJECTS) $(DRIVEROBJECTS) sat.dbg sat.afl sat sat.san *.prof* sat.pgi sat.pgo profdir/*

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
	python3 ./../tools/cnfsearch.py -lower-bound 0 -upper-bound 10 -scale 3.0 benchmark -solver "./sat"
	python3 ./../tools/cnfsearch.py -lower-bound 0 -upper-bound 10 -scale 3.0 benchmark -solver "minisat"
