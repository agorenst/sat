import sys
import argparse
import subprocess
import itertools

# For my experiments, let's enumerate every labeled graph of size n


# What edges in g touch u?
def incident_edges(g, u):
    for (w, v) in g:
        if w == u or v == u:
            yield (w, v)


def make_reduction_map(g):
    edge_number = 1
    reduction_map = {}
    for e in g:
        reduction_map[e] = str(edge_number)
        edge_number += 1
    return reduction_map


# Given a graph, print the reduction of its PM problem to CNF


def reduction(g):
    reduction_map = make_reduction_map(g)
    ns = set([])
    for u, v in g:
        ns.add(u)
        ns.add(v)
    for u in ns:
        match_me_clause = ' '.join(reduction_map[e]
                                   for e in incident_edges(g, u))
        match_me_clause += ' 0'
        print(match_me_clause)

        for e in incident_edges(g, u):
            for h in incident_edges(g, u):
                if e == h:
                    continue
                print('-'+reduction_map[e] + ' -'+reduction_map[h] + ' 0')


def graph_as_dot(g):
    global reduction_map
    s = ''
    s += 'graph {\n'
    for u, v in g:
        s += str(u) + ' -- ' + str(v) + '[label='+reduction_map[(u, v)]+'];\n'
    s += '}\n'
    return s


def make_graph_pdf(g):
    s = graph_as_dot(g)
    p = subprocess.Popen(
        ['dot'] + ['-Tpdf', '-otmp.pdf'], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    p.communicate(s.encode())


def read_graph(infile):
    global reduction_map
    reduction_map.clear()
    g = []
    c = 1
    for line in infile:
        line = line.strip()
        if line == '':
            continue
        if line[0] == '#':  # support comments
            continue
        nodes = map(int, line.split())
        assert(len(nodes) == 2)
        u = min(nodes)
        v = max(nodes)
        g.append((u, v))
        reduction_map[(u, v)] = str(c)
        c += 1
    if args.dot:
        make_graph_pdf(g)
    return g


def produce_one_graph():
    e = int((len(edges)+1) / 2)
    c = 0
    for g in itertools.combinations(edges, e):
        if c == args.seed:
            reduction(g)
            if args.dot:
                make_graph_pdf(g)
            break
        c += 1


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Generate graphs')
    parser.add_argument('--number-of-nodes', metavar="N", type=int, default='8',
                        help='The number of nodes in the generated graph')
    parser.add_argument('--produce-one-graph', action='store_true',
                        help='Different mode, where we produce 1 graph of size N')
    parser.add_argument('--seed', metavar="S", type=int, default=0,
                        help='Random number generator seed')
    parser.add_argument('--dot', action='store_true',
                        help='Output graphviz format for (single) graph')
    parser.add_argument('--reduce-graph', action='store_true',
                        help='Reduce graph fed in from stdin')

    args = parser.parse_args()

    n = args.number_of_nodes

    # Define all possible edges (given n) and map each one to a CNF variable
    edges = [(i, j) for i in range(n) for j in range(n) if j > i]
    if args.produce_one_graph:
        produce_one_graph()
    elif args.reduce_graph:
        g = read_graph(sys.stdin)
        reduction(g)
    else:
        for e in range(n-1, len(edges)+1):
            for g in itertools.combinations(edges, e):
                reduction(g)
                print('=============')
