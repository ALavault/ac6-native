# AC6 Xenia Edge agent bridge — cycle 2 A/B

Date d'observation : 2026-08-16 (Europe/Paris)

## Identité

- Cible : démo PAL AC6 `Default.xex`.
- SHA-256 XEX : `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`.
- Xenia Edge : checkout `e4b13738c3c461b2c06241fa3f54b5a669b6a304`.
- Binaire Debug Clang : SHA-256 `4b80ab4881aedaf31cf3cf12d0194e8fd6c4f3cd3c638e051932dbf82d68f018`.
- Identité journalisée : module `F8FFF5AD248AE8D9`, Media ID `565E01A0`, Title ID `4E4D87E6`.

## Protocole

- Deux stores temporaires neufs et séparés, même XEX, même binaire et même
  configuration.
- A : `agent_bridge=false`; B : `agent_bridge=true`.
- Aucun profil créé : les stores ne contiennent que le journal et le reçu de
  PRESENT.
- Précompilation anticipée, désassemblage, dumps HIR/XEX/shaders/GPU désactivés
  dans la copie temporaire de la configuration d'oracle.
- Le handler commun `XE_SWAP` émet un reçu borné aux 120 premiers PRESENT après
  retour de `IssueSwap` : compteur, adresse du frontbuffer, largeur et hauteur.

Artefacts temporaires :
`/fastdata/lavaulta/tmp/ac6-xenia-edge-ab2.xDZhlf/{off3,on3}/`.

## Résultat

| Mesure | OFF | ON |
|---|---:|---:|
| Reçus comparés | 120 | 120 |
| Frontbuffer | `0x1A9A0000` | `0x1A9A0000` |
| Dimensions | 1280×720 | 1280×720 |
| SHA-256 des reçus normalisés | `28fa03ce0afa6b4bd1a00f6f685935e3ca2080a27207cfe95260dbcbe4f323a5` | identique |
| Comparaison byte-à-byte | PASS | PASS |

Le bridge activé sans action injectée est donc passif sur la séquence de
frontbuffers observée pendant les 120 premiers PRESENT AC6.

## Validations

- `xenia-base-tests '[agent_bridge]'` : 3 cas, 27 assertions, PASS.
- Build complet `xenia_edge` Debug Clang : PASS.
- Reçu OFF/ON normalisé : `cmp`, PASS.
- Aucun processus Xenia Edge résiduel après le cycle.

## Limites et prochain checkpoint

- Ce gate ne compare pas les pixels, l'audio, les entrées HID ni l'état mémoire
  invité ; il qualifie uniquement le boundary PRESENT et sa passivité sans
  action.
- Les deux runs Debug rencontrent après la fenêtre utile une assertion Snappy
  amont (`snappy.cc:914`). Elle n'est pas attribuée au bridge et empêche de
  promouvoir ce cycle en test longue durée.
- Le bridge ne sait pas encore suspendre atomiquement après PRESENT, capturer le
  framebuffer pendant la pause, ni restaurer un checkpoint. Ces capacités
  restent annoncées `false` et doivent être le prochain checkpoint avant la
  boucle vision → décision → contrôle.
