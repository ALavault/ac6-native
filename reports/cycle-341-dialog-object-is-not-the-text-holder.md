# Cycle 341 — l'objet du dialogue ne porte pas le texte ; deux hypothèses tombent

## 1. Test de la frontière parasite : non concluant, et pourquoi

Le cycle 340 proposait de retirer `0x821CE8E0` de `[functions]` et de mesurer.
Fait : **la génération s'interrompt** (`std::bad_alloc`). Cette frontière est
**porteuse pour le générateur** — elle ne peut pas être simplement retirée, et
le test tel que spécifié n'est pas exécutable.

L'hypothèse « la coupure parasite cause le dialogue vide » n'est donc **ni
confirmée ni réfutée** : elle est *intestable par cette voie*. La tester
suppose de retirer la coupure **et** de remplacer ce sur quoi le générateur
s'appuie — travail de générateur, pas de configuration.

Coût annexe, à retenir : la génération avortée laisse le glob CMake périmé, et
la reconstruction échoue ensuite sur `undefined reference to PPCImageConfig` —
une erreur qui désigne le mauvais coupable. **Après une génération échouée,
reconfigurer avant de reconstruire.** Arbre restauré et vérifié : `eop` 6877,
`host_swap_presents` 2292, 0 `FATAL`.

## 2. Sonde sur l'objet du dialogue

`[[midasm_hook]]` en entrée de `sub_821CE8A8`, vidant l'objet reçu en `r3` :

```
obj=0xA3300060 arg=0x00000000
 +0  =820679F4  +4  =FEFEFEFE  +8  =F8000148  +12 =00000000
 +16 =0001FEFE  +20 =00000001  +40 =55736572  +84 =00000000
 +88..+116 = FEFEFEFE
 seul pointeur invité résolu : +0 -> 0x820679F4 (table virtuelle, [0]=0x821CE030)
```

Lectures :

- `+0` est une **table virtuelle**, pas du texte ;
- `+40` = `0x55736572` = **`"User"`** en ASCII — donnée de chaîne *en ligne*,
  pas un pointeur ;
- `FEFEFEFE` est un motif de remplissage **non initialisé**, attendu en `+88`
  et suivants puisque `sub_821CE8A8` s'apprête à les écrire ;
- **aucun autre pointeur invité dans les 128 premiers octets.**

**L'objet ne porte aucun champ de texte.** L'hypothèse du cycle 340 — un
pointeur de texte nul dans l'objet du dialogue — est **réfutée**. Cet objet est
celui de l'utilisateur / du stockage, ce que confirment `"User"` et l'appel à
`XamShowDeviceSelectorUI`. La sonde ne se déclenche **qu'une fois**.

## 3. Deuxième hypothèse, également réfutée

`"User"` suggérait une invite de sauvegarde sans profil connecté — ce qui
aurait expliqué d'un coup le texte absent et les boutons inertes.

`user_profile.h:215` : `uint32_t signin_state() const { return 1; }`.
**Un utilisateur est rapporté connecté**, en dur. L'hypothèse tombe.

## 4. Où cela laisse P1.3

Le chemin partant de `XamShowDeviceSelectorUI` mène à la routine de
**stockage/utilisateur**, pas au rendu du dialogue. Il est épuisé comme piste
vers le texte.

Éliminés par mesure à ce stade, pour le texte manquant :

| candidat | statut |
|---|---|
| boîte de dialogue ImGui du runtime | écarté (cycle 338) |
| table de chaînes XDBF | écarté (cycle 338) |
| périphérique de stockage | écarté (cycle 337) |
| lecture de données échouée ou courte | écarté (cycle 338, 0 occurrence) |
| pointeur de texte nul dans l'objet | **écarté (ici)** |
| absence de profil connecté | **écarté (ici)** |
| frontière parasite `0x821CE8E0` | **intestable par retrait** |

## 5. Front suivant

La piste par les exports XAM est close. Il faut atteindre le **rendu** :

1. Localiser la fonction invitée qui dessine « YES » / « NO » — ce texte-là
   s'affiche, donc son chemin fonctionne et il partage sûrement le
   sous-système de la chaîne manquante.
2. L'oracle émulateur reste le moyen le plus direct de savoir si ce dialogue a
   du texte en fonctionnement nominal. Toujours **non préparé**. Contrainte du
   cycle 316 : Xenia sous Xvfb ne rend rien, il faut un affichage headless
   accéléré GPU ; jamais la session de l'opérateur ; profil Xenia à préserver.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
