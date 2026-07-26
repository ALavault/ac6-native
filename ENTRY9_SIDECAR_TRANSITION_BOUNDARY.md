# DATA.TBL entry-9 Scene-sidecar transition boundary

Date: 2026-07-15

## Result

The DPL9 Scene sidecars close a local asset relation, not a directed
post-CUT action.  No static next-scene, mission-complete, or completion
consumer can be derived from the selected group `22.1.0` without inventing
campaign order.

## What the sidecars encode

Every one of the 44 structurally valid Scene groups has exactly this immediate
FHM layout:

```
member N      NFICCUT state payload
member N + 1  resource FHM
member N + 2  fixed 0x80-byte Scene-path records
```

For a given group, path record `i` names only resource-FHM member `i`.  The
native implementation validates this cardinality equality before returning a
group, and `resolve_scene_resource` refuses an index outside that local pair.
The path records themselves are NUL-terminated `Scene/...` strings; there is
no parsed group identifier, event target, selector, or next-member field.

For the selected group, the relation is fully concrete but remains local:

```
22.1.0.0      NFICCUT (39,352 bytes)
22.1.0.1.0    record 0 resource, Tcam__c01.mop (4,656 bytes)
22.1.0.2[0]   Scene/dd01_01a/dd01_01a_01/Tcam__c01.mop
```

The remaining records in `22.1.0.2` likewise resolve only to
`22.1.0.1.1` through `22.1.0.1.17`; they do not name `22.1.1`, any other
Scene group, or an executable resource request.  The complete 553-row
mapping is retained in `reports/entry9-scene-path-resolution.csv`.

## Completion evidence does not supply a successor

The selected NFIC event stream has a named `CutTerminate` (`0x8004`) boundary.
The static XEX parser/dispatcher stops the current stream at that boundary;
its event receiver is dynamically supplied through vtable slot `+0x20`.
Neither the sidecar join nor that parser/dispatcher yields a statically typed
consumer that requests another Scene group.

In particular, `22.1.1` being adjacent in the archive is not a successor
edge.  Treating it as one would confuse FHM enumeration order with retail
campaign control flow.

## Native consequence

The Linux scene shell correctly remains in `scene_complete` after the decoded
`CutTerminate` and final presentation sample.  It must not automatically
select `22.1.1`, spawn a player, or start flight mode based on these sidecars.

## Required next edge

One of the following is needed to extend the executable beyond local CUT
completion:

1. an executable consumer of `CutTerminate` that issues a typed resource or
   mode request;
2. an executable mapping from a campaign selector/mission state to a specific
   Scene-group identity; or
3. a recovered runtime receiver that converts completion into such a request.

Until then, this is a closed negative result: the data is sufficient for
bounded Scene asset resolution and playback, but not for post-CUT progression.

## Evidence

- `reconstruction/ace-combat-6/include/ac6/scene.h`;
- `reconstruction/ace-combat-6/src/scene.cpp`;
- `ENTRY9_SCENE_RESOURCE_RESOLUTION_REPORT.md`;
- `ENTRY9_TCAM_NFIC_CUT_REPORT.md`;
- `NFIC_XEX_EVENT_CONSUMER_REPORT.md`.
