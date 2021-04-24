import re
import subprocess
import sys
# Takes a solver name and input stream, and looks for "=+\n" (so to speak)
# that delimits the CNFs provided over the input stream, and feeds it in.

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
        [solver_name] + flags, stdin=subprocess.PIPE)
    # [solver_name] + flags, stdin=subprocess.PIPE, stdout=subprocess.DEVNULL)
    p.communicate(cnf_string.encode())


if __name__ == "__main__":
    for cnf in isolate_stream(sys.stdin):
        # Skip empty ones
        if cnf.strip() == '':
            continue
        run_solver_on_input(cnf)
