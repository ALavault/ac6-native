# AC6 retail NTSC-U/J — r506 — un délai de 900s (3,75x le standard) échoue toujours : une seule image présentée en 8s, puis 900+ secondes de blocage RÉEL avant toute reprise, contredisant l'hypothèse « juste lent » de r505

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Oracle utilisé — une seule tentative longue, VRAM libre vérifiée
≥4 Go avant lancement (7 911 MiB, règle utilisateur respectée).

## Contexte

Nommé par r505 : la lecture Ghidra directe avait trouvé une machine à
états invité légitime (délais temporisés chaînés 0,2s/0,5s) comme
explication plausible du blocage répété à `sleep(4)`/`type28=30`, sans
quantifier le délai total cumulé. Ce cycle teste empiriquement
l'hypothèse « juste lent, pas bloqué » avec un délai externe
largement supérieur au `timeout 240` standard utilisé par les 8
cycles précédents : `timeout 900` (15 minutes) sur
`routes/us-pretype28-startup.steps`, une seule tentative longue, pas
plusieurs tentatives courtes.

## Établi — le délai de 900s expire AUSSI, et le journal révèle une image unique suivie d'un arrêt de rendu total, pas une lenteur progressive

Le processus a tourné **~14 minutes 19 secondes** (17:08:39.355 à
17:22:58, dernière ligne du journal) avant que le `timeout 900`
externe n'envoie `SIGTERM` — **le délai de 15 minutes a aussi
expiré**. `RESULT.json` n'a jamais été écrit (interruption pendant
`wait_log()`/`sleep(4)`, trace Python confirmée dans la console).
**Aucun processus résiduel** (`Xvfb`/`ac6recomp`) après — le correctif
`SIGTERM` de r502 fonctionne correctement même sur un arrêt après
900s.

Le journal (`ac6recomp.log`, 229 577 lignes, 37 Mo) montre :
- **UNE SEULE image `XELOG_GPU PRESENT`** dans tout le journal, à
  `17:08:47.494` — **8 secondes après le démarrage**, jamais suivie
  d'une deuxième.
- **228 826 lignes** (99,7 % du journal) sont le motif `KeSetEvent`/
  `KeWaitForMultipleObjects` du mécanisme « movie worker » déjà
  caractérisé par r487/r488 comme un sondage NON bloquant
  (`timeout=0000000000000000`) — actif en continu, preuve directe que
  le processus n'est PAS gelé/suspendu au sens OS (pas de deadlock,
  pas de SIGSTOP), il exécute réellement du code invité en boucle.
- **ZÉRO occurrence** de `type28`/`selector44`/`ac6-save-route`/
  `ac6-save-task`/`ac6-save-state` dans tout le journal —
  **la machine à états de l'écran de sauvegarde lue par r505
  (`Function_821C3800`/`821C5268`/`821C5708`) n'est JAMAIS atteinte**
  pendant ces 900 secondes.

## Conclusion — l'hypothèse de r505 est contredite pour ce lancement précis, le vrai blocage est plus précoce et plus fondamental

r505 avait correctement identifié un mécanisme réel et légitime
(délais temporisés dans une machine à états d'écran de sauvegarde),
mais ce cycle démontre que ce mécanisme n'est **même pas invoqué**
dans ce scénario de lancement précis (route
`us-pretype28-startup.steps`, entrées `Escape`+`space` seules). Le
blocage réel se situe **beaucoup plus tôt** : après la toute première
image présentée (8s), le rendu s'arrête complètement pendant au
minimum 900 secondes, alors qu'un thread continue à exécuter le
sondage non bloquant « movie worker » sans jamais progresser vers un
état qui permettrait une deuxième image ou l'atteinte de l'écran de
sauvegarde. Ceci recontextualise (ne contredit pas la lecture
elle-même, mais son applicabilité à CE scénario) le travail de r505 :
la machine à états qu'il a lue existe et est correcte, mais elle
n'explique pas le blocage observé empiriquement ici.

## Non établi

- **Ce qui bloque spécifiquement la progression après la première
  image** — non identifié ce cycle (limite du périmètre : un seul
  test empirique long, pas une nouvelle lecture Ghidra).
- **Si ce blocage après une seule image est le MÊME phénomène que
  celui documenté historiquement (`presented_frames=0` du sondage
  natif, r279-r286, campagne antérieure à cette chaîne) ou un
  phénomène distinct propre au produit oracle-hybride** — plausible
  qu'il s'agisse d'une manifestation liée (même stade de boot
  précoce), non confirmé par une comparaison directe.
- **Si un délai encore plus long (30+ minutes) permettrait
  éventuellement une reprise** — peu probable étant donné l'absence
  totale de progression sur 900s après la première image, mais non
  testé au-delà de cette durée.

## Décisions prises

- Ne PAS relancer d'autre tentative longue ce cycle — le résultat est
  clair et négatif, une répétition n'apporterait rien de plus sans
  changer d'approche.
- Ne PAS toucher `recompilation/ace-combat-6-retail/native/` — hors
  périmètre, aucune donnée fusionnable obtenue.
- Documenter la contradiction avec r505 explicitement plutôt que de
  la minimiser — conforme à la discipline de ce dépôt (corriger un
  prédécesseur par son nom quand l'évidence l'exige).

## Gate

```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
Tous verts. `ctest` natif non relancé (`native/` non touché). `git
status --porcelain` sous
`recompilation/ace-combat-6-retail/native/` confirmé inchangé.

## Named for r507

**Neuf cycles de capture (r492-r506, ~20 tentatives dont une de
900s), toujours aucune donnée fusionnable pour les 3 états cibles de
r478.** Le vrai blocage se situe après la première image présentée
(8s), pas dans la machine à états de l'écran de sauvegarde (r505,
recontextualisé, pas invalidé). Candidats pour la suite, aucun tenté
ce cycle :
1. Lire directement (Ghidra) ce qui devrait se produire entre la
   première image présentée et la suivante — quel code guest est
   censé s'exécuter à ce stade précis du boot (probablement
   transition logo→titre ou chargement d'un asset initial), et si un
   mécanisme d'attente RÉELLEMENT bloquant (pas le sondage movie
   worker déjà écarté) existe à ce point précis.
2. Comparer directement avec le comportement du produit natif au même
   instant (le sondage `--probe-entry` de r473-r481 atteint son
   contenu cible en <30s, donc la divergence de cadencement
   oracle-hybride/natif documentée depuis r479/r480 concerne
   peut-être exactement ce même point de blocage).
3. Les 3 états cibles de r478 restent non capturés. Aucune reprise de
   capture standard (`timeout 240`) recommandée sans comprendre
   d'abord ce blocage précoce — répéter la même tentative sans
   changement n'apporterait rien de nouveau.

## Files

Committé : ce rapport, `NEXT.md`. Aucun script réutilisable produit
ce cycle (test empirique direct, pas de nouvel outil). Non conservé
(scratch, `/fastdata/lavaulta/tmp/r506-long/`) : journal complet
(37 Mo), captures d'écran, dump de nuanceurs vide. Aucun fichier sous
`recompilation/ace-combat-6-retail/native/` modifié.
