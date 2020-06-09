#make clean
#make -j sat

# https://developer.mantidproject.org/ProfilingWithValgrind.html
#valgrind --tool=callgrind --dump-instr=yes --simulate-cache=yes --collect-jumps=yes ./sat < tests/uuf250-1065/uuf250-01.cnf
valgrind --tool=callgrind --dump-instr=yes --simulate-cache=yes ./sat < tests/uuf250-1065/uuf250-01.cnf

filename=$(ls callgrind*)
extension=${filename##*.}
echo $filename
echo $extension


# TODO: get the callgrind.out.pid exact filename.
#callgrind_file=callgrind.out.9329

# https://web.stanford.edu/class/archive/cs/cs107/cs107.1174/guide_callgrind.html
#callgrind_annotate --auto=yes callgrind.out.pid

# https://stackoverflow.com/questions/375913/how-can-i-profile-c-code-running-on-linux
# https://github.com/jrfonseca/gprof2dot


gprof2dot --format=callgrind --output=uuf250-01.$extension.dot $filename
dot < uuf250-01.$extension.dot -Tpdf > uuf250-01.$extension.pdf
cp uuf250-01.$extension.pdf /mnt/c/Users/agore/Desktop/

#time ./sat < tests/uuf250-1065/uuf250-01.cnf
