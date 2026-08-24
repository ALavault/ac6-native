# AC6 démo — point 8 : frontière d’entrée du sélecteur

**Statut du sous-gate : fermé pour la frontière statique/dynamique d’activation.**  
**Statut global : le rendu gameplay et la capture restent ouverts.**

## Question et critère de clôture

Question traitée : pourquoi le sélecteur global à `0x827AD2F0` reste-t-il à
zéro et empêche-t-il la chaîne de soumission Xenos ?

`done_when` : identifier la chaîne d’entrée vers ce sélecteur, vérifier ses
préconditions sur le replay headless qualifié, et isoler le prochain test
discriminant sans modifier le runtime natif.

## Périmètre et sources

- Cible : démo PAL Xbox 360, architecture Xenon/PPC, projet Ghidra
  `ghidra-projects/ace-combat-6-demo`.
- Contrôle statique : code généré sous
  `recompilation/ace-combat-6-demo/build-codegen-on/codegen/generated/`.
- Agrégation précédente des appels et du replay :
  [`analysis/demo/ac6-demo-displaymode-producer-boundary-v1.json`](../analysis/demo/ac6-demo-displaymode-producer-boundary-v1.json).
- Les résultats ci-dessous ne dépendent pas de l’oracle Xenia/Wine et aucune
  modification du code généré ou de l’implémentation native n’a été faite.

## Faits statiques

### Chaîne d’ensemencement

1. `sub_821ACCD0` lit `0x827AD2F0`. Si la valeur est nulle, elle écrit `0x0F`
   (15) ; sinon elle laisse la valeur inchangée.
2. Dans le code généré, son appel direct qualifié provient de
   `sub_821BE9A0` (retour d’appel `0x821BEA0C`).
3. `sub_821BE9A0` exécute d’abord ses prérequis, puis n’active les indicateurs,
   l’état de file et l’appel à `sub_821ACCD0` que si le champ invité
   `device+13416` est non nul.
4. L’entrée statique de `sub_821BE9A0` passe par `sub_821BB078`. Cette dernière
   n’appelle la chaîne qu’après son garde d’état et un délai d’au moins 5000
   unités sur le compteur du TEB du thread primaire (`TEB+0x58`).
5. La boucle `sub_821C57D0` ne prend cette branche que si
   `state_object+0x56F8` a le bit `0x4` positionné. La construction observée
   initialise ce mot à `0x0C000001` : le bit `0x4` est donc absent.

### Ce qui ne peut pas amorcer la chaîne

- `sub_821AD378` est le consommateur/automate du sélecteur. Il soustrait 11 et
  abandonne hors de l’intervalle `0x0B..0x13`; ses cas internes peuvent ensuite
  écrire des valeurs de sélecteur, mais il ne peut pas démarrer depuis zéro.
- `sub_821AD7C0` est un parseur de caractères (`k`, `a`, `c`, `d`, `f`, `g`)
  qui lit/écrit aussi ce global selon ses branches. Aucun appel direct n’est
  présent dans le code généré ; seuls deux sites d’appel calculé
  (`0x821ADA30`, `0x821ADA64`) restent à résoudre. Son corps ne justifie donc
  pas l’hypothèse « starter du renderer ».
- Les écritures littérales connues de `state_object+0x56F8` sont limitées à
  l’initialiseur `sub_821BB4C8` et au préservateur de bits
  `sub_821AEBE8`. Aucune écriture scalaire ultérieure observée ne positionne le
  bit `0x4`.

## Observations dynamiques

Replay headless borné, avec démarrage START aux ticks 3000–3001, jusqu’au tick
3036 :

- `sub_821AD378` est appelé 2899 fois ; il retourne à chaque fois avant une
  transition utile.
- Aucune entrée observée dans `sub_821BB078`, `sub_821BE9A0`,
  `sub_821ACCD0` ou la route alternative `sub_821C3B88`.
- Le sélecteur reste `0` sur la fenêtre de contrôle ticks 2998–3004.
- `state_object+0x56F8` reste `0x0C000001` (bit `0x4` clair) et
  `device+0x5404` reste nul.
- START ne change donc ni le garde d’état, ni le sélecteur, ni le champ de
  transition observé.

Le probe visuel séparé confirme la conséquence, sans incriminer l’exporteur de
capture : 2928 notifications de présentation VdSwap, mais zéro présentation
typée issue d’un paquet Xenos qualifié, zéro resolve neutre et zéro
guest-writeback. Le chemin de capture est conditionnel à ce paquet et ne peut
pas produire d’image en son absence.

## Décisions sur les hypothèses

| Hypothèse | Décision | Preuve principale |
|---|---|---|
| L’exporteur screencap est cassé | Réfutée | Le chemin accepte la configuration ; aucune capture n’est publiée car le paquet qualifié/guest-writeback manque. |
| START devrait activer le renderer | Réfutée pour ce replay | Les échantillons de bord START ne modifient aucun des trois champs contrôlés. |
| `sub_821AD7C0` est le starter du renderer | Réfutée | Corps de parseur, appels directs absents, sites calculés non résolus. |
| Le délai TEB est la cause prouvée | Indécidable | `sub_821BB078` n’est jamais atteint ; la valeur du compteur n’a donc pas été observée au point de décision. |

## Conclusion du gate

La frontière d’activation est maintenant qualifiée : le replay reste bloqué en
amont, au garde `state_object+0x56F8 & 0x4`, avant le délai TEB et avant
l’ensemencement `sub_821ACCD0`. Le sélecteur nul est une conséquence de cette
absence d’entrée, pas une preuve d’un défaut de sa machine d’état.

Cela ferme le sous-gate « pourquoi le sélecteur ne s’amorce pas ? » sans
autoriser de patch du sélecteur, du temps ou du champ invité.

## Prochain test discriminant

Qualifier une seule frontière statique/dynamique : l’entrée indirecte unique
vers `Function_822F85B8` (`0x822F85B8`) et toute voie de construction qui lui
transmettrait un paramètre avec le bit `0x4` avant l’écriture
`sub_821BB4C8`. Si cette entrée reste absente, la cause se déplace vers le
producteur de l’objet d’état ; si elle est atteinte avec le bit clair, il faut
alors examiner son producteur immédiat, sans injecter artificiellement la
transition.

