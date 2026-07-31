# Cycle 418 — le dernier acte de l'invité : une énumération de contenu vide

## 1. Le contrat overlapped est honoré (piste 1 close)

| horodatage | événement |
|---|---|
| .617 | `queuing for overlapped A33000BC`, résultat mis à `IO_PENDING`, contexte posé |
| .618 | `pre_callback`, `XN_SYS_UI = true` |
| .718 | `running completion` puis **`completing with result 00000000`** |
| .718 | `post_callback` |
| .818 | `XN_SYS_UI = false` |

`CompleteOverlappedEx` écrit résultat, erreur étendue et longueur, signale
l'événement et pose le contexte. **Rien ne manque.** La piste du contrat
overlapped mal rempli est écartée par mesure, pas par lecture.

## 2. Le dernier appel noyau avant le gel

```
12:13:17.101  XamContentCreateEnumerator: added 0 items to enumerator
```

383 ms après la fin de la modale. **C'est le dernier appel noyau de l'invité**
avant qu'il ne se fige pour de bon : le reste du journal ne contient plus que
des présentations et des scrutations de manette.

Séquence complète, désormais lisible de bout en bout :

1. l'invité ouvre le sélecteur de périphérique de stockage ;
2. la modale se termine avec succès, `device_id = 1` ;
3. l'invité énumère le contenu de ce périphérique ;
4. **l'énumérateur rend zéro élément** ;
5. l'invité cesse d'avancer, en redessinant indéfiniment le dialogue vide.

## 3. Ce que cela dit du cycle 397

Le cycle 397 avait testé l'hypothèse « stockage absent » en créant
`content/B13EBABEBABEBABE/4E4D07D1/00000001`, sans effet, et j'en avais conclu
que l'hypothèse était réfutée. **Cette conclusion était trop large.** Ce qui est
réfuté, c'est que *cette arborescence-là* suffise — pas que l'énumération vide
soit hors de cause. Elle est au contraire exactement là où l'invité s'arrête.

L'écart probable est entre la disposition créée et celle que l'énumérateur
parcourt réellement pour `device_id = 1` et `content_type = 1`.

## 4. Reprise, dans l'ordre

1. lire `XamContentCreateEnumerator_entry` (`xam_content.cpp:59`) et relever le
   chemin exact qu'il parcourt pour ce périphérique et ce type ;
2. y placer une sauvegarde et vérifier que le compteur passe au-dessus de zéro ;
3. seulement ensuite, regarder si l'invité avance.

L'étape 2 se vérifie par le journal lui-même — la ligne `added N items` est déjà
instrumentée et dit immédiatement si l'on a visé juste.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
