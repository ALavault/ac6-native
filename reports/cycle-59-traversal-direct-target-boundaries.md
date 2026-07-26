# Cycle 59 — AC6 : corps corrigés des cibles directes de traversal

## Identité et méthode

- cible : AC6 PAL `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- projet : `ace-combat-6-corrected` ;
- méthode : `analyzeHeadless -noanalysis` avec `InspectFunctionIsland.java` et
  `DecompileAt.java`, sans renommage, type, patch ni sortie XenonRecomp.

Les exports plats antérieurs classaient les trois appels directs de
`0x8226ecb0` comme des corps tronqués ou `noreturn`, car ils se terminaient sur
des sous-entrées d'aide ABI `0x82382ef*`. Le gestionnaire de fonctions du
projet corrigé donne les corps suivants :

| Cible directe | Corps corrigé | Instructions | Conclusion sûre |
| --- | --- | ---: | --- |
| `0x82227378` | `0x82227378..0x8222740f` | 38 | corps réel, itère des enregistrements et appelle `0x822272d8` ; ABI de l'appel depuis traversal non résolue |
| `0x8222b740` | `0x8222b740..0x8222b773` | 13 | corps réel court ; dépend de registres non récupérés (`r26/r28/r29/r30/r31`) |
| `0x8222ccd0` | `0x8222ccd0..0x8222ce0b` | 79 | corps réel, écrit des champs `+0x830..+0x86c` d'un objet implicite ; aucun rôle gameplay n'est déduit |

## Effet sur la transcription native

La traduction de `0x8226ecb0` conserve les trois appels comme callbacks
bornés avec offsets retail (`+0x10`, `+0x24f8`, `+0x254c`). Il serait erroné de
remplacer ces callbacks par les pseudo-prototypes Ghidra : les corps utilisent
des registres non attribués et l'appel-site traversal n'établit pas encore la
convention complète, le propriétaire ni les champs de contexte.

Ce résultat améliore le classement de la preuve de `heuristic` à
`cross-match` pour l'existence des corps, mais laisse les prototypes et la
sémantique à `unknown`. Il ne prouve ni MissionAircraft, ni spawn, ni une
activation de mission.

## Contrôle du conteneur d'appel

La même décompilation headless de `0x8226ecb0` dépasse la limite de redémarrages
du décompilateur et introduit des registres non attribués ; son appelant
`0x8226a310` est réduit à un faux leaf qui retourne `in_r11`. Ces sorties ne
constituent donc pas une source de prototype. Elles confirment que les offsets
et les appels PPC déjà extraits de l'assembleur restent plus fiables que le
pseudo-code pour cette frontière.

## Prochaine preuve

Capturer dans Xenia/XenonTests le contexte de registres et le pointeur objet
autour d'un seul appel de `0x8226ecb0`, puis comparer les mémoires écrites par
`0x8222ccd0`. Tant que ce contexte n'existe pas, les callbacks du runtime
natif restent la frontière correcte.
