from __future__ import print_function
import random
import argparse
import sys


def main(varcount, clausecount, literals_per_clause, genseed, permuteseed):
    random.seed(genseed)

    # print("Generating with {} variables and {} clauses".format(varcount, clausecount))

    # Generate the collection of literals
    literal_sequence = []
    variable_sequence = []
    for v in range(1, varcount+1):
        literal_sequence.append(v)
        literal_sequence.append(-v)
        variable_sequence.append(v)
    literal_sequence.sort()
    variable_sequence.sort()

    def is_taut(c):
        for l in c:
            if -l in c:
                return True
        return False

    # Little ambiguous to draw uniformly...
    clauses = []
    while len(clauses) < clausecount:
        c = random.sample(literal_sequence, literals_per_clause)
        if is_taut(c):
            continue
        clauses.append(c)

    assert(len(clauses) == clausecount)

    # If requested, permute by the next seed
    if permuteseed != 0:
        random.seed(permuteseed)
        # Randomize the order of the variables
        random.shuffle(variable_sequence)
        # Create the permutation from that
        remapping = {}
        for v in range(1, varcount+1):
            remapping[v] = variable_sequence[v-1]
            remapping[-v] = -variable_sequence[v-1]

        # Randomize the order of the clauses
        random.shuffle(clauses)

        # Randomize the literals in the clauses
        for c in clauses:
            for i in range(len(c)):
                c[i] = remapping[c[i]]

    return clauses


def cnf_to_string(clauses):
    clause_count = len(clauses)
    varcount = 0
    for c in clauses:
        for l in c:
            varcount = max(varcount, abs(l))
    res = 'p cnf ' + str(varcount) + ' ' + str(clause_count) + '\n'
    for c in clauses:
        res += ' '.join(map(str, c)) + ' 0\n'
    return res


def pretty_print(clauses):
    print(cnf_to_string(clauses))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate k-sat instances")
    parser.add_argument('-v', '--vars', type=int, required=True)
    parser.add_argument('-c', '--clauses', type=int)
    parser.add_argument('-r', '--ratio', type=float, default=4.26)
    parser.add_argument('-k', '--literals-per-clause', type=int, default=3)
    parser.add_argument('-o', '--out-file', type=str)

    parser.add_argument('-s', '--seed', type=int, default=0)
    parser.add_argument('-p', '--permute', type=int, default=0)

    parser.add_argument('-d', '--debug', type=int)

    # handle input
    args = parser.parse_args()
    if args.vars <= 0:
        print("Error, should have positive number of variables")
        sys.exit(1)

    if args.clauses is None:
        if args.ratio <= 0:
            print("Error, bad ratio")
            sys.exit(2)
        args.clauses = int(args.vars * args.ratio)

    res = main(args.vars, args.clauses,
               args.literals_per_clause, args.seed, args.permute)

    pretty_print(res)
