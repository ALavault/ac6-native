# Le frontend tourne, l'anneau ne bouge pas

Date : 2026-08-18
Sondes : les deux runs de 12 000 ticks de
`AC6_DEMO_START_SUPPRESSES_THE_ATTRACT_ADVANCE.md`.

## La mesure

Bloc `graphics.ring` des deux rapports, neutre et START :

```text
submissions            2
submitted_dwords       25
read_pointer           7
write_pointer          25
indirect_buffers       2   ef7ab6e4…d2b0  (11 dwords)
                           d121c8d8…58d6  (3029 dwords)
presentation_notifications  11863
```

**Les deux runs sont identiques champ pour champ, hachages compris.** Sur
12 000 ticks, l'invité soumet deux paquets — au démarrage — et plus jamais
rien, alors que 11 863 présentations sont notifiées.

## Pourquoi c'est le fait le plus utile de la journée

Le run START exécute, lui, un travail soutenu jusqu'au dernier tick :

```text
0x8234F558 -> 0x823513E0    8999 fois   (moteur audio)
0x8234F2D4 -> 0x82357EC0    8998 fois
0x82325CFC -> 0x820EAF30    8964 fois   (rendu swg, une fois par tick)
0x82323E4C -> 0x82322AD8   35856 fois
0x8223D9F0 -> 0x8223D410   26901 fois
```

Le jeu **a répondu** à l'appui : il a démarré une voix audio et une boucle de
rendu par image. Et malgré cela, `submissions` reste à 2.

La question posée toute la journée — « pourquoi le frontend n'avance pas » —
était donc mal posée. Le frontend avance. Ce qui n'avance pas est la
**soumission** : rien de ce que le frontend calcule n'atteint l'anneau.

## Ce que cela relie

Cela rejoint la frontière déjà décrite côté mission, et en fait une seule :

```text
CX360UnitManager slot +0x14 = 0x820A45E0
  lève l'événement (17,6) en 0x820A4778   (seul site de l'image)
→ callback 0x821ADAB8                     (enregistré par 0x821ADC78)
→ arme device+0x5460  ( = [0x10041A00+21600] )
→ sub_821C57D0 soumet
```

`device+0x5460` a été mesuré à 0 pendant tout un run. Les sept sites de
construction de la vtable `CX360UnitManager` sont non atteints en natif.

Frontend et rendu ne sont pas deux chantiers : c'est le même maillon.

## Non établi

- Qui devrait construire `CX360UnitManager`, et pourquoi ce site n'est pas
  atteint. C'est la prochaine mesure.
- Si `device+0x5460` est le seul verrou, ou seulement le dernier lu.
