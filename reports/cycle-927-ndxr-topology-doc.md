# Cycle 927 — documentation topologie NDXR

Le README documente désormais le décodage big-endian sans repacking, la
conservation des markers `0xFFFF` et la publication de `TriangleStripRestart`
ou `TriangleList` selon le flux observé.

Validation : CTest normal et sanitizer déjà verts; paquet régénérable sans
asset retail embarqué.
