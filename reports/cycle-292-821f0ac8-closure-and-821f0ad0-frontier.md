# AC6 cycle 292 — fermeture `0x821F0AC8` et front `0x821F0AD0`

## Identité et question bornée

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- question : l'entrée configurée `0x821F0AC8` est-elle indépendante ou une
  instruction interne de la boucle `0x821F0AB8..0x821F0B00` ?

## Preuve headless

Le vérificateur en lecture seule passe **47/47** assertions :

```bash
.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -noanalysis -readOnly \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript Verify821F0AC8Boundary.java
```

Il établit :

- la fin de la fonction précédente à `0x821F0A8C` ;
- le vrai prologue à `0x821F0A90`, la frame de `0xB0` octets et les curseurs
  préparés avant le loop head `0x821F0AB8` ;
- `addi r11,r11,0x89` à `0x821F0AC8`, sans référence entrante ;
- aucune entrée de fonction Ghidra à la cible, au candidat, au voisin
  `0x821F0AD0` ni au backedge ;
- la chaîne continue d'indexation, d'écritures et d'appel indirect jusqu'au
  backedge `0x821F0B00 -> 0x821F0AB8` ;
- la continuation dans la même frame vers une seconde boucle
  `0x821F0B1C..0x821F0B68`.

Verdict : `0x821F0AC8` est une instruction interne **confirmed**, pas une
entrée PPC indépendante. Le voisin `0x821F0AD0` est observé mais délibérément
préservé pour un audit séparé.

## Patch et régénération

Une seule ligne a été retirée de
`.tools/ac6-recomp-reference/ac6recomp_config.toml` :

```text
0x821F0AC8 = { name = "rex_sub_821F0AC8" }
```

L'entrée suivante est restée intacte :

```text
0x821F0AD0 = { name = "rex_sub_821F0AD0" }
```

- hash TOML avant :
  `a069fc66d35b83e7033c34bead3c9181999cfffadebad7e5bddc1cb78fb13b35` ;
- hash TOML après :
  `23557c0c4a3b7ab27d8f198baa0b2ad4b9f084a91e6c7d3f1dd5eecb74f3a7ea`.

Le codegen passe avec **23 330** fonctions en 12,879 s. Le runtime lié a le
SHA-256
`6b65ef6568e21fb84fb6da3e6ec6e9ff696bc2c8062d6eb5487c62e3fde7543e`.
Le pseudo-symbole `rex_sub_821F0AC8` a disparu et `rex_sub_821F0AD0` reste
présent. Aucun fichier généré n'a été modifié manuellement.

## Smoke Xvfb/GDB et frontière suivante

Le smoke borné atteint encore :

```text
__imp__sub_821F0A90
  -> Unresolved branch from 0x821F0B00 to 0x821F0AB8
```

Ce résultat est attendu dans ce cycle strict : la pseudo-entrée préservée
`0x821F0AD0` coupe encore le corps entre la cible et le backedge. La suppression
qualifiée de `0x821F0AC8` réduit bien le corpus d'une fonction, mais elle ne
peut pas fermer seule cette frontière. `0x821F0AD0` devient donc l'unique
prochain candidat exact ; aucune autre entrée n'est modifiée ici.

## Validation native

- build runtime ReXGlue `-j16` : PASS ;
- build AC6 natif `-j16` : PASS ;
- CTest : **48/48 PASS** en 29,03 s ;
- installation racine : PASS ;
- garde `bin/bin` : PASS ;
- processus résiduels `ac6recomp`/GDB : 0 ;
- vérifications de diff racine et sous-dépôt : PASS.

## État

- fonctions générées : 23 330 ;
- pseudo-entrée `0x821F0AC8` : fermée ;
- frontière runtime `0x821F0B00 -> 0x821F0AB8` : encore ouverte, cause bornée
  à la coupure configurée `0x821F0AD0` ;
- entrée configurée suivante à auditer : `0x821F0AD0` dans
  `sub_821F0A90` ;
- statut produit : `candidate`, pas `verified` ;
- intervention humaine/VNC : aucune et non requise ;
- GUI Ghidra : non utilisée.
