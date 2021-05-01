import os
import sys
import satgen
import subprocess
import time
import pprint
import argparse

def handle_arguments():
    parser = argparse.ArgumentParser(description="Facilitate large-scale scans of inputs for SAT solver")
    parser.add_argument('task', choices=['find-slowest-input', 'benchmark'])
    parser.add_argument('-lower-bound', type=int, default=0)
    parser.add_argument('-upper-bound', type=int, default=10)
    parser.add_argument('-scale', type=float, default=2.5)
    parser.add_argument('-seed', type=int, default=0)
    parser.add_argument('-flags', type=str, nargs='+', default=[])
    parser.add_argument('-solver', type=str, default='./build/sat')

    args = parser.parse_args()
    return args

def main():
    args = handle_arguments()
    if args.task == 'find-slowest-input':
        find_slowest(args.lower_bound, args.upper_bound, args.scale)
    else:
        assert(args.task =='benchmark')
        benchmark1(args.lower_bound, args.upper_bound, args.solver, flags=args.flags, scale=args.scale)


def find_slowest(seed_start, seed_end, scale, solver):
    seed_time = {}
    for i in range(seed_start, seed_end):
        print("Doing ", i)
        seed_time[i] = 0
        start = time.time()
        for p in range(1):
            example_cnf = satgen.main(int(100*scale), int(426*scale), 3, i, p)
            cnf_string = satgen.cnf_to_string(example_cnf)
            p = subprocess.Popen(
                [solver], stdin=subprocess.PIPE, stdout=subprocess.DEVNULL)
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


def benchmark1(seed_start, seed_end, solver_name='./build/sat', flags=[], scale=2.5):
    print(flags)
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

if __name__ == "__main__":
    main()