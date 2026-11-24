/* Unit tests. Build & run with: make test */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include "../src/heap.h"
#include "../src/graph.h"
#include "../src/routing.h"
#include "../src/sample.h"

static int passes = 0, failures = 0;

#define CHECK(cond) do { \
    if (cond) passes++; \
    else { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
} while (0)
#define CHECK_NEAR(a, b) CHECK(fabs((double)(a) - (double)(b)) < 1e-6)

/* ---------- brute force: every loopless path, to verify Dijkstra ---------- */
static void enumerate(const Graph *g, int u, int dst, Metric m,
                      bool *vis, int *path, int depth, double *best) {
    if (u == dst) {
        double c = path_cost(g, path, depth, m);
        if (c < *best) *best = c;
        return;
    }
    const EdgeList *l = &g->adj[u];
    for (int e = 0; e < l->count; e++) {
        Edge ed = l->items[e];
        if (g->roads[ed.road].closed || vis[ed.to]) continue;
        vis[ed.to] = true;
        path[depth] = ed.to;
        enumerate(g, ed.to, dst, m, vis, path, depth + 1, best);
        vis[ed.to] = false;
    }
}

static void test_heap(void) {
    MinHeap h;
    heap_init(&h);
    srand(42);
    double vals[200];
    for (int i = 0; i < 200; i++) {
        vals[i] = rand() % 1000;
        heap_push(&h, vals[i], i, i);
    }
    bool sorted = true;
    double prev = -1;
    for (int i = 0; i < 200; i++) {
        HeapItem it = heap_pop(&h);
        if (it.key < prev) sorted = false;
        prev = it.key;
    }
    CHECK(sorted);
    CHECK(heap_empty(&h));
    heap_free(&h);
}

static void test_graph_rules(void) {
    Graph g;
    graph_init(&g);
    CHECK(graph_add_location(&g, "X", "X") == 0);
    CHECK(graph_add_location(&g, "Y", "Y") == 1);
    CHECK(graph_add_location(&g, "X", "dup") < 0);              /* duplicate id */
    CHECK(graph_add_road(&g, 0, 0, 1.0, 0, "", true) < 0);      /* self loop */
    CHECK(graph_add_road(&g, 0, 1, -1.0, 0, "", true) < 0);     /* bad distance */

    int r = graph_add_road(&g, 0, 1, 50.0, 0, "", true);        /* time derived */
    CHECK(r >= 0);
    CHECK_NEAR(g.roads[r].time_min, 60.0);
    CHECK(graph_add_road(&g, 1, 0, 1.0, 0, "", true) < 0);      /* duplicate reverse */

    graph_set_closed(&g, 0, 1, true);
    CHECK(g.roads[r].closed);
    graph_free(&g);
}

static void test_shortest_and_bruteforce(void) {
    Graph g;
    graph_init(&g);
    build_sample_map(&g);
    int A = graph_find(&g, "A"), F = graph_find(&g, "F");

    Path p;
    CHECK(dijkstra_path(&g, A, F, METRIC_DISTANCE, NULL, NULL, &p));
    CHECK_NEAR(p.cost, 11.5);
    CHECK(p.nodes[0] == A && p.nodes[p.len - 1] == F);
    path_free(&p);

    bool vis[8] = {false};
    int path[8];
    double best = INFINITY;
    vis[A] = true;
    path[0] = A;
    enumerate(&g, A, F, METRIC_DISTANCE, vis, path, 1, &best);
    CHECK_NEAR(best, 11.5);

    int B = graph_find(&g, "B");
    CHECK(dijkstra_path(&g, A, B, METRIC_DISTANCE, NULL, NULL, &p));
    CHECK(p.len == 2);
    CHECK_NEAR(p.cost, 2.0);
    path_free(&p);
    graph_free(&g);
}

static void test_bfs_dfs(void) {
    Graph g;
    graph_init(&g);
    build_sample_map(&g);
    int A = graph_find(&g, "A"), F = graph_find(&g, "F");

    Path p;
    CHECK(bfs_path(&g, A, F, &p));
    CHECK(p.len - 1 == 3);            /* e.g. A-C-E-F */
    path_free(&p);

    CHECK(dfs_path(&g, A, F, &p));
    CHECK(p.nodes[0] == A && p.nodes[p.len - 1] == F);
    for (int i = 0; i + 1 < p.len; i++)
        CHECK(graph_find_road(&g, p.nodes[i], p.nodes[i + 1]) >= 0);
    path_free(&p);
    graph_free(&g);
}

static void test_yen(void) {
    Graph g;
    graph_init(&g);
    build_sample_map(&g);
    int A = graph_find(&g, "A"), F = graph_find(&g, "F");

    Path routes[5];
    int n = yen_k_shortest(&g, A, F, 5, METRIC_DISTANCE, routes);
    CHECK(n >= 2);

    bool sorted = true, distinct = true;
    for (int i = 1; i < n; i++)
        if (routes[i].cost < routes[i - 1].cost) sorted = false;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (routes[i].len == routes[j].len &&
                memcmp(routes[i].nodes, routes[j].nodes, (size_t)routes[i].len * sizeof(int)) == 0)
                distinct = false;
    CHECK(sorted);
    CHECK(distinct);

    Path best;
    dijkstra_path(&g, A, F, METRIC_DISTANCE, NULL, NULL, &best);
    CHECK_NEAR(routes[0].cost, best.cost);
    path_free(&best);

    for (int i = 0; i < n; i++) path_free(&routes[i]);
    graph_free(&g);
}

static void test_closures(void) {
    Graph g;
    graph_init(&g);
    build_sample_map(&g);
    int A = graph_find(&g, "A"), B = graph_find(&g, "B"), C = graph_find(&g, "C");
    int E = graph_find(&g, "E"), F = graph_find(&g, "F"), G_ = graph_find(&g, "G");

    int bc = graph_find_road(&g, B, C);
    g.roads[bc].closed = true;
    Path p;
    CHECK(dijkstra_path(&g, A, F, METRIC_DISTANCE, NULL, NULL, &p));
    CHECK_NEAR(p.cost, 12.0);                    /* forced detour A-B-D-G-F */
    path_free(&p);
    g.roads[bc].closed = false;                  /* restore */

    graph_set_closed(&g, E, F, true);
    graph_set_closed(&g, G_, F, true);
    CHECK(!dijkstra_path(&g, A, F, METRIC_DISTANCE, NULL, NULL, &p));
    graph_free(&g);
}

static void test_time_metric(void) {
    Graph g;
    graph_init(&g);
    graph_add_location(&g, "X", "X");
    graph_add_location(&g, "Y", "Y");
    graph_add_location(&g, "Z", "Z");
    graph_add_road(&g, 0, 1, 10.0, 30.0, "", true);   /* short but slow */
    graph_add_road(&g, 0, 2, 7.0, 3.0, "", true);     /* longer but fast */
    graph_add_road(&g, 2, 1, 8.0, 3.0, "", true);

    Path p;
    dijkstra_path(&g, 0, 1, METRIC_DISTANCE, NULL, NULL, &p);
    CHECK(p.len == 2);
    path_free(&p);
    dijkstra_path(&g, 0, 1, METRIC_TIME, NULL, NULL, &p);
    CHECK(p.len == 3 && p.nodes[1] == 2);
    path_free(&p);
    graph_free(&g);
}

static void test_save_load(void) {
    const char *file = "test_map_tmp.txt";
    Graph g;
    graph_init(&g);
    build_sample_map(&g);
    graph_set_closed(&g, graph_find(&g, "B"), graph_find(&g, "C"), true);
    CHECK(graph_save(&g, file));

    Graph h;
    graph_init(&h);
    CHECK(graph_load(&h, file));
    CHECK(h.loc_count == g.loc_count);
    CHECK(h.road_count == g.road_count);
    CHECK(h.roads[graph_find_road(&h, graph_find(&h, "B"), graph_find(&h, "C"))].closed);

    Path p1, p2;
    dijkstra_path(&g, 0, 5, METRIC_DISTANCE, NULL, NULL, &p1);
    dijkstra_path(&h, 0, 5, METRIC_DISTANCE, NULL, NULL, &p2);
    CHECK_NEAR(p1.cost, p2.cost);
    path_free(&p1);
    path_free(&p2);

    graph_free(&g);
    graph_free(&h);
    remove(file);
}

int main(void) {
    test_heap();
    test_graph_rules();
    test_shortest_and_bruteforce();
    test_bfs_dfs();
    test_yen();
    test_closures();
    test_time_metric();
    test_save_load();
    printf("%d checks passed, %d failed\n", passes, failures);
    return failures ? 1 : 0;
}
