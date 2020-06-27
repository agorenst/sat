CXX=clang++
CC=clang++
CXXFLAGS=-Wall -std=c++2a -g -O2 -flto
#-O2 -flto

#ifdef SAT_DEBUG_MODE

uf20-91-tests = $(addsuffix .satresult, $(basename $(wildcard tests/uf20-91/*.cnf)))

uf50-218-tests = $(addsuffix .satresult, $(basename $(wildcard tests/uf50-218/*.cnf)))
uuf50-218-tests = $(addsuffix .unsatresult, $(basename $(wildcard tests/uuf50-218/*.cnf)))

uf100-430-tests = $(addsuffix .satresult, $(basename $(wildcard tests/uf100-430/*.cnf)))
uf100-430-tests-medium = $(addsuffix .satresult, $(basename $(wildcard tests/uf100-430/uf100-01*.cnf)))
uuf100-430-tests-brief = $(addsuffix .unsatresult, $(basename $(wildcard tests/uuf100-430/uuf100-011*.cnf)))
uuf100-430-tests-medium = $(addsuffix .unsatresult, $(basename $(wildcard tests/uuf100-430/uuf100-01*.cnf)))
uuf100-430-tests = $(addsuffix .unsatresult, $(basename $(wildcard tests/uuf100-430/*.cnf)))

uuf150-645-tests = $(addsuffix .unsatresult, $(basename $(wildcard tests/uuf150-645/*.cnf)))
uuf150-645-tests-brief = $(addsuffix .unsatresult, $(basename $(wildcard tests/uuf150-645/uuf150-01*.cnf)))

uf250-1065-tests = $(addsuffix .satresult, $(basename $(wildcard tests/uf250-1065/*.cnf)))
uuf250-1065-tests = $(addsuffix .unsatresult, $(basename $(wildcard tests/uuf250-1065/*.cnf)))

sat: cnf.o trace.o watched_literals.o preprocess.o lcm.o action.o trail.o clause_learning.o backtrack.o bce.o literal_incidence_map.o vsids.o unit_queue.o subsumption.o viv.o plugins.o visualizations.o circuit.o clause_set.o lbm.o solver.o

# to test learning, e.g.: ./test_learning < tests/clause_learning/t3.learninglog
test_learning: clause_learning.o trail.o cnf.o action.o lcm.o literal_incidence_map.o visualizations.o

tests: $(uuf50-218-tests) $(uf50-218-tests) tests0 tests8 unit
#tests: $(uf20-91-tests)

benchmark1: $(uuf50-218-tests)
benchmark2: $(uuf100-430-tests-brief)
benchmark3: $(uuf100-430-tests-medium)
benchmark4: $(uuf100-430-tests)
benchmark5: $(uf100-430-tests-medium)
benchmark6: $(uuf150-645-tests-brief)
benchmark7: $(uuf150-645-tests)

# The ultimate!
benchmark10: $(uuf250-1065-tests)

timing1: sat
	cat tests/uuf250-1065/uuf250-01.cnf | sed s/%// | sed s/^0// | ./sat
	cat tests/uuf250-1065/uuf250-01.cnf | sed s/%// | sed s/^0// | ./sat
	cat tests/uuf250-1065/uuf250-01.cnf | sed s/%// | sed s/^0// | ./sat

unit: sat
	./sat < tests/units1.cnf | diff - SAT

# minisat -no-pre -no-elim -no-luby -rfirst=100000

%.unsatresult : %.cnf sat
	cat $< | sed s/%// | sed s/^0// | ./sat -u w -l r -b n | tail -n 1 | diff - UNSAT
#	cat $< | sed s/%// | sed s/^0// | ./sat -u s -l r -b n | tail -n 1 | diff - UNSAT
#	cat $< | sed s/%// | sed s/^0// | ./sat -u s -l s -b n | tail -n 1 | diff - UNSAT
#	cat $< | sed s/%// | sed s/^0// | ./sat -u s -l r -b s | tail -n 1 | diff - UNSAT
#	cat $< | sed s/%// | sed s/^0// | ./sat -u s -l s -b s | tail -n 1 | diff - UNSAT
#	cat $< | sed s/%// | sed s/^0// | ./sat -u w -l s -b n | tail -n 1 | diff - UNSAT
#	cat $< | sed s/%// | sed s/^0// | ./sat -u w -l r -b s | tail -n 1 | diff - UNSAT
#	cat $< | sed s/%// | sed s/^0// | ./sat -u w -l s -b s | tail -n 1 | diff - UNSAT
#	cat $< | sed s/%// | sed s/^0// | ./sat -u q -l r -b n | tail -n 1 | diff - UNSAT
#	cat $< | sed s/%// | sed s/^0// | ./sat -u q -l s -b n | tail -n 1 | diff - UNSAT
#	cat $< | sed s/%// | sed s/^0// | ./sat -u q -l r -b s | tail -n 1 | diff - UNSAT
#	cat $< | sed s/%// | sed s/^0// | ./sat -u q -l s -b s | tail -n 1 | diff - UNSAT
%.satresult : %.cnf sat
	cat $< | sed s/%// | sed s/^0// | ./sat -u w -l r -b n | tail -n 1 | diff - SAT
#	cat $< | sed s/%// | sed s/^0// | ./sat -u s -l r -b n | tail -n 1 | diff - SAT
#	cat $< | sed s/%// | sed s/^0// | ./sat -u s -l s -b n | tail -n 1 | diff - SAT
#	cat $< | sed s/%// | sed s/^0// | ./sat -u s -l r -b s | tail -n 1 | diff - SAT
#	cat $< | sed s/%// | sed s/^0// | ./sat -u s -l s -b s | tail -n 1 | diff - SAT
#	cat $< | sed s/%// | sed s/^0// | ./sat -u w -l s -b n | tail -n 1 | diff - SAT
#	cat $< | sed s/%// | sed s/^0// | ./sat -u w -l r -b s | tail -n 1 | diff - SAT
#	cat $< | sed s/%// | sed s/^0// | ./sat -u w -l s -b s | tail -n 1 | diff - SAT
#	cat $< | sed s/%// | sed s/^0// | ./sat -u q -l r -b n | tail -n 1 | diff - SAT
#	cat $< | sed s/%// | sed s/^0// | ./sat -u q -l s -b n | tail -n 1 | diff - SAT
#	cat $< | sed s/%// | sed s/^0// | ./sat -u q -l r -b s | tail -n 1 | diff - SAT
#	cat $< | sed s/%// | sed s/^0// | ./sat -u q -l s -b s | tail -n 1 | diff - SAT


# a sequence of basic tests, really trivial CNFs
tests0: sat
	./sat < tests/trivial1.cnf | diff - SAT
	./sat < tests/trivial2.cnf | diff - UNSAT
	./sat < tests/trivial3.cnf | diff - SAT

# This is a sequence of tests where have the trivial 8-clause unsat 3SAT instances.
tests8: tests0
	./sat < tests/tests_3sat_unsat.cnf | diff - UNSAT

clean:
	rm -f sat *.o *~


# Run in bactch mode
#gdb -q -batch -ex run -ex backtrace ./sat < tests/units1.cnf

# $(info $(uf50-218-tests))

fix: sat
	gdb -q -batch -ex run -ex backtrace ./sat < tests/uuf50-218/uuf50-0222.cnf
#gdb -q -batch -ex run -ex backtrace ./sat < tests/uf50-218/uf50-0615.cnf
#gdb -q -batch -ex run -ex backtrace ./sat < tests/uuf50-218/uuf50-0830.cnf
#gdb -q -batch -ex run -ex backtrace ./sat < tests/uuf50-218/uuf50-0218.cnf
#gdb -q -batch -ex run -ex backtrace ./sat < tests/uuf50-218/uuf50-0757.cnf
#	./sat -u w -l r -b n < tests/uuf150-645/uuf150-01.cnf
#	cat tests/uuf150-645/uuf150-01.cnf | sed s/%// | sed s/^0// | minisat -no-pre -verb=2 -rinc=1000000 -ccmin-mode=0

#gdb -q -batch -ex run -ex backtrace ./sat < watched_bug_5.cnf
#gdb -q -batch -ex run -ex backtrace ./sat < tests/uf20-91/uf20-093.cnf
#gdb -q -batch -ex run -ex backtrace ./sat < tests/uuf50-218/uuf50-0218.cnf
#gdb -q -batch -ex run -ex backtrace ./sat < tests/uf50-218/uf50-0100.cnf
#gdb -q -batch -ex run -ex backtrace ./sat < tests/tests_3sat_unsat.cnf
#gdb -q -batch -ex run -ex backtrace ./sat < tests/tests_3sat_unsat.cnf

apply_trace: cnf.o
