CXX=clang++
CC=clang++
CXXFLAGS=-Wall -std=c++1z -g

uf50-218-tests = $(addsuffix .satresult, $(basename $(wildcard tests/uf50-218/*.cnf)))
uuf50-218-tests = $(addsuffix .unsatresult, $(basename $(wildcard tests/uuf50-218/*.cnf)))

uf100-430-tests = $(addsuffix .satresult, $(basename $(wildcard tests/uf100-430/*.cnf)))
uuf100-430-tests = $(addsuffix .unsatresult, $(basename $(wildcard tests/uuf100-430/uuf100-011*.cnf)))

uf250-1065-tests = $(addsuffix .satresult, $(basename $(wildcard tests/uf250-1065/*.cnf)))
uuf250-1065-tests = $(addsuffix .unsatresult, $(basename $(wildcard tests/uuf250-1065/*.cnf)))

sat: sat.o cnf.o
tests: $(uuf50-218-tests) $(uf50-218-tests) tests0 tests8 unit

benchmark1: $(uuf50-218-tests)
benchmark2: $(uuf100-430-tests)

unit: sat
	./sat < tests/units1.cnf | diff - SAT

%.unsatresult : %.cnf sat
	cat $< | sed s/%// | sed s/^0// | ./sat | tail -n 1 | diff - UNSAT
%.satresult : %.cnf sat
	cat $< | sed s/%// | sed s/^0// | ./sat | tail -n 1 | diff - SAT


# a sequence of basic tests, really trivial CNFs
tests0: sat
	./sat < tests/trivial1.cnf | diff - SAT
	./sat < tests/trivial2.cnf | diff - UNSAT
	./sat < tests/trivial3.cnf | diff - SAT

# This is a sequence of tests where have the trivial 8-clause unsat 3SAT instances.
tests8: tests0
	./sat < tests/tests_3sat_unsat.cnf | diff - UNSAT

clean:
	rm -f sat *.o


# Run in bactch mode
#gdb -q -batch -ex run -ex backtrace ./sat < tests/units1.cnf

# $(info $(uf50-218-tests))

fix: sat
	gdb -q -batch -ex run -ex backtrace ./sat < tests/tests_3sat_unsat.cnf
#gdb -q -batch -ex run -ex backtrace ./sat < tests/uf50-218/uf50-0100.cnf
#gdb -q -batch -ex run -ex backtrace ./sat < tests/tests_3sat_unsat.cnf
