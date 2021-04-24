import argparse
import itertools
# For my experiments, let's enumerate every labeled graph of size n

n = 6

# Define all possible edges (given n) and map each one to a CNF variable
edges = [(i, j) for i in range(n) for j in range(n) if j > i]
edge_number = 1
reduction_map = {}
for e in edges:
    reduction_map[e] = str(edge_number)
    edge_number += 1


# What edges in g touch u?
def incident_edges(g, u):
    for (w, v) in g:
        if w == u or v == u:
            yield (w, v)


# Given a graph, print the reduction of its PM problem to CNF
def reduction(g):
    n = max([max(i, j) for (i, j) in g])
    for u in range(n):
        match_me_clause = ' '.join(reduction_map[e]
                                   for e in incident_edges(g, u))
        match_me_clause += ' 0'
        print(match_me_clause)

        for e in incident_edges(g, u):
            for h in incident_edges(g, u):
                if e == h:
                    continue
                print('-'+reduction_map[e] + ' -'+reduction_map[h] + ' 0')


for e in range(n-1, len(edges)+1):
    for g in itertools.combinations(edges, e):
        reduction(g)
        print('=============')
