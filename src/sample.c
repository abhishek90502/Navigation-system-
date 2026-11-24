#include <stddef.h>
#include "sample.h"

typedef struct {
    const char *a, *b;
    double distance_km;
    double time_min;   /* 0 = derive from distance at 50 km/h */
    const char *name;
} SampleRoad;

void build_sample_map(Graph *g) {
    static const char *ids[]   = {"A", "B", "C", "D", "E", "F", "G", "H"};
    static const char *names[] = {"Central Station", "Market Square", "Library", "Hospital",
                                  "Stadium", "Airport", "University", "Harbor"};
    for (int i = 0; i < 8; i++) graph_add_location(g, ids[i], names[i]);

    static const SampleRoad roads[] = {
        {"A", "B", 2.0, 0.0, "Main St"},
        {"A", "C", 4.5, 0.0, "Park Ave"},
        {"B", "C", 1.0, 0.0, "Oak Rd"},
        {"B", "D", 3.0, 0.0, "Elm St"},
        {"C", "E", 2.5, 0.0, "River Rd"},
        {"D", "E", 1.5, 0.0, "Hill Rd"},
        {"D", "G", 2.0, 0.0, "Ring Rd"},
        {"C", "G", 3.5, 0.0, "Bridge Ave"},
        {"E", "F", 6.0, 4.0, "Highway 9"},
        {"G", "F", 5.0, 0.0, "Airport Rd"},
        {"E", "H", 4.0, 0.0, "Harbor Way"},
        {"G", "H", 2.5, 0.0, "Dock St"},
    };
    for (size_t i = 0; i < sizeof roads / sizeof roads[0]; i++) {
        const SampleRoad *r = &roads[i];
        graph_add_road(g, graph_find(g, r->a), graph_find(g, r->b),
                       r->distance_km, r->time_min, r->name, true);
    }
}
