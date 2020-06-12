#make clean
#make -j sat

# https://developer.mantidproject.org/ProfilingWithValgrind.html
#valgrind --tool=callgrind --dump-instr=yes --simulate-cache=yes --collect-jumps=yes ./sat < tests/uuf250-1065/uuf250-01.cnf
#valgrind --tool=callgrind --dump-instr=yes ./sat < tests/uuf250-1065/uuf250-01.cnf


#valgrind --tool=massif ./sat < tests/uuf200-860/uuf200-01.cnf

#filename=$(ls massif*)
#extension=${filename##*.}
#echo $filename
#echo $extension

#ms_print $filename







rm callgrind*
valgrind --tool=callgrind --dump-instr=yes ./sat < tests/uuf200-860/uuf200-01.cnf
filename=$(ls callgrind*)
extension=${filename##*.}
echo $filename
echo $extension


gprof2dot --format=callgrind --output=uuf250-01.$extension.dot $filename
dot < uuf250-01.$extension.dot -Tpdf > uuf250-01.$extension.pdf
cp uuf250-01.$extension.pdf /mnt/c/Users/agore/Desktop/
callgrind_annotate --auto=yes callgrind.out.$extension > annotate.$extension.txt
objdump -d -S -M intel sat > asm.txt










#time ./sat < tests/uuf250-1065/uuf250-01.cnf



# TODO: get the callgrind.out.pid exact filename.
#callgrind_file=callgrind.out.9329

# https://web.stanford.edu/class/archive/cs/cs107/cs107.1174/guide_callgrind.html
#callgrind_annotate --auto=yes callgrind.out.pid

# https://stackoverflow.com/questions/375913/how-can-i-profile-c-code-running-on-linux
# https://github.com/jrfonseca/gprof2dot
