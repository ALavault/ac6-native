# Cycle 919 — fixture NDXR big-endian avec primitive-restart

Une fixture binaire minimale couvre désormais le chemin réel NDXR : header
big-endian, format vertex `0x0613`, polygon descriptor, quatre vertices,
triangle-strip `[0,1,2,3,0xFFFF]`. Le test vérifie `NDXR_BE`, la publication de
`NativeIndexTopology::TriangleStripRestart` et la conservation du sentinel
`UINT32_MAX` dans le décodage interne.

Validations :

- CTest normal : 3/3 ;
- CTest ASan/UBSan avec SDL dummy : 3/3 ;
- CPack audit : `package_audit=pass entries=17`.
