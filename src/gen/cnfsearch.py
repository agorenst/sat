import os
import sys
import satgen
import subprocess
import time
import pprint

'''
This is a (ever-growing) program that uses a random CNF generator to find
"interesting" inputs for the provided sat solver.
'''


def exercise_sanitized(seed_start, seed_end):
    solver_name = './build/sat.san'
    os.environ['UBSAN_OPTIONS'] = 'print_stacktrace=1'
    # TODO: it would be nice to parallelize this. "communicate" means
    # the script polls on the whole thing.
    for i in range(seed_start, seed_end):
        example_cnf = satgen.main(200, 426*2, 3, i, 0)
        cnf_string = satgen.cnf_to_string(example_cnf)
        p = subprocess.Popen([solver_name], stdin=subprocess.PIPE)
        p.communicate(cnf_string.encode())


def find_slowest(seed_start, seed_end):
    solver_name = './build/sat'
    seed_time = {}
    scale = 2.5
    for i in range(seed_start, seed_end):
        seed_time[i] = 0
        start = time.time()
        for p in range(5):
            example_cnf = satgen.main(int(100*scale), int(426*scale), 3, i, p)
            cnf_string = satgen.cnf_to_string(example_cnf)
            p = subprocess.Popen([solver_name], stdin=subprocess.PIPE)
            p.communicate(cnf_string.encode())
        end = time.time()
        seed_time[i] = end-start

    max_seed = -1
    max_time = 0
    for s, t in seed_time.items():
        if t > max_time:
            max_seed = s
            max_time = t
    pprint.pprint(seed_time)
    print(max_seed, max_time)


if __name__ == "__main__":
    #exercise_sanitized(0, 100)
    find_slowest(0, 10)
