# CXX=clang

# a sequence of basic tests, really trivial CNFs
tests0: sat
	./sat < tests/trivial1.cnf
	./sat < tests/trivial2.cnf

# Build the main executable
sat:

run_tests: 
	minisat < tests/trivial1.cnf | grep SATISFIABLE
	minisat < tests/trivial2.cnf | grep UNSATISIFIABLE
