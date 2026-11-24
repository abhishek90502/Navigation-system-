/* Console front-end. Usage: ./navmap [--demo] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "graph.h"
#include "routing.h"
#include "sample.h"

#define MAX_ROUTES 10

static Graph G;

/* ---------- input helpers ---------- */
static void read_text(const char *msg, char *buf, size_t n) {
    printf("%s", msg);
    fflush(stdout);
    if (!fgets(buf, (int)n, stdin)) { printf("\nGoodbye!\n"); exit(0); }
    buf[strcspn(buf, "\r\n")] = '\0';
}

static int read_int(const char *msg, int def) {
    char buf[64];
    read_text(msg, buf, sizeof buf);
    return buf[0] ? atoi(buf) : def;
}

static double read_double(const char *msg) {
    char buf[64];
    for (;;) {
        read_text(msg, buf, sizeof buf);
        char *end;
        double v = strtod(buf, &end);
        if (end != buf && *end == '\0') return v;
        printf("  Please enter a number.\n");
    }
}

static bool read_yes(const char *msg, bool def) {
    char buf[32];
    read_text(msg, buf, sizeof buf);
    if (buf[0] == '\0') return def;
    return buf[0] == 'y' || buf[0] == 'Y';
}

static Metric read_metric(void) {
    char buf[32];
    read_text("Optimise by [d]istance or [t]ime? (default d): ", buf, sizeof buf);
    return (buf[0] == 't' || buf[0] == 'T') ? METRIC_TIME : METRIC_DISTANCE;
}

static int read_location(const char *label) {
    for (;;) {
        printf("%s [", label);
        for (int i = 0; i < G.loc_count; i++)
            printf("%s%s", i ? ", " : "", G.locs[i].id);
        printf("]: ");
        fflush(stdout);
        char buf[NAME_LEN];
        if (!fgets(buf, sizeof buf, stdin)) { printf("\nGoodbye!\n"); exit(0); }
        buf[strcspn(buf, "\r\n")] = '\0';
        int idx = graph_find(&G, buf);
        if (idx >= 0) return idx;
        printf("  Unknown location.\n");
    }
}

static const char *metric_label(Metric m) { return m == METRIC_TIME ? "Time" : "Distance"; }
static const char *metric_unit(Metric m)  { return m == METRIC_TIME ? "min" : "km"; }

static bool need_locations(int n) {
    if (G.loc_count >= n) return true;
    printf("  Add at least %d locations first.\n", n);
    return false;
}

/* ---------- output helpers ---------- */
static void print_route(const Path *p, const char *title, const char *label, const char *unit) {
    printf("\n%s\n", title);
    if (!p || p->len == 0) { printf("  No route found.\n"); return; }
    double dist, time;
    route_totals(&G, p->nodes, p->len, &dist, &time);
    printf("  Path: ");
    for (int i = 0; i < p->len; i++)
        printf("%s%s", i ? " -> " : "", G.locs[p->nodes[i]].name);
    printf("\n");
    if (strcmp(unit, "hops") == 0)
        printf("  Hops: %d | Distance: %.2f km | Time: %.1f min\n", (int)p->cost, dist, time);
    else
        printf("  %s: %.2f %s (distance %.2f km, time %.1f min)\n", label, p->cost, unit, dist, time);
}

/* ---------- menu commands ---------- */
static void cmd_show_map(void) {
    printf("\nLocations:\n");
    for (int i = 0; i < G.loc_count; i++)
        printf("  %s: %s\n", G.locs[i].id, G.locs[i].name);
    printf("Roads:\n");
    for (int i = 0; i < G.road_count; i++) {
        const Road *r = &G.roads[i];
        printf("  %s %s %s  %.1f km / %.1f min",
               G.locs[r->a].id, r->two_way ? "<->" : "-->", G.locs[r->b].id,
               r->distance_km, r->time_min);
        if (r->name[0]) printf(" (%s)", r->name);
        if (r->closed) printf("  [CLOSED]");
        printf("\n");
    }
}

static void cmd_add_location(void) {
    char id[NAME_LEN], name[NAME_LEN];
    do read_text("Short id (e.g. Z): ", id, sizeof id); while (id[0] == '\0');
    read_text("Display name: ", name, sizeof name);
    if (name[0] == '\0') strcpy(name, id);
    if (graph_add_location(&G, id, name) < 0)
        printf("  Could not add location (duplicate id or name too long).\n");
    else
        printf("  Added %s (%s).\n", name, id);
}

static void cmd_add_road(void) {
    if (!need_locations(2)) return;
    int a = read_location("From");
    int b = read_location("To");
    double dist = read_double("Distance (km): ");
    char buf[64];
    read_text("Travel time in minutes (blank = derive from 50 km/h): ", buf, sizeof buf);
    double t = buf[0] ? atof(buf) : 0.0;
    char name[NAME_LEN];
    read_text("Road name (optional): ", name, sizeof name);
    bool two_way = read_yes("Two-way? [Y/n]: ", true);

    if (graph_add_road(&G, a, b, dist, t, name, two_way) < 0)
        printf("  Could not add road (duplicate, same location, or invalid values).\n");
    else
        printf("  Road added.\n");
}

static void cmd_shortest(void) {
    if (!need_locations(2)) return;
    int s = read_location("Start"), d = read_location("Destination");
    Metric m = read_metric();
    Path p;
    bool ok = dijkstra_path(&G, s, d, m, NULL, NULL, &p);
    print_route(ok ? &p : NULL, "Shortest route (Dijkstra + min-heap):", metric_label(m), metric_unit(m));
    if (ok) path_free(&p);
}

static void cmd_bfs(void) {
    if (!need_locations(2)) return;
    int s = read_location("Start"), d = read_location("Destination");
    Path p;
    bool ok = bfs_path(&G, s, d, &p);
    print_route(ok ? &p : NULL, "Fewest-hops route (BFS, ignores weights):", "Hops", "hops");
    if (ok) path_free(&p);
}

static void cmd_dfs(void) {
    if (!need_locations(2)) return;
    int s = read_location("Start"), d = read_location("Destination");
    Path p;
    bool ok = dfs_path(&G, s, d, &p);
    print_route(ok ? &p : NULL, "Any route (DFS, not guaranteed shortest):", "Hops", "hops");
    if (ok) path_free(&p);
}

static void cmd_alternatives(void) {
    if (!need_locations(2)) return;
    int s = read_location("Start"), d = read_location("Destination");
    int k = read_int("How many routes? (default 3): ", 3);
    if (k < 1) k = 1;
    if (k > MAX_ROUTES) k = MAX_ROUTES;
    Metric m = read_metric();

    Path routes[MAX_ROUTES];
    int count = yen_k_shortest(&G, s, d, k, m, routes);
    if (count == 0) printf("\nNo route found.\n");
    for (int i = 0; i < count; i++) {
        char title[32];
        snprintf(title, sizeof title, "Option %d:", i + 1);
        print_route(&routes[i], title, metric_label(m), metric_unit(m));
        path_free(&routes[i]);
    }
}

static void cmd_simulate_closure(void) {
    if (!need_locations(2)) return;
    int s = read_location("Start"), d = read_location("Destination");
    printf("Now pick the road to simulate closing:\n");
    int a = read_location("  Road from");
    int b = read_location("  Road to");
    Metric m = read_metric();

    int ri = graph_find_road(&G, a, b);
    if (ri < 0) ri = graph_find_road(&G, b, a);
    if (ri < 0) {
        printf("  No road exists between those locations.\n");
        return;
    }

    Path before, after;
    bool had_before = dijkstra_path(&G, s, d, m, NULL, NULL, &before);

    bool was_closed = G.roads[ri].closed;
    G.roads[ri].closed = true;
    bool had_after = dijkstra_path(&G, s, d, m, NULL, NULL, &after);
    G.roads[ri].closed = was_closed;            /* always restore */

    print_route(had_before ? &before : NULL, "Before closure:", metric_label(m), metric_unit(m));
    print_route(had_after ? &after : NULL, "After closing that road:", metric_label(m), metric_unit(m));

    if (had_before && had_after) {
        bool same = before.len == after.len &&
                    memcmp(before.nodes, after.nodes, (size_t)before.len * sizeof(int)) == 0;
        if (same) {
            printf("\n  The closure does not affect the best route.\n");
        } else {
            double d1, t1, d2, t2;
            route_totals(&G, before.nodes, before.len, &d1, &t1);
            route_totals(&G, after.nodes, after.len, &d2, &t2);
            printf("\n  Detour adds %+.2f km and %+.1f min.\n", d2 - d1, t2 - t1);
        }
    } else if (had_before) {
        printf("\n  Destination becomes unreachable after this closure.\n");
    }
    printf("  (The road was restored to its original state.)\n");

    if (had_before) path_free(&before);
    if (had_after) path_free(&after);
}

static void cmd_toggle_road(void) {
    if (!need_locations(2)) return;
    int a = read_location("Road from");
    int b = read_location("Road to");
    char buf[32];
    read_text("[c]lose or [r]eopen? (default c): ", buf, sizeof buf);
    bool close = !(buf[0] == 'r' || buf[0] == 'R');
    int ri = graph_find_road(&G, a, b);
    if (ri < 0) ri = graph_find_road(&G, b, a);
    if (ri < 0) { printf("  No road exists between those locations.\n"); return; }
    G.roads[ri].closed = close;
    printf("  Road %s.\n", close ? "closed" : "reopened");
}

static void cmd_save(void) {
    char path[256];
    read_text("File path (default map.txt): ", path, sizeof path);
    if (path[0] == '\0') strcpy(path, "map.txt");
    printf(graph_save(&G, path) ? "  Saved to %s.\n" : "  Could not write %s.\n", path);
}

static void cmd_load(void) {
    char path[256];
    read_text("File path (default map.txt): ", path, sizeof path);
    if (path[0] == '\0') strcpy(path, "map.txt");
    if (graph_load(&G, path))
        printf("  Loaded %d locations and %d roads from %s.\n", G.loc_count, G.road_count, path);
    else
        printf("  Could not load %s (missing or malformed).\n", path);
}

static void print_menu(void) {
    printf("\n=== Navigation System (C) ===\n");
    printf(" 1) Show map\n");
    printf(" 2) Add location\n");
    printf(" 3) Add road\n");
    printf(" 4) Shortest route (Dijkstra)\n");
    printf(" 5) Fewest-hops route (BFS)\n");
    printf(" 6) Any route (DFS)\n");
    printf(" 7) Alternative routes (Yen's k-shortest)\n");
    printf(" 8) Simulate road closure\n");
    printf(" 9) Close / reopen a road\n");
    printf("10) Save map\n");
    printf("11) Load map\n");
    printf(" 0) Exit\n");
}

int main(int argc, char **argv) {
    graph_init(&G);
    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        build_sample_map(&G);
        printf("Sample city loaded.\n");
    }

    for (;;) {
        print_menu();
        char buf[32];
        read_text("Choose: ", buf, sizeof buf);
        switch (atoi(buf)) {
            case 0:  printf("Goodbye!\n"); graph_free(&G); return 0;
            case 1:  cmd_show_map(); break;
            case 2:  cmd_add_location(); break;
            case 3:  cmd_add_road(); break;
            case 4:  cmd_shortest(); break;
            case 5:  cmd_bfs(); break;
            case 6:  cmd_dfs(); break;
            case 7:  cmd_alternatives(); break;
            case 8:  cmd_simulate_closure(); break;
            case 9:  cmd_toggle_road(); break;
            case 10: cmd_save(); break;
            case 11: cmd_load(); break;
            default: printf("Unknown option.\n");
        }
    }
}
