# Cycle 436 — recherche non concluante : le motif était trop étroit

## 1. Ce que j'ai cherché

Lectures `PPC_LOAD_U32(ctx.rN.u32 + 92)` ou `+ 96` dans la région
`sub_821C56F8` / `sub_821CE8A8` (lignes 16400-17600).

**Zéro correspondance.**

## 2. Pourquoi je n'en conclus rien

Le motif suppose que l'`overlapped` est toujours adressé depuis le pointeur
d'objet. Or `sub_821CE8A8` établit explicitement `r7 = this+88` avant l'appel.
Depuis `r7`, l'`overlapped` est à `r7+4`, et son champ résultat à `r7+8` — une
lecture apparaîtrait donc comme `+ 4` ou `+ 8`, jamais comme `+ 92`.

Toute base intermédiaire produit le même effet. Le PowerPC calcule couramment
une adresse de base une fois puis adresse par petits décalages ; chercher le
décalage absolu ne capture qu'une forme parmi beaucoup.

**Zéro correspondance ne signifie donc pas « aucune scrutation ».** Cela
signifie « aucune scrutation *de cette forme-là* ».

## 3. Pourquoi je le signale plutôt que de conclure

C'est exactement l'erreur commise aux cycles 412 (registre lu au mauvais
moment), 428 (étiquette prise pour un chemin), 431 (accolade prise pour une fin
de fonction) et 433 (champs supposés au lieu d'être relevés). Dans les quatre
cas, un instrument trop étroit a produit une affirmation fausse.

Le cycle 435 énonçait qu'une absence de correspondance signifierait un défaut de
codegen. **Cette inférence est invalide** telle qu'elle était posée, et je la
retire avant qu'elle ne serve de base à la suite.

## 4. Recherche correcte, à faire

Ne pas chercher un décalage. Chercher **l'usage de l'adresse** :

1. relever, à l'exécution, le contenu de `A33000BC+0` (champ résultat) à
   intervalles réguliers pendant le blocage — s'il passe de `IO_PENDING` à 0 et
   que l'écran ne bouge pas, personne ne le lit ;
2. ou poser une surveillance mémoire en lecture sur `A33000BC` et relever le
   `lr` de l'accédant — cela nomme le lecteur, s'il existe, sans dépendre
   d'aucun motif textuel.

La seconde est décisive et indépendante de la forme du code.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
