# AC6 cycle 115 — contrat statique des fronts et répétitions d'entrée

## Périmètre

Analyse headless en lecture seule du binaire Xbox 360 PAL `default.xex`,
SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
Aucune session Xenia, Wine, GUI ou humaine n'a été lancée.

Cette tranche suit l'agrégateur identifié au cycle 112 et ne donne aucun nom
de touche, d'axe, d'avion, d'arme ou de caméra.

## `0x82214f88` — fronts, répétition et compteurs flottants

Le helper reçoit le temps/delta en premier paramètre et l'état agrégé en
second. Il calcule :

```text
state+0xe4c = current_bits & ~previous_bits
state+0xe50 = previous_bits & ~current_bits
state+0xe54 = current_bits & ~previous_bits
```

Il traite 32 bits par groupes de quatre. Pour chaque bit actif, il accumule un
compteur flottant dans un bloc de 32 octets; pour chaque bit inactif il remet le
compteur et la valeur de seuil aux valeurs stockées autour de `state+0x1058`.
Lorsque le compteur atteint le seuil, le bit correspondant est ajouté à
`state+0xe54` et le compteur est réinitialisé avec la seconde valeur de seuil
(`state+0x105c`). Les champs flottants sont donc des compteurs de durée/
répétition, pas des axes analogiques qualifiés.

Les offsets observés dans le premier groupe sont `+0x1060` à `+0x107c`; les
sept groupes suivants avancent de `0x20`, jusqu'à `+0x115c` pour 32 entrées.
Le helper ne lit ni chaîne clavier, ni scan code, ni état avion.

## `0x82215140` — projection des masques

Ce helper lit le masque externe `external+0x08` et quatre mots du périphérique
(`device+0x08..+0x14`). Pour chaque groupe de quatre bits, il OR les bits
correspondants dans le mot de sortie `state+0xe44`. La projection est donc
structurellement :

```text
external_mask & device_word[0..3] -> aggregate_bit[0..31]
```

Le test `external+0x08 == 0` court-circuite la projection. Aucun champ ne
permet encore d'associer un bit à `Start`, `A`, au manche, à la caméra ou à une
commande de mission.

## Limite de la preuve

Les appels directs sont internes à l'agrégateur (`0x82215470 -> 0x82215140`,
`0x82215484 -> 0x82215210`, `0x822154a0 -> 0x82214f88`). L'entrée
`0x82215418` reste référencée par la table runtime `0x82080c40`, sans appel
statique ordinaire. Le raccord suivant — qui consomme `+0xe4c/+0xe50/+0xe54`
dans le contrôleur de vol — n'est donc pas établi par ce cycle.

Décision : conserver les champs sous les noms offset-qualified du cycle 112,
classer `0x82214f88` comme `input_edge_repeat_helper`, et ne pas ajouter de
wrapper natif ou de mapping de touches. La jonction flight-consumer reste
`needs-dynamic-evidence`; elle pourra être reprise plus tard avec une session
Xenia/XenonTests explicitement autorisée, mais elle ne constitue pas une
demande d'action humaine maintenant.

## Validation

- `FindDirectCallsTo.java` sur `0x82215418`, `0x82214f88`, `0x82215140` et
  `0x82215210`.
- `DecompileAt.java` sur `0x82214f88`, `0x82215140`, `0x82215210` et la zone
  `0x82215418`.
- CTest natif AC6 : **41/41 PASS**.
- Aucun fichier généré XenonRecomp, source retail ou état d'émulateur modifié.
