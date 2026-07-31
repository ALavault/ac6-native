# Cycle 426 — `sub_8234D210` est la répétition automatique, et elle dépend d'une horloge

## 1. Décodage

```
bl   sub_821F3BA0        ; source de temps -> r3
r9 = [this+120]          ; délai initial   ; si 0 -> [this+36] = 0, sortie
r8 = [this+124]          ; intervalle      ; si 0 -> [this+36] = 0, sortie
si arg == 0 :  [this+128] = maintenant     ; horodatage d'appui
sinon       :  écoulé = maintenant - [this+128] ou [this+132]
               comparé au délai, puis à l'intervalle
               si écoulé < seuil -> sortie
               [this+132] = maintenant
               [this+36]  = [this+28]      ; boutons répétés
```

## 2. Décalages ajoutés

| décalage | contenu |
|---|---|
| `+36` | **sortie « répétition »** (boutons répétés) |
| `+120` | délai initial avant répétition |
| `+124` | intervalle de répétition |
| `+128` | horodatage du dernier appui |
| `+132` | horodatage de la dernière répétition |

## 3. Ce que cela suggère, sans le démontrer

Le menu dispose donc de **deux sorties distinctes** : les fronts d'appui
(`[this+20]`, cycle 425) et la répétition (`[this+36]`, ici). Une interface de
menu utilise typiquement la répétition pour la navigation et les fronts pour la
validation.

Si c'est le cas ici, alors la navigation qui fonctionne emprunte `[this+36]` et
la validation défaillante emprunte `[this+20]` — deux chemins séparés, ce qui
expliquerait qu'une croix directionnelle réponde pendant que les boutons de face
restent inertes.

**Ce n'est pas établi.** Rien de mesuré ne dit lequel des deux champs le menu
consulte. C'est une hypothèse, formulée comme telle, et je l'écris à ce titre
plutôt que de la traiter comme un résultat — la série 394-420 s'est perdue
précisément pour avoir sauté ce pas.

## 4. Test qui trancherait

Relever à l'exécution `[this+20]` et `[this+36]` pour l'emplacement actif
pendant un appui sur A puis sur Gauche. Si A n'apparaît que dans `[this+20]` et
Gauche dans `[this+36]`, l'hypothèse tient et le consommateur de `[this+20]`
devient la cible unique.

`sub_821F3BA0`, la source de temps, mérite le même contrôle : si elle rend une
valeur constante, aucune répétition ne se déclenche jamais.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
