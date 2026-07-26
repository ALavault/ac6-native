# Cycle 171 — ABI de `0x822c20c8` et ses deux étapes vectorielles

Date: 2026-07-18 (Europe/Paris)

## Cible et preuves

Cible canonique AC6 Xbox 360 PAL : `default.xex`, target ID
`ac6-xbox360-pal`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`, base
`0x82000000`, projet Ghidra `ace-combat-6`.

La qualification utilise le désassemblage headless canonique (`DumpRange.java`,
`-noanalysis`) de `0x822c1d88..0x822c213c` et le cross-check XenonRecomp dans
`.tools/recomp-eval/ac6/output/ppc_recomp.45.cpp`. Aucun fichier généré,
projet Ghidra ou source natif n'a été modifié.

## Contrat d'appel de `0x822c20c8`

Le helper reçoit sept pointeurs/valeurs en `r3..r9` :

```text
r3 = contexte de sortie de 16 octets
r4 = vecteur candidat modifiable
r5 = enregistrement de 16 octets
r6 = enregistrement de 16 octets
r7 = vecteur de référence
r8 = vecteur de référence
r9 = vecteur de référence
```

Ces types sont volontairement structurels : les unités et la sémantique moteur
restent `unknown`. Dans l'appel depuis `0x822c2868`, les pointeurs sont des
zones scratch distinctes (`r1+0x60`, `+0x70`, `+0x90`, `+0xa0`, `+0xb0`,
`+0xc0`, `+0xd0`); ne les réutiliser qu'avec l'identité de
fonction et de frame, jamais comme offsets globaux.

Le prologue remappe les arguments vers une première étape :

```text
sub_822c1d88(
    r3 = original r4,
    r4 = original r7,
    r5 = original r8,
    r6 = original r9
)
```

Si l'octet bas de son retour vaut zéro, `0x822c20c8` retourne immédiatement
zéro. Sinon il appelle une seconde étape :

```text
sub_822c1ef0(
    r3 = original r3,   # contexte de sortie
    r4 = original r5,
    r5 = original r6,
    r6 = original r7,
    r7 = original r8,
    r8 = original r9,
    r9 = original r4    # vecteur modifié par la première étape
)
```

Le retour de cette seconde étape est le retour de `0x822c20c8` (réduit à un
octet par l'appelant). Il s'agit donc d'un prédicat en deux étapes, pas d'une
simple copie de données.

## Première étape `0x822c1d88`

Le corps headless confirme :

- chargement de vecteurs 128 bits depuis les arguments `r3/r4` remappés ;
- lecture de deux enregistrements de quatre mots depuis `r5/r6` ;
- différences vectorielles `vsubfp`, permutations et produits VMX128 ;
- comparaison des valeurs absolues avec une constante flottante ;
- normalisation conditionnelle (réciproque/racine vectorielle) ;
- écriture du vecteur transformé à l'adresse du premier argument remappé
  (donc l'original `r4`) ;
- retour `0/1` selon le seuil et les comparaisons flottantes.

Le helper modifie ainsi le vecteur candidat avant la seconde étape. Il faut
préserver l'ordre des lanes, l'endianness big-endian, le mode single-precision,
les comparaisons `fcmpu` et les cas NaN/valeurs nulles.

## Seconde étape `0x822c1ef0`

Cette étape lit trois vecteurs de référence et deux enregistrements 4-mots,
calcule des différences et des produits scalaires vectoriels, puis :

- calcule un facteur flottant par division lorsque les bornes l'autorisent ;
- écrit ce facteur dans `r3+12` ;
- écrit trois composantes flottantes dans `r3+0`, `r3+4` et `r3+8` ;
- applique des opérations VMX128 aux vecteurs temporaires ;
- retourne `1` si les trois comparaisons finales sont acceptées, sinon `0`.

Le `r3` final est donc un petit contrat de sortie (trois flottants + un
facteur), tandis que le vecteur candidat original `r4` est également modifié
par la première étape. Aucun nom comme position, normale, avion ou cellule ne
peut être promu à partir de cette seule preuve.

## Relation avec `0x822c2868` et le lecteur

Dans `0x822c2868`, l'appel à `0x822c20c8` est exécuté pour une entrée de la
table sélectionnée. Le résultat est testé sur son octet bas ; en cas de succès,
la boucle peut committer les vecteurs produits et poursuivre avec le compteur
de l'entrée. Cela confirme le rôle de `0x822c20c8` comme validateur/
transformateur numérique borné pour une entrée, mais ne prouve pas la finalité
du lecteur NDXR ni la signification de ses valeurs.

Cette étape corrige et complète le cycle 170 : le callback `0x821023a0` fournit
un résultat consommé par `0x822c2868`; le booléen observable au niveau du
lecteur provient ensuite de `0x822c20c8`, puis éventuellement de
`0x822c2868`/`0x82102568` selon le chemin.

## Confiance et limites

- `confirmed` : remapping `r3..r9`, court-circuit après la première étape et
  propagation du retour de la seconde étape.
- `confirmed` : écritures `r3+0/+4/+8/+12` de `0x822c1ef0` et écriture du
  vecteur candidat par `0x822c1d88`.
- `confirmed` : opérations VMX128 et comparaisons flottantes observées dans le
  désassemblage headless et le code XenonRecomp.
- `unknown` : classe C++, unité des vecteurs, propriétaire de la table et
  signification gameplay.

Aucun run humain, VNC, Xenia ou intervention clavier n'est nécessaire pour ce
contrat ABI. La prochaine analyse utile est la corrélation des call-sites de
`0x822c20c8` et de la table de descripteurs, sans exposer encore une API
sémantique.

## Reproduction headless

```bash
HOME=/tmp/ac6-ghidra-cycle171-home \
  .tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript DumpRange.java 0x822c1d88 0x822c213c \
  -noanalysis
```
