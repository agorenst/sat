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


# TODO: figure out how to parallelize this even while we have to wait for
# the stdout of each thread.
def confirm_with_minisat(seed_start, seed_end, scale):
    diff_name = './build/sat'
    #diff_name = '/mnt/c/Users/agore/source/repos/sat/build/Release/sat.exe'
    base_name = 'minisat'
    for i in range(seed_start, seed_end):
        example_cnf = satgen.main(int(100 * scale), int(426 * scale), 3, i, 0)
        cnf_string = satgen.cnf_to_string(example_cnf)
        d = subprocess.Popen(
            [diff_name], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        (o, _) = d.communicate(cnf_string.encode())
        o = o.decode()
        my_answer = o.strip()

        b = subprocess.Popen(
            [base_name], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        (a, _) = b.communicate(cnf_string.encode())
        a = a.decode()
        # get the last word (unsat or sat)
        true_answer = a.split()[-1].strip()

        if i % 10 == 0:
            print(i)

        assert(my_answer == true_answer)


def find_slowest(seed_start, seed_end):
    solver_name = './build/sat'
    #solver_name = 'minisat'
    seed_time = {}
    scale = 2.3
    for i in range(seed_start, seed_end):
        print("Doing ", i)
        seed_time[i] = 0
        start = time.time()
        for p in range(1):
            example_cnf = satgen.main(int(100*scale), int(426*scale), 3, i, p)
            cnf_string = satgen.cnf_to_string(example_cnf)
            p = subprocess.Popen(
                [solver_name], stdin=subprocess.PIPE, stdout=subprocess.DEVNULL)
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


def benchmark1(seed_start, seed_end, solver_name='./build/sat', flags=[]):
    scale = 2.5
    start = time.time()
    for i in range(seed_start, seed_end):
        example_cnf = satgen.main(int(100*scale), int(426*scale), 3, i, 0)
        cnf_string = satgen.cnf_to_string(example_cnf)
        p = subprocess.Popen(
            # [solver_name] + flags, stdin=subprocess.PIPE)
            [solver_name] + flags, stdin=subprocess.PIPE, stdout=subprocess.DEVNULL)
        p.communicate(cnf_string.encode())
    end = time.time()
    print(end-start)


def compare_benchmark():
    l = 10
    u = 30
    print("My solver without on the fly subsumption (but with backtrack subsumption):")
    benchmark1(l, u, './build/sat', ['--on-the-fly-subsumption-'])
    print("My solver without backtrack subsumption (but on the fly)")
    benchmark1(l, u, './build/sat', ['--backtrack-subsumption-'])
    print("My solver without either")
    benchmark1(l, u, './build/sat', ['--backtrack-subsumption-', '--on-the-fly-subsumption-'])
    print("My solver with both!")
    benchmark1(l, u, './build/sat', ['--backtrack-subsumption-', '--on-the-fly-subsumption-'])
    # benchmark1(l, u, './build/sat', ['--naive-vsids'])
    # print("My solver better:")
    # print("My solver naive:")
    # benchmark1(l, u, './build/sat', ['--naive-vsids'])
    # print("My solver better:")
    # benchmark1(l, u, './build/sat')

    print("Minisat:")
    benchmark1(l, u, 'minisat')


def single_big_one():
    solver_name = './build/sat'
    scale = 2.5
    example_cnf = satgen.main(int(100*scale), int(426*scale), 3, 0, 0)
    cnf_string = satgen.cnf_to_string(example_cnf)
    p = subprocess.Popen([solver_name], stdin=subprocess.PIPE)
    p.communicate(cnf_string.encode())


if __name__ == "__main__":
    # exercise_sanitized(0, 100)
    #find_slowest(0, 10)

    #confirm_with_minisat(0, 1000, 0.3)
    #confirm_with_minisat(0, 100, 0.8)
    #confirm_with_minisat(0, 10, 2.0)

    compare_benchmark()
    # single_big_one()
