CXX=clang++
CXXFLAGS=-Wall -std=c++1z -g -O2

uf50-218-tests = $(addsuffix .satresult, $(basename $(wildcard tests/uf50-218/*.cnf)))
uuf50-218-tests = $(addsuffix .unsatresult, $(basename $(wildcard tests/uuf50-218/*.cnf)))
uf250-1065-tests = $(addsuffix .satresult, $(basename $(wildcard tests/uf250-1065/*.cnf)))
uuf250-1065-tests = $(addsuffix .unsatresult, $(basename $(wildcard tests/uuf250-1065/*.cnf)))

#run-tests: $(uuf50-218-tests) $(uf50-218-tests)
run-tests: $(uf250-1065-tests)

fail1: sat
	cat tests/uuf50-218/uuf50-0218.cnf | sed s/%// | sed s/^0// | ./sat


unit: sat
	./sat < tests/units1.cnf



# $(info $(uf50-218-tests))

%.unsatresult : %.cnf sat
	cat $< | sed s/%// | sed s/^0// | ./sat | tail -n 1 | diff - UNSAT
%.satresult : %.cnf sat
	cat $< | sed s/%// | sed s/^0// | ./sat | tail -n 1 | diff - SAT


# a sequence of basic tests, really trivial CNFs
tests0: sat
	./sat < tests/trivial1.cnf
	./sat < tests/trivial2.cnf
	./sat < tests/trivial3.cnf

# This is a sequence of tests where have the trivial 8-clause unsat 3SAT instances.
tests8: tests0
	./sat < tests/tests_3sat_unsat.cnf

run_tests: 
	minisat < tests/trivial1.cnf | grep SATISFIABLE
	minisat < tests/trivial2.cnf | grep UNSATISIFIABLE

# Build the main executable
sat: sat.cpp



clean:
	rm sat


# Run in bactch mode
#gdb -q -batch -ex run -ex backtrace ./sat < tests/units1.cnf
