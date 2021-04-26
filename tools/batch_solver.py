import re
import subprocess
import sys
import argparse
# Takes a solver name and input stream, and looks for "=+\n" (so to speak)
# that delimits the CNFs provided over the input stream, and feeds it in.

parser = argparse.ArgumentParser(description='Run a sat solver many times')
parser.add_argument('--print-unsat', action='store_true',
                    help='Print out CNFs for which we are unsat')
parser.add_argument('--print-unsat-index', action='store_true',
                    help='Print out the index of the currently-processing input when its unsat')
parser.add_argument('--print-index', action='store_true',
                    help='Print out the index of the currently-processing input')
parser.add_argument('--start-index', metavar="S", type=int, default=0,
                    help='What index should we start processing the batch (i.e., how many inputs should we skip)')
args = parser.parse_args()

solver = './../build/sat'
delimeter = re.compile('^=+$')


def isolate_stream(infile):
    real_input = ''
    for line in infile:
        if delimeter.match(line):
            yield real_input
            real_input = ''
            continue
        real_input += line


def run_solver_on_input(cnf_string, solver_name='./../build/sat', flags=[]):
    p = subprocess.Popen(
        [solver_name] + flags, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    # [solver_name] + flags, stdin=subprocess.PIPE)
    # [solver_name] + flags, stdin=subprocess.PIPE, stdout=subprocess.DEVNULL)
    (o, _) = p.communicate(cnf_string.encode())
    return o.decode().strip()


def validate_pm_reduction_assumption(cnf_string):
    real_answer = run_solver_on_input(cnf_string)
    candidate_answer = run_solver_on_input(
        cnf_string, flags=["--only-positive-choices"])
    assert(real_answer == candidate_answer)


if __name__ == "__main__":
    c = 0
    for cnf in isolate_stream(sys.stdin):
        # Skip empty ones, don't count them
        if cnf.strip() == '':
            continue
        if c < args.start_index:
            c += 1
            continue

        result = run_solver_on_input(cnf)
        if args.print_unsat_index and result == 'UNSATISFIABLE':
            print('count:', c)
        if args.print_unsat and result == 'UNSATISFIABLE':
            print(cnf)

        # Note count is always yeah
        c += 1
        # validate_pm_reduction_assumption(cnf)
