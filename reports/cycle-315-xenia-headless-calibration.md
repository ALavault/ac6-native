# AC6 cycle 315 — l'oracle Xenia sous Xvfb ne rend rien non plus

Le cycle 314 désignait l'oracle Xenia comme la prochaine étape : franchit-il le
point où le natif s'arrête ? Ce cycle exécute la mesure. **Réponse : non**, et
cela recalibre l'interprétation des cycles 308 à 314.

## 1. Exécution

Le binaire Linux qualifié était absent de l'arbre ; seule l'archive subsistait.
Après extraction :

```
sha256 98559834c570d4be8ba5d532f000aadf8ea6cf4d495be34a02b7ae766134007c
```

— identique au hash consigné dans `AC6_XENIA_ENTRY9_ORACLE_AUDIT.md`. L'oracle
exécuté est donc bien celui qualifié.

Exécution via le script de référence `run_xenia_ac6_oracle_baseline.sh`, avec
captures à **30 s** et **150 s**, soit très au-delà des 8/20 s par défaut, sous
la garde de progression `Launching module` exigée par le script.

## 2. Résultat

| Capture | Moyenne des pixels | Couleurs |
| --- | --- | --- |
| `xenia-30s.png` | 0,0360151 | 171 |
| `xenia-150s.png` | **0,0360151** | **171** |
| `xenia-canary-retail-15s.png` (2026-07-15) | **0,0360151** | **171** |

Les trois captures sont **identiques**, à 15 s, 30 s et 150 s. Xenia atteint
bien le jeu — son journal porte `Title name: ACE COMBAT 6` puis
`KernelState: Launching module...` — et n'affiche jamais la moindre image.

Multiplier la durée par dix ne change rien.

## 3. Conséquence sur l'interprétation

L'absence de contenu rendu par le runtime natif **n'est pas** un écart par
rapport à l'oracle : dans les mêmes conditions sans affichage réel, l'émulateur
de référence ne rend rien non plus.

Comparaison à conditions égales, sous Xvfb :

| | Xenia Linux | runtime natif |
| --- | --- | --- |
| module chargé | oui | oui |
| images présentées | **0** | **2** |
| surimpression rendue | non | **oui** |
| audio actif | — | **oui, 10 trames** |
| survie | oui | oui |

Le runtime natif fait donc **davantage** que l'oracle dans cet environnement.

Cela ne prouve aucune parité — il ne rend toujours pas de contenu de jeu — mais
cela retire tout fondement à l'hypothèse « le natif est en retard sur
l'émulateur ». Les deux butent au même stade sans affichage réel.

## 4. Ce que l'audit disait déjà

`AC6_XENIA_ENTRY9_ORACLE_AUDIT.md` (2026-07-15) l'énonçait :

> sur cet hôte, la version native a lancé le module mais est restée noire. La
> route interactive qualifiée est la version Windows épinglée via Wine avec
> Vulkan.

Ce cycle **confirme** ce constat à 150 s au lieu de 20 s, et le referme :
**l'oracle Xenia Linux sous Xvfb est une impasse**, non par manque de temps,
mais par nature.

## 5. Blocage identifié

La seule route d'observation qualifiée est Xenia Canary **Windows** via **Wine**
sur l'**affichage GNOME vivant**, avec focus fenêtre et clavier émulé.

Cette route exige la session graphique de l'utilisateur. Elle ne peut pas être
empruntée depuis une tâche de fond sans réquisitionner son écran.

Le même raisonnement vaut pour le runtime natif : sa progression au-delà de deux
images n'a jamais été tentée ailleurs que sous Xvfb.

## 6. Prochaine tranche

1. **Décision humaine requise** : autoriser l'usage de l'affichage vivant, pour
   l'oracle et pour le runtime natif. Sans cela, la comparaison de rendu reste
   hors d'atteinte.
2. À défaut, poursuivre sans rendu : instrumenter la boucle active identifiée
   au cycle 314 (26 s de CPU sur un fil) pour déterminer quelle condition
   d'état n'est jamais atteinte.
3. Vérifier les adresses `[rexcrt]` restantes, le cycle 313 ayant montré qu'une
   seule ligne fausse suffit à tout bloquer.

`recompiler-generated` n'est pas `verified`.
