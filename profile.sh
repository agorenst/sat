#make clean
#make -j sat

# https://developer.mantidproject.org/ProfilingWithValgrind.html
#valgrind --tool=callgrind --dump-instr=yes --simulate-cache=yes --collect-jumps=yes ./sat < tests/uuf250-1065/uuf250-01.cnf

# TODO: get the callgrind.out.pid exact filename.

# https://web.stanford.edu/class/archive/cs/cs107/cs107.1174/guide_callgrind.html
#callgrind_annotate --auto=yes callgrind.out.pid

# https://stackoverflow.com/questions/375913/how-can-i-profile-c-code-running-on-linux
# https://github.com/jrfonseca/gprof2dot
#gprof2dot --format=callgrind --output=uuf250-01.dot callgrind.out.17522

time ./sat < tests/uuf250-1065/uuf250-01.cnf
