# Group 5 timeline resource identities

Date: 2026-07-15

## Result

The group-5 metadata timeline can be associated with exact local Scene
resources beyond its visible `3/3/3` count overlay. The native shell JSON now
emits the selected Tcam MOP path, each resolved E_EFFMOVE resource in original
MoveEffect event order, and every active world resource whose independent
Rigid/AnimRigid-to-MDLP/NDXR join succeeded.

This is a provenance/reporting surface. It does not assign gameplay ownership,
effect behavior, renderer semantics, or a relation between an effect resource
and a displayed aircraft beyond their common serialized frame.

## Frame-1 identity snapshot

The exact `--scene-group 5 --capture-frame 1` JSON contains:

| Serialized role | Scene ID / index | Exact MOP path |
| --- | ---: | --- |
| Tcam | object 1 | `Scene/dd01_01a/dd01_01a_06/Tcam__c06.mop` |
| AnimRigid | object 3 | `Scene/dd01_01a/dd01_01a_06/Tlod__r_f16c_t1__01.mop` |
| AnimRigid | object 6 | `Scene/dd01_01a/dd01_01a_06/Tlod__r_f18f_t1__01.mop` |
| MoveEffect | ID 4 / Scene record 3 | `Scene/dd01_01a/dd01_01a_06/E_EFFMOVE_eff_dd_01a01tmp_rlf16c01.mop` |
| MoveEffect | ID 5 / Scene record 4 | `Scene/dd01_01a/dd01_01a_06/E_EFFMOVE_eff_dd_01a01tmp_rlf16c02.mop` |
| MoveEffect | ID 7 / Scene record 6 | `Scene/dd01_01a/dd01_01a_06/E_EFFMOVE_eff_dd_01a01tmp_rlf18f01.mop` |

All three effect resource rows report validated two-record metadata. Their
content IDs and opaque record counts are kept in the JSON as raw metadata;
the prior schedule audit proves they must not be read as duration or transform
fields.

## Capture association

The 35-second native WebM’s group-5 `3/3/3` overlay therefore corresponds to
the above three ordered E_EFFMOVE resource identities at its frame-1 snapshot.
The two aircraft MOP paths are separately exact active AnimRigid resources in
that same serialized frame. This is co-occurrence/provenance only: the data
does not prove that a particular effect is attached to either aircraft.

## Validation

The updated shell was built with `-j 16`; a dummy-SDL group-5 frame-1 capture
reported all six paths above, 220 Tcam samples, two active AnimRigid tracks,
and exactly three serialized/resolved/metadata-validated effect resources.
The focused Scene-shell, NFIC, MOP, and Scene tests pass.
