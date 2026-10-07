# Staged growth snapshots

Growth supports overlapping schedules in two modes. The default `"mode":"scale"`
preserves existing recipes. Use `"mode":"developmental"` for extending stems,
independent thickening, and offspring that emerge when their support reaches them.

## Developmental growth

See [development-tree.json](../samples/graphs/development-tree.json) for baby leaves,
or [sakura-growth.json](../samples/graphs/sakura-growth.json) for overlapping flowering
shoots. A root configuration for the development-tree nodes is:

```json
"growth": {
  "mode": "developmental",
  "steps": 17,
  "stages": {
    "trunk": {
      "start": 0, "end": 1,
      "length_profile": [[0, 0], [0.65, 1], [1, 1]],
      "radius_profile": [[0, 0.08], [0.4, 0.35], [1, 1]]
    },
    "limbs": {"start": 0.12, "end": 1, "attachment_delay": 0.02},
    "twigs": {"start": 0.2, "end": 1, "attachment_delay": 0.015},
    "leaves": {
      "start": 0.28, "end": 1, "attachment_delay": 0.02,
      "scale_profile": [[0, 0], [0.16, 0.12], [0.4, 0.3], [0.75, 0.85], [1, 1]]
    }
  }
}
```

Here the trunk reaches its full length at 65% while continuing to thicken until
100%. Lower branches can already be extending, with baby leaves on their reached
sections, while the trunk and upper branches are still extending.

### Stem geometry and thickness

Developmental skeletons retain their seeded mature guide paths. `tube` and `ribbon`
reveal a growing arc-length prefix, with an interpolated advancing tip. Established
centerline positions stay fixed. Tube longitudinal UVs stay anchored to distance
from the base; packed ribbons reveal the corresponding prefix of their atlas cell.
Run `grow` before attaching children, as in the mature recipe, so the full guide
already contains the intended hanging shape. Roots extend over their authored
ground-shaped paths.

`length_profile` and `radius_profile` are separate stage channels. Each contains
2..32 `[local_age, fraction]` keys. Ages strictly increase from 0 to 1; fractions
stay in 0..1 and never decrease. Both finish at fraction 1. Length starts at zero;
radius may start above zero to give an emerging stem some initial thickness.
Defaults are `[[0,0],[0.75,1],[1,1]]` and `[[0,0.04],[1,1]]`, respectively.
These temporal channels are separate from a node's spatial `radius_profile`.

A child uses its mature reference radius. Its developing radius fraction is
capped by the support's fraction at the attachment, using the smaller fraction
instead of multiplying parent and child fractions. Branches therefore strengthen
with their support without repeatedly collapsing their cross-sections through
several generations.

Young advancing tips taper over `tip_length` of the mature path (default 0.15,
range 0.001..1), reaching a `tip_radius` multiplier at the tip (default 0.12,
range 0.01..1). The extra taper fades out as the mature path is completed. Set
`tip_radius:1` to disable it. Mesh generation defers sub-resolution pieces and
avoids tiny partial rows that cannot be represented at the current float-coordinate
scale. It does not inflate geometry or remove the solid connectivity checks.

### Attachment-driven birth and baby phases

Branch and root birth waits until the supporting tip reaches the attachment's
normalized arc-length position. Each scatter point carries its own support-arrival
time to `orient`, `transform`, `merge` and `instance`. A stage's actual birth is
the later of that arrival and its declared `start`. `attachment_delay` adds a
fraction of the remaining interval before `end`, in 0..0.95. For example, arrival
0.3, end 1 and delay 0.1 give birth at 0.37.

Local age runs from that birth to the declared `end`. Later attachments have less
time to mature. Give the supporting length channel enough time to reach its tip
before offspring must finish. A stage whose support arrives at or after its end
fails during developmental evaluation with an explanatory error. The default
length channel reaches full length before age 1 to leave time for tip offspring.

`scale_profile` controls instance size around each attachment, allowing a slow
baby phase followed by expansion. It has the same monotone-key rules, starts at
zero and finishes at one; its default is `[[0,0],[1,1]]`. `easing:"smoothstep"`
eases local age before channel lookup; the default is linear. This resizes the
authored leaf, pod or flower prototype, rather than modelling bud opening.

Unstaged children of developing stems receive the default developmental schedule.
Unstaged instances on developing scatter points similarly mature after arrival.
A stage on points handles their size itself, so an unstaged instance does not
apply a second automatic growth factor. Explicit stages on both points and their
instances compose deliberately. Ordinary mesh stages retain their explicit pivot
scaling behavior; unrelated unstaged meshes and points evaluate normally.

Hidden path attachments retain their candidate slots, size/color streams and atlas
selection. For skeleton scatter with `proximity_mesh`, the mature support determines
candidate membership, then the currently grown support controls visibility and
snapping. The mature support query is cached per generation and charged against the
same limits. Mesh-area scatter does not acquire persistent surface correspondence
when its mesh changes. These guarantees concern seeded path attachments.
The proximity report's `accepted` count includes hidden candidate slots; its
developmental `visible` and `snapped` counts describe currently supported points.

Progress 1 produces the same mature mesh as an export without growth. The library's
developmental `Stem.samples` contain full guides with opaque `growth` metadata;
surface outputs contain only their reached portions. No skeleton or point metadata
is written as a runtime animation system in GLB.

```sh
./build/foliageutil out/samples/graphs/development-tree.json --out out/development-tree --no-lods --json
cmake --build build --target foliage_growth_frame --parallel 8
./build/foliage_growth_frame out/samples/graphs/sakura-growth.json --check-sequence 192 > out/sakura-sequence-check.json
```

The sequence check evaluates real geometry without writing repeated GLBs. It checks
2..2000 continuously spaced frames and records output counts, active development and
solid diagnostics. Preview representative frames before committing to a full render.

## Default scale mode

The optional root `growth` configuration schedules generic graph nodes:

```json
"growth": {
  "steps": 9,
  "stages": {
    "trunk": {"start": 0, "end": 0.7},
    "branches": {"start": 0.4, "end": 1},
    "leaves": {"start": 0.75, "end": 1, "easing": "smoothstep"}
  }
}
```

Stage keys must name actual graph nodes. `steps` is 2..32; the step index is
zero-based and progress is `index / (steps - 1)`. Each stage's factor is zero
through `start`, one from `end`, and interpolated in between. Start/end must be
ordered within 0..1; defaults are 0/1. `easing` is `linear` (default) or
`smoothstep`. Unlisted nodes evaluate normally. Graph dependencies carry their
parents' current shapes to their children.

- A skeleton stage scales each path from its own first sample, including its
  radius. Stage the trunk before branching from it so attachments move with it.
- An `instance` stage scales each copied leaf, pod, flower, card or cluster at its
  own placement pivot. It preserves seeded cell/size/color choices. This is the
  usual choice for attached organs.
- A points stage scales each placement's size while keeping its pivot and frame.
- Other mesh stages scale around `pivot:[x,y,z]`, defaulting to the origin. Stage
  an origin-based prototype or provide the intended pivot for translated meshes.
  Nonzero custom pivots are not accepted on skeleton, points or instance stages.

Stages on successive nodes compound. For example, a flower assembly can grow
at its stem attachment while individual petal instances develop on that assembly.
For hanging strands, stage the path before its final `grow` bend and before leaf
scattering. An optional stage on the `grow` output instead scales the already
bent path from its base. This complements the existing `grow` shape operation.

At factor zero, a staged node returns an empty value of the correct type. Growth
allows empty dependent scatter/solidify results to propagate. Empty final outputs
are listed as `empty_outputs` in the report and manifest, with no invalid empty
GLB/OBJ created. A stale file and its MTL at those exact generated paths are
removed. Texture folders and other files are retained. A graph is only completely
empty at zero if all its output geometry depends on stages that have not started.

```sh
./build/foliageutil growth --json
./build/foliageutil out/samples/graphs/growth-tree.json --out out/growth-tree --json
./build/foliageutil out/samples/graphs/growth-tree.json --out out/mature --no-growth
./build/foliageutil out/samples/graphs/growth-tree.json --out out/step --growth-step 7 --no-lods
./build/foliageutil out/samples/graphs/growth-tree.json --out out/step-low --growth-step 7 --lod LOD1
```

Normal CLI export writes the mature base, configured mature LODs, and growth
snapshots such as `growth-tree-G07.glb`. `--no-growth` skips snapshots, and
`--growth-step N` exports only the selected snapshot. `--no-lods` skips LODs.
Set `growth.lods:true` to export the configured LODs at each growth step too;
the default is false. A combined filename is `tree-LOD1-G07.glb`. A batch is
limited to 128 variants before any write. Each variant gets its own geometry
budget and is evaluated sequentially.

`growth.json` records step, progress, level, seed, bounds, counts, portable output
paths and empty outputs. A single-step export's manifest contains only that step.
The API accepts continuous `Options.growth` in 0..1 for `generate`. No growth
option means the ordinary mature asset. Progress 1 produces the exact mature
geometry and GLB bytes. `exportGraph` accepts LOD/growth enable flags and an
optional step index; consult [the public header](../include/foliage/foliage.hpp).

[Growth tree](../samples/graphs/growth-tree.json) starts branches at 40%, twigs at 55%,
and leaves at 75%. [Sunflower](../samples/graphs/sunflower.json) stages stem, petioles,
leaves, flower assembly, two petal rings and seeds separately.

These are independent procedural snapshots, not botanical simulation, skeletal
animation, morph targets or a guarantee of matching topology between steps.
Topology changes when parts appear. UVs remain attached; growth does not invent
bud-opening mechanics or interpolate material pigment. Stages on whole prototype
meshes scale their shape. Roots can move relative to a ground plane when scaled,
and union inputs must still intersect at each phase. Check authored intermediate
steps, especially very small stages and solid junctions. Growth does not repair
collisions, self-intersections or disconnected input geometry.


For a complete rendered animation, see the [sakura growth example](SAKURA.md#growth-animation).
Its optional `foliage_growth_frame` adapter samples the existing continuous API,
and `tools/render_growth_animation.py` creates a Blender mesh sequence and MP4.
The ordinary saved-step limit remains 32; continuous API sampling does not change
that export contract.

## Packaged growth exports

See [Growth animation and stage packages](EXPORTS.md) for native Alembic export
and independently generated geometry/CardBake stages. Named profiles select
their own progress samples and optional LOD. Each CardBake stage captures its
currently grown source and visible guide paths; empty foliage can retain wood
alone. These profiles run explicitly with `--export NAME`.
