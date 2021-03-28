#!/bin/bash

# First, reset the state:
rm -r generated_input
rm -r unique_input
rm -r minned_input
rm -r fuzz_output
rm dump*

# Generate the initial test data
mkdir generated_input
for i in {1..3}; do
    python3 ./../../src/gen/satgen.py -v 100 -c 426 -s $i > generated_input/test_100_426_$i.cnf
done

# Run the solver on it just to make sure we're getting a good spread of sat/unsat
for f in generated_input/*.cnf; do ./../../build/sat.afl < $f; done

# Uniqueify the test data
mkdir unique_input
afl-cmin -i generated_input/ -o unique_input/ -- ./../../build/sat.afl

# Min the test data!
mkdir minned_input
for i in ./unique_input/*; do
    # The & is horrifying
    bn=$(basename $i)
    echo $bn
    afl-tmin -i $i -o minned_input/$bn.min -- ./../../build/sat.afl 
done

# Finally, run the fuzzer for 10 minutes
timeout 10m afl-fuzz -i minned_input/ -o fuzz_output/ -- ./../../build/sat.afl

# And then run it on the result just in case
for f in fuzz_output/crashes/id*; do ./../../build/sat.afl < $f; done

# Finally, save a dump of each crash.
for f in fuzz_output/crashes/id*; do
    echo "./../../build/sat.dbg < $f" > dump.$(basename $f)
    gdb -q -batch -ex run -ex backtrace ./../../build/sat.dbg < $f >> dump.$(basename $f)
done