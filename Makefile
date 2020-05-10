# CXX=clang
CXXFLAGS=-Wall -std=c++1z

# This is a sequence of tests where have the trivial 8-clause unsat 3SAT instances.
tests8: tests0
	./sat < tests/tests_3sat_unsat.cnf

# a sequence of basic tests, really trivial CNFs
tests0: sat
	./sat < tests/trivial1.cnf
	./sat < tests/trivial2.cnf
	./sat < tests/trivial3.cnf


# Build the main executable
sat:

run_tests: 
	minisat < tests/trivial1.cnf | grep SATISFIABLE
	minisat < tests/trivial2.cnf | grep UNSATISIFIABLE
