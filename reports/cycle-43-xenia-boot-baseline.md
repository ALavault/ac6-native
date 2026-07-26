# Cycle 43 — baseline Xenia AC6 restauré et borné

## Exécution

Le binaire Xenia Canary a été vérifié directement dans son archive locale avant
extraction : son SHA-256 est
`98559834c570d4be8ba5d532f000aadf8ea6cf4d495be34a02b7ae766134007c`.
L'archive compressée a une identité séparée et n'est pas substituée au binaire.

```sh
XENIA_AC6_CAPTURE_SECONDS='8 20' \
  workspaces/ace-combat-6/scripts/run_xenia_ac6_oracle_baseline.sh \
  .build/xenia-ac6-cycle-43-short-20260716T090000Z
```

Le manifeste JSON de sortie a pour SHA-256
`9f875c280740965967e88628d0270cfd333fe0698e9fed950573ad6a4cb85f67`.
Il lie le XEX PAL
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
aux captures 8 s et 20 s. Celles-ci sont byte-identiques, chacune SHA-256
`42fa11d06dad4ca1a1793a84882aa8a1fcb6eaeeb7373dbec443d2150e448a6b`
(comparaison ImageMagick : 0 pixels différents).

Le journal atteint `KernelState: Launching module...` et résout `\default.xex`.

## Statut

`blocked-oracle` pour toute preuve de scène, handle ressource ou frame Xenos :
le lancement du module est observable, mais aucune image retail distincte n'est
produite à cet horizon. L'échec d'initialisation audio SDL est enregistré comme
limite d'environnement, non comme cause prouvée de l'écran noir. Aucun XEX,
shader, save state ou entrée n'est modifié.

Prochaine action : une condition de progression Xenia attestée par log ou
instrumentation XenonTests avant d'étendre les captures ; ne pas transformer
un timeout noir en preuve de stabilité des handles `0x821b9408`.
