# AC6 cycle 206 — propriétaire partagé canonique et receiver `+0x8`

## Cible et méthode

- target : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- image base : `0x82000000`

Passe statique headless en lecture seule sur le projet Ghidra canonique
`workspaces/ace-combat-6/ghidra-projects/ace-combat-6`. Les résultats du projet
historique `ace-combat-6-corrected` n'ont pas été mélangés à cette passe.

## Initialisation canonique du propriétaire

Le balayage des références à `DAT_8293BA10` confirme une écriture canonique à
`0x821b95c0` :

```text
0x821b95b8  lis r11,-0x7d6c
0x821b95bc  li  r4,0
0x821b95c0  stw r30,-0x45f0(r11)   ; DAT_8293BA10 <- r30
```

Le bloc d'initialisation non couvert par une `Function` Ghidra commence à
`0x821b9408`. Il construit un objet dont le vtable pointer est
`0x82065ac4`, initialise notamment `owner+0x4 = 1`, `owner+0xc = 0` et
`owner+0x8 = 0`, puis publie cet objet dans `DAT_8293BA10` à `0x821b95c0`.
Ce bloc est conservé comme instruction island : il ne doit pas être attribué
à une fonction ou à une sémantique métier sans récupération supplémentaire de
sa frontière.

## Trace `owner -> receiver -> vtable +0x20`

Sur le projet canonique, `TraceGlobalReceiverDispatches.java` trouve **13**
dispatches statiques qui suivent l'idiome exact :

```text
lwz owner, DAT_8293BA10
lwz receiver, owner+0x8
lwz vtable, receiver+0
lwz slot, vtable+0x20
call slot(r4 = 3)
```

Les fonctions sont :

```text
0x82197c08  0x82198050  0x821966f0  0x8219a378
0x821ade00  0x821ae140  0x821afca0  0x821b01e8
0x821b04d0  0x821b5690  0x821b5a48  0x821b5f80
0x821b6668
```

Chaque site publie auparavant `owner+0x18 = 1` et `owner+0x1c = 3`, puis
teste le receiver avant l'appel. La répétition dans des familles de tâches
distinctes confirme une notification de mode partagée, mais n'identifie pas
un passage title-to-campaign.

## Recherche d'affectation ultérieure de `owner+0x8`

Les recherches suivantes ont été exécutées sur l'image canonique :

```text
FindGlobalFieldWrites.java 0x8293ba10 0x8
FindGlobalPointerFieldStores.java 0x8293ba10 0x8
TraceGlobalReceiverDispatches.java 0x8293ba10 0x8
ReferencesTo.java 0x8293ba10
TraceGlobalOwnerFlow.java 0x8293ba10
```

Résultat :

- `FindGlobalPointerFieldStores` ne produit aucun `CANDIDATE`;
- le traceur de flux global, qui parcourt aussi les instructions exécutables
  hors des fonctions Ghidra, ne produit aucun `DIRECT_STORE` vers `+0x8`;
- les stores `stw ...,0x8(r3)` relevés dans les fonctions `0x82196430`,
  `0x821b5540`, `0x821ae140` et `0x821b5808` écrivent dans des objets locaux ou
  dans un service obtenu via un autre champ; ils ne sont pas démontrés comme
  des stores `owner+0x8`;
- `ReferencesTo` révèle uniquement la publication du pointeur global à
  `0x821b95c0` comme écriture directe de `DAT_8293BA10`.

Le nouveau traceur est
`workspaces/ace-combat-6/scripts/TraceGlobalOwnerFlow.java`. Il reste un
chercheur de candidats, sans inférence de type, de callee ou de campagne.

## Qualification et limite

- `confirmed` : constructeur/initialisation du propriétaire canonique,
  `owner+0x8 = null` initial, publication globale, et 13 dispatches
  owner/receiver/vtable reproduisant le même argument `3`;
- `cross-match` : répétition du signal `{owner+0x18=1, owner+0x1c=3}` dans les
  tâches de mode;
- `unknown` / `needs-dynamic-evidence` : affectation runtime ultérieure de
  `owner+0x8`, type du receiver et implémentation de son slot `+0x20(3)`.

Cette passe améliore la couverture canonique mais **ne ferme pas** le verrou
post-CUT. Aucun lien vers `0x820a85e0`, le sélecteur `1`, `CutTerminate`, un
groupe de mission ou un objet de vol n'est démontré. L'exécutable natif reste
donc borné à `scene_complete`.

Aucune action humaine n'est requise pour cette tranche : la limite est statique
et la prochaine piste reste l'analyse du producteur du sélecteur `1` ou du
consommateur de `CutTerminate`, sans lancer de session interactive.

## Validation

- `analyzeHeadless ... -readOnly -noanalysis` sur chaque script ci-dessus;
- aucune base Ghidra, sortie générée ou binaire n'a été modifié;
- le script ajouté compile et s'exécute avec code retour nul;
- CTest AC6 : `ctest --test-dir .build/ace-combat-6/native
  --output-on-failure` → **41/41 PASS** (16,34 s);
- contrôle oracle : `workspaces/ace-combat-6/scripts/launch_xenia_ac6_wine.sh
  check` → `status=ready`, `release=16e1eb8`, `renderer=vulkan`,
  `service=ac6-xenia-wine-gui.service`;
- `git diff --check` → PASS.
