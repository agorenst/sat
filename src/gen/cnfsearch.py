import sys
import satgen
import subprocess

'''
This is a (ever-growing) program that uses a random CNF generator to find
"interesting" inputs for the provided sat solver.
'''


def exercise_sanitized(seed_start, seed_end):
    solver_name = './build/sat.san'
    for i in range(seed_start, seed_end):
        example_cnf = satgen.main(200, 426*2, 3, i, 0)
        cnf_string = satgen.cnf_to_string(example_cnf)
        p = subprocess.Popen([solver_name], stdin=subprocess.PIPE)
        p.communicate(cnf_string.encode())


exercise_sanitized(0, 100)
