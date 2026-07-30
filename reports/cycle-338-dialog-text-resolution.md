# Cycle 338 — d'où vient le texte du dialogue : trois chemins écartés, la lecture est saine

## 0. Demande

Trouver comment le texte du dialogue est résolu, et rendre l'échec bruyant.

## 1. Trois chemins runtime écartés, par mesure

| candidat | verdict | preuve |
|---|---|---|
| boîte de dialogue ImGui du runtime | **écarté** | aucun `XamShowMessageBoxUI*` dans le journal ; les seuls appels XAM autour du dialogue sont `XamShowDeviceSelectorUI`, `XamContentCreateEnumerator`, `XamDispatchHeadless`, `XamNotifyCreateListener` |
| table de chaînes XDBF | **écarté** | `XdbfWrapper::GetStringTableEntry` n'est appelé que pour le titre et les succès (`xam_user.cpp`), jamais pour une interface en jeu |
| périphérique de stockage / profil | **écarté** (cycle 337) | `XamShowDeviceSelectorUI` implémenté, rend `X_ERROR_SUCCESS`, `device_id = 1` |

**Conclusion : AC6 dessine ce dialogue lui-même et résout son texte dans son
propre code, à partir de ses propres données.** Le runtime n'a aucune
visibilité sur cette résolution — sauf à la lecture des données.

Indice confirmant : « YES » et « NO » **sont** du texte et s'affichent
correctement. Le rendu de texte fonctionne. Seule la chaîne de la question
manque.

## 2. Échec bruyant posé à la seule frontière visible

`NtReadFile` ne journalisait que les chemins déjà suspects
(`focused_pac_read`) : une lecture **échouée ou courte** ailleurs était
silencieuse. Ajouté, au niveau `warn`, et uniquement en cas d'échec ou de
lecture courte, pour qu'une exécution saine reste muette :

```
[AC6 IO] guest read did not deliver: caller=... path=... status=...
         requested=... delivered=...   <- read FAILED | SHORT read
```

Patch : `patches/rexglue-loud-short-and-failed-guest-reads-20260730.patch`.

C'est la règle des cycles 312, 329 et 333 appliquée au chemin des ressources.

## 3. Ce que l'instrument dit

Exécution complète jusqu'au dialogue, avec entrée :

```
lignes [AC6 IO] : 0
```

**Aucune lecture invitée n'échoue ni ne rend moins que demandé.** Le chemin de
lecture des ressources est sain de bout en bout.

C'est un résultat négatif utile, et il resserre la cause à deux possibilités,
qui s'excluent :

1. la chaîne est **lue correctement puis non retenue** — recherche dans une
   table qui rend vide alors que la donnée est présente (index, langue, ou
   clé erronée) ;
2. la chaîne n'est **jamais demandée** — le code qui construit le dialogue
   saute l'assignation du texte.

La distinction se fait invité-side, pas runtime-side : il faut instrumenter la
fonction invitée qui construit ce dialogue.

## 4. Front suivant

1. Localiser la fonction invitée qui construit le dialogue OUI/NON — partir de
   `sub_821BABB8` (routine de locale, déjà documentée dans `xam_info.cpp`) et
   des acquis `FUNCTION_*` existants plutôt que d'instrumenter à l'aveugle.
2. Poser un `[[midasm_hook]]` sur son assignation de texte et journaliser
   l'identifiant demandé et le pointeur rendu.
3. Le cas (2) ci-dessus — chaîne jamais demandée — est le plus probable si la
   construction dépend d'un état non initialisé, et rejoindrait alors
   l'observation du cycle 337 : un dialogue bâti sans son texte peut aussi
   avoir été bâti sans l'action de ses boutons.

## 5. Ce qui reste

P1.3 non franchie. P2 à P7 non faits. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
