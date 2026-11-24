#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "heap.h"
#include "routing.h"

void path_free(Path *p) {
    free(p->nodes);
    p->nodes = NULL;
    p->len = 0;
    p->cost = 0.0;
}

/* Walks the parent array back from dst to src and fills out->nodes. */
static void build_path(const int *parent, int src, int dst, Path *out) {
    int len = 1;
    for (int x = dst; x != src; x = parent[x]) len++;
    out->nodes = malloc((size_t)len * sizeof(int));
    out->len = len;
    int x = dst;
    for (int i = len - 1; i >= 0; i--) {
        out->nodes[i] = x;
        x = parent[x];
    }
}

static void set_trivial(int src, Path *out) {
    out->nodes = malloc(sizeof(int));
    out->nodes[0] = src;
    out->len = 1;
    out->cost = 0.0;
}

bool bfs_path(const Graph *g, int src, int dst, Path *out) {
    if (src == dst) { set_trivial(src, out); return true; }
    int n = g->loc_count;
    int *parent = malloc((size_t)n * sizeof *parent);
    int *queue  = malloc((size_t)n * sizeof *queue);
    bool *seen  = calloc((size_t)n, sizeof *seen);
    for (int i = 0; i < n; i++) parent[i] = -1;

    int head = 0, tail = 0;
    queue[tail++] = src;
    seen[src] = true;
    bool found = false;

    while (head < tail && !found) {
        int u = queue[head++];
        const EdgeList *l = &g->adj[u];
        for (int e = 0; e < l->count && !found; e++) {
            Edge ed = l->items[e];
            if (g->roads[ed.road].closed || seen[ed.to]) continue;
            seen[ed.to] = true;
            parent[ed.to] = u;
            if (ed.to == dst) found = true;
            else queue[tail++] = ed.to;
        }
    }
    if (found) {
        build_path(parent, src, dst, out);
        out->cost = out->len - 1;   /* hops */
    }
    free(parent); free(queue); free(seen);
    return found;
}

bool dfs_path(const Graph *g, int src, int dst, Path *out) {
    if (src == dst) { set_trivial(src, out); return true; }
    int n = g->loc_count;
    int *parent = malloc((size_t)n * sizeof *parent);
    int *stack  = malloc((size_t)n * sizeof *stack);
    bool *seen  = calloc((size_t)n, sizeof *seen);
    for (int i = 0; i < n; i++) parent[i] = -1;

    int sp = 0;
    stack[sp++] = src;
    seen[src] = true;
    bool found = false;

    while (sp > 0 && !found) {
        int u = stack[--sp];
        if (u == dst) { found = true; break; }
        const EdgeList *l = &g->adj[u];
        for (int e = 0; e < l->count; e++) {
            Edge ed = l->items[e];
            if (g->roads[ed.road].closed || seen[ed.to]) continue;
            seen[ed.to] = true;
            parent[ed.to] = u;
            stack[sp++] = ed.to;
        }
    }
    if (found) {
        build_path(parent, src, dst, out);
        out->cost = out->len - 1;
    }
    free(parent); free(stack); free(seen);
    return found;
}

/* Dijkstra with a binary min-heap and lazy deletion.
 * banned_node[v]   : v may not be entered at all.
 * banned_target[v]: the edge src -> v is forbidden (used by Yen's algorithm). */
bool dijkstra_path(const Graph *g, int src, int dst, Metric m,
                   const bool *banned_node, const bool *banned_target, Path *out) {
    int n = g->loc_count;
    double *dist = malloc((size_t)n * sizeof *dist);
    int    *parent = malloc((size_t)n * sizeof *parent);
    bool   *done = calloc((size_t)n, sizeof *done);
    for (int i = 0; i < n; i++) { dist[i] = INFINITY; parent[i] = -1; }

    MinHeap h;
    heap_init(&h);
    long seq = 0;
    dist[src] = 0.0;
    heap_push(&h, 0.0, seq++, src);

    while (!heap_empty(&h)) {
        HeapItem it = heap_pop(&h);
        int u = it.value;
        if (done[u]) continue;          /* stale entry */
        done[u] = true;
        if (u == dst) break;

        const EdgeList *list = &g->adj[u];
        for (int e = 0; e < list->count; e++) {
            Edge ed = list->items[e];
            const Road *r = &g->roads[ed.road];
            int v = ed.to;
            if (r->closed) continue;
            if (banned_node && banned_node[v]) continue;
            if (u == src && banned_target && banned_target[v]) continue;
            double nd = dist[u] + road_cost(r, m);
            if (nd < dist[v]) {
                dist[v] = nd;
                parent[v] = u;
                heap_push(&h, nd, seq++, v);
            }
        }
    }

    bool found = done[dst];
    if (found) {
        build_path(parent, src, dst, out);
        out->cost = dist[dst];
    }
    heap_free(&h);
    free(dist);
    free(parent);
    free(done);
    return found;
}

double path_cost(const Graph *g, const int *nodes, int len, Metric m) {
    double total = 0.0;
    for (int i = 0; i + 1 < len; i++) {
        int road = graph_find_road(g, nodes[i], nodes[i + 1]);
        total += road_cost(&g->roads[road], m);
    }
    return total;
}

void route_totals(const Graph *g, const int *nodes, int len, double *dist, double *time) {
    *dist = 0.0;
    *time = 0.0;
    for (int i = 0; i + 1 < len; i++) {
        const Road *r = &g->roads[graph_find_road(g, nodes[i], nodes[i + 1])];
        *dist += r->distance_km;
        *time += r->time_min;
    }
}

static bool same_prefix(const int *a, const int *b, int len) {
    return memcmp(a, b, (size_t)len * sizeof(int)) == 0;
}

static bool path_seen(const Path *list, int count, const Path *p) {
    for (int i = 0; i < count; i++)
        if (list[i].nodes && list[i].len == p->len &&
            memcmp(list[i].nodes, p->nodes, (size_t)p->len * sizeof(int)) == 0)
            return true;
    return false;
}

/* Yen's algorithm. `accepted` must have room for k paths.
 * Returns the number of paths found (<= k); caller frees each accepted[i]. */
int yen_k_shortest(const Graph *g, int src, int dst, int k, Metric m, Path *accepted) {
    if (k < 1) return 0;
    if (!dijkstra_path(g, src, dst, m, NULL, NULL, &accepted[0])) return 0;

    int na = 1;
    int n = g->loc_count;
    bool *ban_node   = malloc((size_t)n * sizeof *ban_node);
    bool *ban_target = malloc((size_t)n * sizeof *ban_target);
    Path *cand = NULL;
    int nc = 0, capc = 0;
    MinHeap h;
    heap_init(&h);
    long seq = 0;

    while (na < k) {
        const Path *prev = &accepted[na - 1];
        for (int i = 0; i < prev->len - 1; i++) {
            int spur = prev->nodes[i];
            memset(ban_node, 0, (size_t)n * sizeof *ban_node);
            memset(ban_target, 0, (size_t)n * sizeof *ban_target);

            /* root = prev[0..i]; its nodes except the spur node are off-limits */
            for (int j = 0; j < i; j++) ban_node[prev->nodes[j]] = true;
            /* forbid the next hop of any accepted path that shares this root */
            for (int a = 0; a < na; a++) {
                const Path *p = &accepted[a];
                if (p->len > i + 1 && same_prefix(p->nodes, prev->nodes, i + 1))
                    ban_target[p->nodes[i + 1]] = true;
            }

            Path spur_path;
            if (!dijkstra_path(g, spur, dst, m, ban_node, ban_target, &spur_path)) continue;

            Path total;
            total.len = i + spur_path.len;
            total.nodes = malloc((size_t)total.len * sizeof(int));
            memcpy(total.nodes, prev->nodes, (size_t)i * sizeof(int));
            memcpy(total.nodes + i, spur_path.nodes, (size_t)spur_path.len * sizeof(int));
            path_free(&spur_path);
            total.cost = path_cost(g, total.nodes, total.len, m);

            if (path_seen(accepted, na, &total) || path_seen(cand, nc, &total)) {
                path_free(&total);
                continue;
            }
            if (nc == capc) {
                capc = capc ? capc * 2 : 8;
                cand = realloc(cand, (size_t)capc * sizeof *cand);
            }
            cand[nc] = total;
            heap_push(&h, total.cost, seq++, nc);
            nc++;
        }
        if (heap_empty(&h)) break;

        HeapItem it = heap_pop(&h);
        accepted[na++] = cand[it.value];
        cand[it.value].nodes = NULL;     /* ownership moved to accepted[] */
        cand[it.value].len = 0;
    }

    for (int i = 0; i < nc; i++) path_free(&cand[i]);
    free(cand);
    heap_free(&h);
    free(ban_node);
    free(ban_target);
    return na;
}
