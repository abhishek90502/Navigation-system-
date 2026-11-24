/* Routing algorithms: BFS, DFS, Dijkstra (min-heap), Yen's k-shortest paths. */
#ifndef ROUTING_H
#define ROUTING_H

#include <stdbool.h>
#include "graph.h"

typedef struct {
    int    *nodes;  /* location indices, src first, dst last */
    int     len;
    double  cost;   /* in the chosen metric (hops for BFS/DFS) */
} Path;

void   path_free(Path *p);
bool   bfs_path(const Graph *g, int src, int dst, Path *out);
bool   dfs_path(const Graph *g, int src, int dst, Path *out);
bool   dijkstra_path(const Graph *g, int src, int dst, Metric m,
                     const bool *banned_node, const bool *banned_target, Path *out);
double path_cost(const Graph *g, const int *nodes, int len, Metric m);
void   route_totals(const Graph *g, const int *nodes, int len, double *dist, double *time);
int    yen_k_shortest(const Graph *g, int src, int dst, int k, Metric m, Path *accepted);

#endif
