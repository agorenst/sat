from __future__ import print_function
import argparse
import sys
import random
import graph_enum
from collections import Counter, defaultdict
'''
This captures all the logic I have for generating graphs.
'''


def min_degree_gen(n, e, d):
    edges = set([(i, j) for i in range(n) for j in range(n) if j > i])
    edgecount = int(len(edges)*e)

    g = []
    # for each node, pick d random edges:
    for u in range(n):
        g += random.sample([(u, v)
                            for v in range(n) if v != u], d)
    # sort things
    g = [(min(e), max(e)) for e in g]
    # remove redundancies
    g = set(g)
    # remove those edges from consideration
    edges -= g
    edges = list(edges)
    edges.sort()

    g = list(g)
    # Now let's pick the remaining edges
    orig_edgecount = edgecount
    edgecount -= len(g)
    # assert(edgecount >= 0) Make this a warning...
    if edgecount <= 0:
        sys.stderr.write("Error, edgecount is " + str(len(g)) +
                         " when we wanted at most " + str(orig_edgecount) + '\n')
    else:
        g += random.sample(edges, edgecount)
    g.sort()
    graph_enum.reduction(g)


def gen(n, e, d=0):
    edges = [(i, j) for i in range(n) for j in range(n) if j > i]
    edgecount = int(len(edges)*e)

    g = random.sample(edges, edgecount)

    # Keep on randomly drawing until we get a degree we like.
    assert(edgecount*2 > n * d)
    if d > 0:
        c = Counter([u for e in g for u in e])
        min_value = c.most_common()[-1][1]
        while min_value < d:
            g = random.sample(edges, edgecount)
            c = Counter([u for e in g for u in e])
            min_value = c.most_common()[-1][1]

    g.sort()
    graph_enum.reduction(g)


def main():
    parser = argparse.ArgumentParser(description='Generate graphs')
    parser.add_argument('--number-of-nodes', metavar="N", type=int, default='8',
                        help='The number of nodes in the generated graph')
    parser.add_argument('--edge-density', metavar="D", type=float, default='0.5',
                        help='There are n-choose-2 possible edges; we multiply that by density to choose the edge count')
    parser.add_argument('--batch', metavar="D", type=int, default=1,
                        help='How many samples to emit?')
    parser.add_argument('--seed', metavar="S", type=int, default=0,
                        help='Random seed')
    parser.add_argument('--min-degree', metavar="R", type=int, default=0,
                        help='Specify the minimum degree we demand of the graph')
    args = parser.parse_args()
    n = args.batch - 1
    assert(n >= 0)
    random.seed(args.seed)
    min_degree_gen(args.number_of_nodes, args.edge_density, args.min_degree)
    for i in range(n):
        print('='*80)
        min_degree_gen(args.number_of_nodes,
                       args.edge_density, args.min_degree)


if __name__ == "__main__":
    main()
