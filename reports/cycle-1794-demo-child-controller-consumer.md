# Cycle 1794 — consumer du `MovieController` enfant

## Cible qualifiée

- projet Ghidra canonique : `ghidra-projects/ace-combat-6-demo`
- module : `Default.xex` de la démo PAL
- corps qualifié : `0x82323468..0x823235CF`
- callsite de construction enfant : `0x82323568`, retour `0x8232356C`

## Verdict

`CHILD_RESULT_CONSUMER_QUALIFIED / ROOT_PUBLICATION_REFUTED_IN_BODY`

Le projet canonique place le callsite dans la fonction
`0x82323468..0x823235CF`; l'ancienne frontière autonome `0x82323510` est donc
supersédée sur ce seam. Le corps calcule le descripteur `D` depuis la table
référencée par `A+0x20`, avec l'index porté par `param3+0x08`. Il tente d'abord
de réutiliser un objet dont `candidate+0x24 == D` et
`candidate+0xD0 == *(param3+0x0C)`. L'objet correspondant est détaché de sa
liste ; s'il était aussi `A+0xF4`, ce champ reçoit son successeur.

En l'absence de candidat, le slot virtuel 5 qualifié, thunk `0x820D0F78`,
construit l'enfant observé au cycle 1793. Les deux branches convergent sur
`C`, puis écrivent `C+0x04=param3` et `C+0x08=param4`, enregistrent `C` par le
slot virtuel 4, thunk `0x820D0F68`, et l'insèrent en tête de la liste :

```text
C+0x18 = 0
C+0x1C = A+0xE8
ancien(A+0xE8)+0x18 = C
A+0xE8 = C
si A+0xE4 == 0 : A+0xE4 = C
```

Le corps ne publie jamais `C` dans `A+0xF4`. Sa seule écriture à `A+0xF4`
retire un ancien candidat actif correspondant. Le résultat est donc un enfant
en attente dans `A+0xE4/A+0xE8`, pas encore le contrôleur actif.

## Frontière sémantique

`swg::MovieController` est ici le contrôleur d'une timeline d'interface
scriptée : il avance des éléments graphiques et programme des offsets de
bytecode dans `MovieMemory` pour la VM `swg::ASContext`. Ce n'est ni le
décodeur de `moviepack.bin`, ni `CModeTaskTitleMovie`. L'analogie avec un
`MovieClip` Flash est fonctionnelle seulement ; elle ne prouve ni un format
SWF standard ni l'expansion du sigle SWG.

La chaîne structurelle déjà qualifiée appelle
`0x82323BB8(A+0xF4)` depuis le tick de `CSwgManager`. Dans cette boucle,
`MovieMemory` est consommée comme une file d'offsets de script ; le handler
`0x82322A80` en est un producteur. Cette jointure distingue donc clairement
la liste d'enfants en attente `A+0xE4/A+0xE8` du contrôleur actif `A+0xF4`.

## Portée et gate suivant

Aucune preuve de `EndMode`, listener Title, écriture `manager+0x18`, frame
frontend ou gameplay ne découle de cette insertion. Le prochain gate est
strictement statique : qualifier le writer et le prédicat qui promeuvent un
enfant de `A+0xE4/A+0xE8` vers `A+0xF4`, puis joindre son premier appel à
`0x82323BB8`. Il est fermé lorsque cette promotion et ce premier tick sont
établis, ou lorsqu'une preuve négative bornée nomme le propriétaire alternatif.

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

