# AC6 retail NTSC-U/J — r429 — tampon frontal confirmé par motif de double-tamponnage, dimensions réelles obtenues en direct (1280×720) — correctif entièrement spécifié, PAS ENCORE appliqué (risque d'écriture au mauvais endroit de l'anneau)

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Suite de r428 (nommé pour r429) : confirmer si le mot `+20` de `r4`
pointe vers une vraie surface de rendu, décoder `r7`..`r10`, puis
concevoir/appliquer/vérifier en direct un correctif.

## Établi

### Le mot `+20` de `r4` est confirmé comme candidat tampon frontal par un motif de double-tamponnage

Relecture du journal `r428_vdswap_args.log` (5 appels) : le mot `+20`
alterne entre EXACTEMENT deux valeurs fixes sur les appels 2 à 5 —
`0x2e33449c` (appels 2, 3) puis `0x8288db80` (appels 4, 5). **Une
alternance entre deux adresses fixes est la signature classique d'un
double-tamponnage** (tampon avant/arrière échangés à chaque
présentation) — cohérent avec un pointeur de tampon frontal, pas avec
une valeur aléatoire ou un compteur. (L'appel n°1 ne suit pas ce
motif — probablement un état transitoire du tout premier appel,
avant que le double-tamponnage ne soit établi.)

### Les dimensions réelles de la cible de présentation, obtenues en direct

`r429_offscreen_dims.gdb` — point d'observation sur
`NativeGuestVdService::bind_offscreen` (déjà câblé dans le runtime
réel, `native_runtime.cpp:50` — le vide signalé par r294 est déjà
comblé) : **`width=1280 height=720`**, lu directement depuis l'objet
`VulkanOffscreenTarget` réellement configuré (`+0x28`/`+0x2c`, mise en
page de classe calculée à la main depuis `native_vulkan_device.h`
faute de symboles de débogage pour les accesseurs triviaux, vérifiée
par un vidage plus large qui confirme des valeurs cohérentes
alentour). `present_to_offscreen` exige explicitement que les
dimensions du paquet CORRESPONDENT à celles de la cible — utiliser
cette valeur interrogée en direct plutôt qu'une constante codée en dur
est donc la conception correcte, pas seulement une commodité.

## Correctif conçu, entièrement spécifié — PAS ENCORE appliqué

**Paquet PM4 `XE_SWAP` à synthétiser** (5 mots, écrits via
`store_guest_word`, déjà utilisé dans ce même fichier) :

```
mot0 (en-tête) = pm4::type3_header(kOpcodeXeSwap, 4) = 0xC0036400
mot1 (payload[0]) = pm4::kSwapSignature = 0x53574150
mot2 (payload[1]) = 0  (le décodeur ne le valide pas actuellement --
                        native_xenos.cpp:401-410 ignore payload[1],
                        n'écrire QUE ce que le décodeur consomme
                        réellement plutôt que d'y injecter le
                        candidat tampon frontal non confirmé à 100%)
mot3 (payload[2] = largeur) = present_target_->width()  (interrogé en
                        direct au moment de la synthèse, PAS codé en
                        dur à 1280 -- suit la cible réelle si elle
                        change)
mot4 (payload[3] = hauteur) = present_target_->height()
```

À ajouter comme méthode dans `NativeGuestVdService`
(`native/src/native_guest_vd.cpp`), appelée depuis `publish_write_address`
avant `drain_locked()`.

## Non établi — pourquoi ce cycle s'arrête ici plutôt que d'appliquer

**L'emplacement EXACT où écrire ces 5 mots dans l'anneau n'est pas
confirmé avec une certitude suffisante pour écrire dans la mémoire
invitée sans risque.** `ctx.r3` (le curseur reçu par `VdSwap`) est
déjà documenté par le code existant comme marquant la FIN du contenu
que le jeu a déjà écrit (`publish_write_address` l'utilise pour
calculer `write_index`, la fin du nouveau contenu) — pas
nécessairement le DÉBUT de l'espace où écrire le nouveau paquet. Le
jeu réserve `64` octets (`sub_821E4F88(r31,64)`) mais le rôle exact de
cette réservation par rapport au curseur `r30+4` transmis n'est pas
entièrement réconcilié. **Écrire au mauvais décalage risquerait de
corrompre un mot appartenant à un AUTRE paquet déjà valide dans
l'anneau** (les traces montrent des tirages/attentes/écritures
d'événement réels autour de ce curseur, r426) — un risque jugé trop
élevé pour un essai non vérifié en direct avant application.

## Décisions prises

- Ne PAS appliquer le correctif ce cycle malgré une conception
  complète — le risque d'écrire au mauvais décalage de l'anneau et de
  corrompre un paquet déjà valide (r426 a confirmé qu'il y en a
  autour du curseur) l'emporte sur le bénéfice d'aller plus vite
  (précédent r1111/r1113 : ne pas deviner/agir sans preuve suffisante ;
  précédent CLAUDE.md général : mesurer deux fois, couper une fois).
- Interroger les dimensions réelles de la cible en direct plutôt que
  d'utiliser la valeur `1280×720` observée comme une constante codée
  en dur dans le correctif — la conception doit rester correcte même
  si la cible change de résolution.
- Ne PAS injecter le candidat tampon frontal (`r4+20`) dans le paquet
  tant que le décodeur ne le consomme pas réellement — écrire
  uniquement les champs dont on est certain qu'ils sont exploités
  (`payload[0]`, `[2]`, `[3]`), pas une valeur non validée dans un
  champ actuellement ignoré.

## Gate

Aucune source de production éditée ce cycle.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées dans le commit de ce cycle)
`ctest` relancé en entier.

## Named for r430

Confirmer précisément l'emplacement d'écriture dans l'anneau (lire
`discover_write_index_locked`/`drain_locked` une fois de plus, en
correspondance exacte avec la réservation de 64 octets côté invité,
ou capturer en direct ce qui se trouve DÉJÀ écrit dans les 64 octets
réservés à `r30` avant l'appel `VdSwap`, pas seulement autour du
curseur `r30+4` comme l'a fait r426). Une fois confirmé, appliquer le
correctif conçu ci-dessus dans `native/src/native_guest_vd.cpp`,
reconstruire, et vérifier en direct que `present_count`/
`presented_frames` progresse enfin.

## Files

`recompilation/ace-combat-6-retail/artifacts/retail-us-native-r404-bucket17-writer/r429_offscreen_dims.gdb/.log`.
