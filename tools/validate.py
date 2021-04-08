
from __future__ import print_function
import random
import argparse
import sys

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Validate a certificate against a CNF")
    parser.add_argument('-c', '--certificate', type=str, required=True)
    parser.add_argument('-p', '--cnf-formula', type=str, required=True)

    parser.add_argument('-d', '--debug', type=int)

    # handle input
    args = parser.parse_args()

    # load in the certificate:
    mapping = []
    with open(args.certificate) as certificate:
        for l in certificate:
            mapping.append(int(l))
    
    cnf = []
    with open(args.cnf_formula) as cnf_formula:
        clause = []
        for line in cnf_formula:
            line = line.strip()
            if len(line) == 0: continue
            words = line.split()
            if words[0] == 'c' or words[0] == 'p': continue
            for w in words:
                w = int(w)
                if w == 0:
                    assert len(clause) > 0
                    cnf.append(clause[:])
                    clause = []
                else:
                    clause.append(w)
        assert len(clause) == 0

    for i, c in enumerate(cnf):
        print(i)
        is_sat = False
        for l in mapping:
            if l in c: 
                is_sat = True
        if not is_sat:
            print("Error with ", c)