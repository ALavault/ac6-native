# Cycle 453 — le panneau vide est ramené à la chaîne de clés UI

## Périmètre qualifié

- Cible : Ace Combat 6 PAL, Xenon PPC big-endian.
- `default.xex` SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Runtime Linux normal final SHA-256 : `3945617bc76996e5a3652e28261cbbc08e0c1723097afff806949c7e9b227514`.
- Runtime diagnostic final SHA-256 : `eb7d9f23000cf48ba8c92ba4cef4f28bb798db2dc7ad577799da0a2a5ea8112f`.
- Aucun fichier généré par le recompilateur n'a été modifié.

## Résultat

Le changement de locale anglais/japonais testé par Claude ne modifie pas le
panneau. Cette hypothèse est close.

Le défaut est reproduit : après le titre, l'invité affiche d'abord son
inventaire de caractères, puis un panneau sans corps avec seulement `YES` et
`NO`. Le rendu continue à 60 Hz. `XamShowDeviceSelectorUI` est appelé par
l'invité depuis `0x821CE928`; le panneau reste rendu par le jeu, pas par une UI
hôte de secours.

La nouvelle instrumentation qualifie la chaîne suivante :

1. `sub_8237D5F0` construit les données du nœud UI. Son appel indirect qui
   retourne à `0x8237D750` remplit notamment le buffer `stack+208`.
2. Ce buffer devient `r7` de `sub_820D7C08`.
3. `sub_820D7C08` transmet la chaîne via son appel indirect qui retourne à
   `0x820D7C5C`.
4. La cible observée est `sub_820F8608`; son argument chaîne est `r5`.

Pendant le panneau fautif, 600 appels bornés à `sub_820F8608` donnent :

| Appels | Retour PPC | Chaîne |
|---:|---:|---|
| 37 | `0x820D7C5C` | `LOAD_W_003` |
| 31 | `0x820D7C5C` | `M70000_222` |
| 64 | `0x820D7C5C` | `M70000_122` |
| 63 | `0x820D7C5C` | `H90000_100` |
| 63 | `0x820D7C5C` | `H90000_101` |

`B00000_007` passe par la même fonction sur un écran antérieur correct. La
présence d'une clé à cette frontière est donc normale et ne prouve pas encore
que la clé elle-même est le défaut. En revanche, la famille précise du panneau
est maintenant isolée. La prochaine comparaison discriminante est le résultat
de l'appel indirect à `0x8237D750`, ou le premier consommateur de ces clés dans
`sub_820F8608`, entre Linux et un oracle fonctionnel.

Confiance :

- panneau invité, appel XAM et adresses ci-dessus : `confirmed` / `dynamic` ;
- familles de clés relevées et comptes : `confirmed` / `dynamic` ;
- défaut de résolution ou de chargement de la famille `LOAD/M/H` : `heuristic` ;
- cible dynamique de l'appel retournant à `0x8237D750` : `unknown`.

## Outillage conservé

- Le scanner global `/proc/self/maps`, qui pouvait produire un `SIGBUS` sur un
  trou de fichier, est supprimé.
- `AC6RECOMP_PROBE_GUEST_TEXT` force un préambule uniquement dans six unités UI
  générées, sans les éditer. Les lectures passent par les permissions du
  gestionnaire mémoire invité.
- Les wrappers `sub_820F8608` et `sub_820D7C08` sont compilés uniquement dans ce
  build diagnostic. Le build normal ne paie aucun branchement de probe.
- `tools/ac6-run.sh` refuse un display X déjà occupé, ne tue que ses propres
  processus, propage les sorties anormales et reconnaît un `SIGKILL` de
  `timeout -k` seulement après la durée demandée.

## Validations

- Configuration diagnostic Clang 21 : PASS, 6 unités UI instrumentées.
- Build diagnostic `cmake --build build-text -j16` : PASS.
- Rebuild diagnostic final : PASS, action
  `2c31ea5334a8575bf05aaf0e431b9997a5568c62f5104881c80793c0f5323736`.
- Capture qualifiée avec LR : action `eb872be4acaafa4251c439bc5275f8b12b866f04e57fe7704d6a65a155086fa9`.
- Capture du renderer : action `0c24352956147944cfebf5b0e065ded94e26154fd018b7133ec554df783339b3`.
- Log renderer SHA-256 : `14c38aeeae245a8b4fb915869239dd71491725f81aef4a8bd9a386ead9281fd7`.
- Build normal ciblé `ac6recomp -j16` : PASS, action
  `24d5ab71761fe13e0f7c355256c9e677345afacfb6677063299adeb6491cae7e`.
- CTest AC6 ciblé : 2/2 PASS, action
  `7478c9ba598b799d727f0dacb8b5b7264737081004e5834cbb7056ad32a27e82`.
- `git diff --check` et `bash -n tools/ac6-run.sh` : PASS.
- Layout : `build-rt/bin/bin` absent.

Le corpus CTest complet n'est pas exécutable dans cet état du SDK : les 166
tests PPC échouent avant compilation car
`thirdparty/rexglue-sdk/tools/binutils/powerpc-none-elf-as` n'a pas le bit
d'exécution (`Permission denied`, code 126). Aucune permission n'a été changée.

## Risques résiduels et reprise

- Aucun correctif visuel n'est encore démontré ; AC6 ne franchit pas ce panneau.
- Les deux dernières tentatives de capture amont n'ont pas produit de nouvelle
  preuve : quota consommé par `B00000_007`, puis appuis arrivés dans la
  cinématique. Les relances dynamiques sont suspendues.
- Les exécutables diagnostiques des captures intermédiaires ont été remplacés
  en place avant calcul de leur SHA-256 brut. Leurs identités restent liées aux
  digests d'action `run-once`, qui ne doivent pas être présentés comme des
  SHA-256 de fichier.
- Reprise recommandée : résoudre statiquement la cible vtable de l'appel
  retournant à `0x8237D750`, puis instrumenter uniquement son buffer de sortie
  `stack+208`. Ne pas relancer le scanner global ni retester la locale.

État : `needs-dynamic-evidence`; `recompiler-generated` n'est pas `verified`.
