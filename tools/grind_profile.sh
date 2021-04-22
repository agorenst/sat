#!/bin/bash

testname=satgen.300
rm $testname*
rm asm.txt
rm callgrind*

python3 ./../src/gen/satgen.py -v 300 | valgrind --tool=callgrind --dump-instr=yes ./../build/sat --only-preprocess

filename=$(ls callgrind*)
extension=${filename##*.}
echo $filename
echo $extension


gprof2dot --format=callgrind --output=$testname.$extension.dot $filename
dot < $testname.$extension.dot -Tpdf > $testname.pdf
cp $testname.pdf /mnt/c/Users/agore/Desktop/
callgrind_annotate --auto=yes callgrind.out.$extension > $testname.$extension.txt
objdump -d -S -M intel ./../build/sat > asm.txt