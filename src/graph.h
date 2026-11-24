/* Road network: locations are nodes, roads are weighted edges (adjacency lists). */
#ifndef GRAPH_H
#define GRAPH_H

#include <stdbool.h>

#define NAME_LEN 64
#define AVERAGE_SPEED_KMH 50.0

typedef enum { METRIC_DISTANCE, METRIC_TIME } Metric;

typedef struct {
    char id[NAME_LEN];
    char name[NAME_LEN];
} Location;

typedef struct {
    int    a, b;          /* location indices                 */
    double distance_km;
    double time_min;
    char   name[NAME_LEN];
    bool   closed;
    bool   two_way;       /* two-way roads are stored in both adjacency lists */
} Road;

typedef struct {
    int to;               /* destination location index */
    int road;             /* index into Graph.roads     */
} Edge;

typedef struct {
    Edge *items;
    int count;
    int cap;
} EdgeList;

typedef struct {
    Location *locs;   int loc_count, loc_cap;
    Road     *roads;  int road_count, road_cap;
    EdgeList *adj;    /* adj[i] = outgoing edges of location i */
} Graph;

void   graph_init(Graph *g);
void   graph_free(Graph *g);
int    graph_find(const Graph *g, const char *id);                 /* -1 if missing */
int    graph_add_location(Graph *g, const char *id, const char *name); /* index or -1 */
int    graph_find_road(const Graph *g, int a, int b);              /* road index or -1 */
int    graph_add_road(Graph *g, int a, int b, double distance_km, double time_min,
                      const char *name, bool two_way);             /* index or -1 */
bool   graph_set_closed(Graph *g, int a, int b, bool closed);
double road_cost(const Road *r, Metric m);
bool   graph_save(const Graph *g, const char *path);
bool   graph_load(Graph *g, const char *path);

#endif
