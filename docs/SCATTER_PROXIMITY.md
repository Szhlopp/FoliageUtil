# Keep scattered foliage attached

`scatter` can check each proposed pivot against another mesh's triangle surface.
This is useful when path scatter, offsets, imported prototypes or sparse branch
geometry could leave foliage detached from its visible support.

```json
{
  "leaf_sites": {
    "op": "scatter",
    "input": "branch_paths",
    "count": 5,
    "range": [0.22, 1],
    "proximity_mesh": "branch_mesh",
    "max_distance": 0.002,
    "snap_to_mesh": true
  }
}
```

| Control | Behavior |
| --- | --- |
| `proximity_mesh` | Optional mesh-node reference. Its presence enables filtering. |
| `max_distance` | Maximum distance in meters to a triangle surface, inclusive. Default 0.005; bounds 0..10000. |
| `snap_to_mesh` | False retains accepted positions. True moves them to the nearest point on the surface. Default false. |

The check runs after each candidate's scatter offset, for path, mesh and ground
scatter alike. Candidates beyond the limit are rejected even when snapping is
enabled. No retries refill the count, so `count` is the number of attempted points
(per stem for path inputs). An empty support rejects all candidates. An empty
result is allowed inside the graph, but an exported mesh must still have faces.

Distances use the current graph coordinates and the nearest point on triangle
faces, edges or corners. A bounding-volume hierarchy accelerates the queries.
This is unsigned surface distance, not containment or a collision solver. Texture
alpha and the eventual instanced mesh are not sampled. Non-default proximity
settings require a `proximity_mesh`, and normal reference/type/cycle validation
also applies to that dependency, including on unused nodes.

Snapping preserves orientation, scale and phase. It does not align the leaf normal
to the support. Each candidate consumes its original random draws before filtering,
so rejecting one does not move later scatter candidates. The surviving set is
compacted; downstream per-instance randomness is indexed by that surviving order.
Generation budgets charge all attempted points, including rejected candidates.

The `--json` per-node report includes:

```json
"proximity": {
  "candidates": 320,
  "accepted": 320,
  "rejected": 0,
  "snapped": 320,
  "max_distance": 0.002
}
```

This is the first bamboo prototype's leaf scatter. The [bamboo recipe](../samples/graphs/bamboo.json)
uses its visible branch tubes as support, snaps leaf bases within 2 mm, and adds
a short sheath joining each blade to its attachment. Other culm prototypes use
the same controls before the complete plants are scattered into the grove.

Keep the leaf base or card pivot at the attachment origin. Transparent padding
inside a sprite can make the visible leaf start away from that origin even when
the pivot is exactly on a branch. Likewise, very thin supporting twigs can vanish
at render distance. Check both the mesh connections and the actual textured render.
