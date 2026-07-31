# Cycle 431 — l'état **est** lu, et l'attente branche vers `0x821C58CC`

## 1. Une fausse rétractation évitée de justesse

J'ai d'abord cru que `sub_821C56F8` se terminait ligne 16910, donc que le second
site d'appel (17013) appartenait à une autre fonction — et j'allais corriger le
cycle 430 sur ce point.

C'était faux : mon motif `^}$` a trouvé une **accolade fermante interne**, pas la
fin de la fonction. Vérification par recherche de la dernière `PPC_FUNC_IMPL`
avant 17013 : c'est bien `sub_821C56F8`. **Le cycle 430 tient.**

Troisième erreur de lecture en trois cycles, et cette fois elle a été rattrapée
avant publication — mais uniquement parce que j'ai recoupé avec une seconde
méthode. Une seule mesure de la fin d'une fonction ne suffit pas.

## 2. La lecture d'état existe

À `loc_821C5848`, dans `sub_821C56F8` :

```
r11 = [this+4]           ; l'objet sélecteur
r10 = [r11+84]           ; son état  (0 repos, 1 démarrage, 2 attente)
cntlzw r10,r10
rlwinm r10,r10,27,31,31  ; idiome « est nul » : r10 = (état == 0) ? 1 : 0
cmplwi cr6,r10,0
beq cr6 -> loc_821C58CC  ; état ≠ 0  ->  saut
```

Donc la routine de mise à jour **consulte bien** `[obj+84]`. L'hypothèse « la
complétion n'est constatée nulle part » (cycle 430, issue 3) est **écartée**.

## 3. Où va l'écran quand il attend

État 2 (attente) ⇒ `r10 = 0` ⇒ branchement vers **`0x821C58CC`**.

C'est donc `0x821C58CC` que l'écran exécute en boucle pendant tout le blocage.
C'est la branche d'attente, et le seul endroit où la sortie de l'état 2 peut se
décider.

## 4. Suite, précise et courte

Lire `loc_821C58CC` **en entier, sans filtrer**. Deux issues :

- il consulte l'`overlapped` (résultat, événement, `XamGetOverlappedResult`) et
  remet `[obj+84]` à 0 → comparer alors ce qu'il attend avec ce que nous
  écrivons ;
- il ne remet jamais l'état à 0 → l'écran ne peut structurellement pas sortir,
  et la cause est nommée.

L'adresse est connue, la branche est courte d'après l'espacement (`0x821C5848` →
`0x821C58CC`, 132 octets), et c'est le dernier maillon non lu de la chaîne.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
