#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "graph.h"

void graph_init(Graph *g) {
    memset(g, 0, sizeof *g);
}

void graph_free(Graph *g) {
    for (int i = 0; i < g->loc_count; i++) free(g->adj[i].items);
    free(g->adj);
    free(g->locs);
    free(g->roads);
    graph_init(g);
}

static void edges_add(EdgeList *l, int to, int road) {
    if (l->count == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 4;
        l->items = realloc(l->items, (size_t)l->cap * sizeof *l->items);
    }
    l->items[l->count++] = (Edge){to, road};
}

int graph_find(const Graph *g, const char *id) {
    for (int i = 0; i < g->loc_count; i++)
        if (strcmp(g->locs[i].id, id) == 0) return i;
    return -1;
}

int graph_add_location(Graph *g, const char *id, const char *name) {
    if (id[0] == '\0' || strlen(id) >= NAME_LEN || strlen(name) >= NAME_LEN) return -1;
    if (graph_find(g, id) >= 0) return -1;
    if (g->loc_count == g->loc_cap) {
        g->loc_cap = g->loc_cap ? g->loc_cap * 2 : 8;
        g->locs = realloc(g->locs, (size_t)g->loc_cap * sizeof *g->locs);
        g->adj  = realloc(g->adj,  (size_t)g->loc_cap * sizeof *g->adj);
    }
    int idx = g->loc_count++;
    snprintf(g->locs[idx].id, NAME_LEN, "%s", id);
    snprintf(g->locs[idx].name, NAME_LEN, "%s", name);
    g->adj[idx].items = NULL;
    g->adj[idx].count = 0;
    g->adj[idx].cap = 0;
    return idx;
}

int graph_find_road(const Graph *g, int a, int b) {
    const EdgeList *l = &g->adj[a];
    for (int i = 0; i < l->count; i++)
        if (l->items[i].to == b) return l->items[i].road;
    return -1;
}

int graph_add_road(Graph *g, int a, int b, double distance_km, double time_min,
                   const char *name, bool two_way) {
    if (a < 0 || b < 0 || a >= g->loc_count || b >= g->loc_count || a == b) return -1;
    if (distance_km <= 0) return -1;
    if (time_min <= 0) time_min = distance_km / AVERAGE_SPEED_KMH * 60.0;
    if (graph_find_road(g, a, b) >= 0) return -1;                 /* duplicate */
    if (two_way && graph_find_road(g, b, a) >= 0) return -1;      /* duplicate reverse */

    if (g->road_count == g->road_cap) {
        g->road_cap = g->road_cap ? g->road_cap * 2 : 8;
        g->roads = realloc(g->roads, (size_t)g->road_cap * sizeof *g->roads);
    }
    int idx = g->road_count++;
    Road *r = &g->roads[idx];
    r->a = a;
    r->b = b;
    r->distance_km = distance_km;
    r->time_min = time_min;
    snprintf(r->name, NAME_LEN, "%s", name ? name : "");
    r->closed = false;
    r->two_way = two_way;

    edges_add(&g->adj[a], b, idx);
    if (two_way) edges_add(&g->adj[b], a, idx);
    return idx;
}

bool graph_set_closed(Graph *g, int a, int b, bool closed) {
    int idx = graph_find_road(g, a, b);
    if (idx < 0) return false;
    g->roads[idx].closed = closed;
    return true;
}

double road_cost(const Road *r, Metric m) {
    return m == METRIC_DISTANCE ? r->distance_km : r->time_min;
}

/* ---------- persistence: tab-separated text ----------
 *   L <id> <name>
 *   R <idA> <idB> <km> <min> <two_way> <closed> <name>
 */
bool graph_save(const Graph *g, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return false;
    for (int i = 0; i < g->loc_count; i++)
        fprintf(f, "L\t%s\t%s\n", g->locs[i].id, g->locs[i].name);
    for (int i = 0; i < g->road_count; i++) {
        const Road *r = &g->roads[i];
        fprintf(f, "R\t%s\t%s\t%.6f\t%.6f\t%d\t%d\t%s\n",
                g->locs[r->a].id, g->locs[r->b].id, r->distance_km, r->time_min,
                r->two_way ? 1 : 0, r->closed ? 1 : 0, r->name);
    }
    return fclose(f) == 0;
}

/* Splits s in place on tabs; empty fields are preserved. */
static int split_tabs(char *s, char **out, int max) {
    int n = 0;
    out[n++] = s;
    for (char *p = s; *p && n < max; p++) {
        if (*p == '\t') {
            *p = '\0';
            out[n++] = p + 1;
        }
    }
    return n;
}

bool graph_load(Graph *g, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return false;

    Graph tmp;
    graph_init(&tmp);
    char line[512];
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue;
        char *fields[8];
        int n = split_tabs(line, fields, 8);
        if (n >= 3 && strcmp(fields[0], "L") == 0) {
            if (graph_add_location(&tmp, fields[1], fields[2]) < 0) goto fail;
        } else if (n >= 8 && strcmp(fields[0], "R") == 0) {
            int a = graph_find(&tmp, fields[1]);
            int b = graph_find(&tmp, fields[2]);
            int idx = graph_add_road(&tmp, a, b, atof(fields[3]), atof(fields[4]),
                                     fields[7], atoi(fields[5]) != 0);
            if (idx < 0) goto fail;
            tmp.roads[idx].closed = atoi(fields[6]) != 0;
        } else {
            goto fail;
        }
    }
    fclose(f);
    graph_free(g);
    *g = tmp;
    return true;

fail:
    fclose(f);
    graph_free(&tmp);
    return false;
}
