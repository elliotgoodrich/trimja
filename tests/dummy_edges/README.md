# dummy_edges

We want to make sure that dummy edges are correctly injected when we want
to prioritize certain build commands.

Given the graph below,

    +----------+
    |     A    |
    |   /|\ \  |
    |  B | E F |
    |  | | | | |
    |  C | | G |
    |   \|/  | |
    |    D   H |
    |        | |
    |        I |
    +----------+

we have the following longest path lengths:


+---+---+
| A | 0 |
| B | 1 |
| C | 2 |
| D | 3 |
| E | 1 |
| F | 1 |
| G | 2 |
| H | 3 |
| I | 4 |
+---+---+

The build order would be {I} -> {D, H} -> {C, G} -> {B, E, F} -> {A}.

If we marked D as affected, we would like to build `D`, `C`, `B`, and `E` as a priority
compared to the `I` branch.  This is because it is more likey that one of these will
fail to build because of the change.

To do this we need to find the first node that has both affected and non-affected
children (this case it is the root `A`) and inject dummy edges between it and its
children so that all children have a longer path than all non-affected nodes.

Here we need to inject 1 edge between A-B and 2 edges between A-E in order to
make all affected edges have at least the length of the longest non-affected
edge (which is I at 4).
