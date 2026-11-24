# Google Maps-Like Navigation System (C)

A console navigation engine in C11 with no external dependencies. It covers a graph
model, BFS, DFS, Dijkstra with a hand-written binary min-heap, Yen's k-shortest
paths for alternative routes, and road closure simulation.

## Build and run

```bash
make                 # builds ./navmap
./navmap --demo      # sample city, interactive menu
make test            # builds and runs the unit tests
make clean
```

Requires any C11 compiler (gcc or clang).

## Layout

```
src/heap.[ch]        binary min-heap (sift up / sift down, lazy deletion support)
src/graph.[ch]       locations, roads, adjacency lists, text save/load
src/routing.[ch]     bfs_path, dfs_path, dijkstra_path, yen_k_shortest
src/sample.[ch]      built-in sample city
src/main.c           menu-driven CLI
tests/test_navigation.c   unit tests, including brute-force verification
```

## Concepts and complexity

| Algorithm | Time | Space | Notes |
|---|---|---|---|
| BFS | O(V + E) | O(V) | Fewest hops |
| DFS | O(V + E) | O(V) | Any path |
| Dijkstra + binary heap | O((V + E) log V) | O(V) | Non-negative weights |
| Yen's (k paths) | O(k · V · (E + V log V)) | O(k · V) | Dijkstra per spur node |

## C-specific design notes

- **Index-based graph.** Locations and roads live in dynamic arrays and reference each
  other by index. This avoids pointer invalidation when arrays grow with `realloc`.
- **Two-way roads** are stored as two edges that point to one shared `Road` record, so
  closing the road blocks both directions.
- **Heap items** are `{key, seq, value}`. The `seq` field breaks ties in insertion order.
- **Memory ownership.** Every `Path` returned by a routing function owns its `nodes`
  array, released with `path_free`. Yen's algorithm moves candidates into the accepted
  list rather than copying them.
- **Save format** is tab-separated text, so it is easy to read and diff:
  `L <id> <name>` for locations and `R <a> <b> <km> <min> <two_way> <closed> <name>` for roads.
