# Cycle 1676 — frontière de lecture hôte du frontbuffer

## Verdict

Une instrumentation opt-in de `GuestMemory::load_bytes` a été rejouée sur
deux processus frais, neutral et START, jusqu'à 253 ticks avec le backend
Vulkan. Les deux exécutions produisent exactement deux lectures hôte de la
plage frontbuffer, chacune de `0x1374A000` sur `0x398000` octets. Les deux
callers se résolvent dans `commit_reached_guest_present`, aux offsets binaires
`0x9BA2B` et `0x9BA9C`, correspondant aux lectures avant et après le
writeback (`xenos_guest_present_join.hpp:30` et `:41`).

Ces lectures sont donc la validation hôte du writeback renderer déjà qualifié,
pas un consumer guest-owned ni une présentation écran. Aucun consumer de pixel
guest-owned n'est promu et aucune screencap n'est produite.

## Identité et périmètre

- cible exclusive : `Default.xex`, démo Xbox LIVE PAL ;
- SHA-256 XEX :
  `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` ;
- architecture : Xenon big-endian / Xenos ;
- adresse de destination jointe par `XE_SWAP` : `0x1374A000` ;
- étendue tiled : `0x398000` octets (`1280x720`, format `6`) ;
- aucune preuve retail, Xenia, XenosRecomp, Ghidra, microcode ou actif
  propriétaire n'est fusionnée.

## Instrumentation

`src/guest_memory.cpp` ajoute `watch_frontbuffer_host_read`, activé uniquement
par `AC6_DEMO_WATCH_FRONTBUFFER_HOST_READS=1`. Il journalise l'adresse, la
taille, l'offset du caller hôte résolu par `dladdr` et le symbole lorsque
disponible. La garde est bornée à 128 lignes, ne modifie ni mémoire ni
ordonnanceur et reste désactivée dans le produit normal.

Les seuls appels sources qui recouvrent cette plage sont les deux
`load_guest_bytes` de `commit_reached_guest_present` :

1. ligne 30 : copie de l'allocation guest avant insertion des pixels résolus ;
2. ligne 41 : relecture de validation après `store_guest_bytes`.

La recherche source ne trouve aucun `vkQueuePresentKHR`. Les usages de
`GuestMemory::raw_base()` restent limités au dispatch/scheduler guest et aux
hooks de diagnostic ; aucun chemin de scanout hôte n'est présent. Cela borne
le résultat au code actuel et ne remplace pas une preuve guest d'un consumer
PPC futur.

## A/B process-fresh

| mesure | neutral | START au tick 252 |
|---|---|---|
| backend / ticks | Vulkan / 253 | Vulkan / 253 |
| lectures frontbuffer | 2 | 2 |
| lignes observées | adresse `0x1374A000`, taille `0x398000`, callers `0x9BA2B/0x9BA9C` | identiques |
| RTPLY SHA-256 | `c5357c6d9639c2a675161bd10c3cdbe97096df0dac11d760fe8a2d964b1c5794` | `4a7326d9b1148dc6a5943cabea5c0e2562ae7ad833eda6bc7cd92a089e25724f` |
| rapport SHA-256 | `4c6bea202bb0c2c86ecf102735ab4523c2f12411d6f241e8aa1ade1e8d77a214` | `d69ab8c7cf0e2d887f251ed62a5e803f1f51f5a9136e27aec2547dcfc2c88a35` |
| stderr SHA-256 | `62eac5079fbd9312d00c705dbd08ebcd87e98a0961d137c44f219a65e274df56` | identique |
| stdout SHA-256 | `380814f143c6c85707eb60f76c4ef137d9151d005b3100bda87569a0f2912e79` | `10ea62ed7253b1192d16f1784ccfa3a4a7c16b8f0a34fa9c5a1b77d0b01e312e` |
| présentations / IB principal | 116 / `d121c8d8…358d6` | identiques |
| guest writeback / digest linéaire | `1` / `0c660f2b…a4913a5f` | identiques |
| frontend / mission / terminal | non / non / non | non / non / non |

Le stderr complet est limité aux deux lignes suivantes par route :

```text
AC6_FRONTBUFFER_HOST_READ address=0x1374A000 size=3768320 caller_module_offset=0x9BA2B symbol=
AC6_FRONTBUFFER_HOST_READ address=0x1374A000 size=3768320 caller_module_offset=0x9BA9C symbol=
```

## Qualification

- `demo-qualified` : identité XEX, plage/adresse/taille, deux lectures par
  route, callers hôte et jointure aux deux lignes de `commit_reached_guest_present`,
  A/B process-fresh, IB/VdSwap/readback identiques ;
- `demo-observed` : `guest_writeback=1`, digest linéaire noir
  `0c660f2b…a4913a5f`, 116 notifications `VdSwap` ;
- `xenia-generic` : aucun nouvel élément ;
- `unknown` : consumer PPC guest-owned, scanout externe à ce processus,
  pixels non noirs, frontend, mission et screencap.

Le readback reste un effet renderer exact et fail-closed, pas une image de
référence. START n'est toujours pas promu comme transition frontend ou
visuelle causale.

## Validation

- build codegen normal : succès ;
- build diagnostic vector : succès ;
- CTest codegen : `17/17` ;
- CTest diagnostic : `17/17` ;
- `render_status.py --check` : `AC6_DEMO_STATUS_PASS` ;
- `git diff --check` : pass ;
- Xenia/ReXGlue/Ghidra/C++ généré/microcodes inchangés.

## Prochain checkpoint

Rechercher le premier accès PPC guest-owned hors `0x821C5190` en conservant la
garde d'état et le traceur hôte séparés. Toute nouvelle adresse non jointe doit
trap avant effet. Tant que ce consumer n'est pas observé avec PC/LR/thread/tick,
aucun readback ne doit être promu en screencap et la progression START reste
bornée au même état guest.
