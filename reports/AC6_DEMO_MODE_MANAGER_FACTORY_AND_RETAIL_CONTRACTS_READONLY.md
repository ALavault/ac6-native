# AC6 démo — factories de `CTaskModeManager` et maturité des contrats retail

> **QUALIFIÉ ET SUPERSEDÉ (2026-08-23).** Cette piste préparatoire avait raison
> sur le flux principal, mais seule la reprise Ghidra canonique fait autorité.
> Elle confirme le store `this+0x08` et l'insertion via `0x82822F08` dans la
> phase de complétion de `0x82190B18`, et ferme statiquement le type
> `0x827435F8 : CTaskModeManager*`. Verdict et nuances des deux phases :
> `artifacts/goal-playable/frontend-mode-manager-canonical-ghidra-20260823/RESULT.md`
> avec `analysis/demo/ac6-demo-structural-types-v1.json`.

Date : 2026-08-23  
Statut : analyse statique read-only ; aucune exécution runtime et aucune modification des sources  
Cible démo : `Default.xex`, SHA-256 `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`  
Projet Ghidra canonique à employer pour la qualification finale : `ghidra-projects/ace-combat-6-demo`

## Résultat principal

Plusieurs reçus existants confondent la factory de mode avec l'objet construit par cette factory. Le cross-match littéral CFG/ABI du C++ généré, combiné aux preuves RTTI et Ghidra déjà qualifiées, donne la chaîne suivante :

```text
CTaskLoading::slot+0x48, 0x8217E890(index=0)
  -> accès de table global 0x8218E970
  -> [0x82391F0C] = factory 0x82192780
  -> setter d'instance 0x8218EA88
  -> CTaskModeManager+0x10 = callback de factory par défaut

CTaskModeManager::Update, 0x821929A8
  -> méthode de transition d'instance 0x82190B18
  -> choisit manager+0x14 si non nul, sinon manager+0x10
  -> invoque la factory 0x82192780
  -> allocation puis constructeur 0x8218D688
  -> objet CModeTaskGameDemoOffline
  -> manager+0x08 = objet courant
  -> [0x82822F08]->slot+0x0C, insertion qualifiée à 0x82259E18
```

`0x8218E970` ne construit donc pas l'objet : il vérifie l'index puis retourne une entrée de la table commençant à `0x82391F0C`. `0x8218EA88` effectue seulement `stw r4, 0x10(r3)`. L'objet n'apparaît qu'au moment où `0x82190B18` invoque la factory et range son résultat dans `manager+0x08`.

Cette chaîne ferme statiquement l'ancien « consumer missing » entre la publication et l'insertion du mode. Elle ne ferme pas le prédicat guest qui demande naturellement la transition.

## Classification objet/méthode/attribut/global

| Élément | Classification proposée | Confiance | Motif |
|---|---|---:|---|
| `0x8217E890` | méthode virtuelle de `CTaskLoading`, slot `+0x48` | haute | reçoit `this`, sélectionne une entrée de factory et l'installe dans le manager |
| `0x8218E970` | fonction globale d'accès à une table de factories | haute | aucune utilisation d'un `this`; retourne une entrée indexée de `0x82391F0C` |
| `0x8218EA88` | setter non virtuel d'instance de `CTaskModeManager` | haute | écrit directement l'argument dans `this+0x10` |
| `0x82192780` | factory globale retournant un `CModeTaskGameDemoOffline*` | haute | allocation de 136 octets puis appel du constructeur `0x8218D688` |
| `0x82190B18` | méthode non virtuelle de transition/remplacement de mode sur `CTaskModeManager` | haute | lit et modifie plusieurs champs du même receiver, invoque la factory et publie son résultat |
| `0x821929A8` | méthode virtuelle d'update de `CTaskModeManager`, slot `+0x10` | haute | vtable RTTI `0x82011694`; appelle `0x82190B18` quand `this+0x18` est actif |
| `0x82192850` | méthode virtuelle d'initialisation/démarrage du manager, slot `+0x0C` | moyenne-haute | initialise un mode par défaut mais n'écrit pas elle-même la vtable; « constructeur » serait trop fort |
| `0x827435F8` | cellule globale `CTaskModeManager*` | haute | l'objet observé porte la vtable `0x82011694` et son slot d'update pointe sur `0x821929A8` |
| `manager+0x08` | pointeur vers le mode courant construit | haute | reçoit le retour de la factory avant insertion |
| `manager+0x0C` | pointeur vers le mode précédent/sortant | moyenne-haute | reçoit l'ancien `+0x08`, est désinséré/nettoyé puis remis à zéro |
| `manager+0x10` | callback de factory par défaut | haute | écrit par `0x8218EA88`, puis appelé indirectement par `0x82190B18` |
| `manager+0x14` | callback de factory de transition ponctuel | haute | prioritaire sur `+0x10`, puis effacé après consommation |
| `manager+0x18` | requête ou état de transition | moyenne | déclenche le chemin dans l'update; la signification métier exacte reste ouverte |

La preuve RTTI existante rattache le constructeur `0x8218D688` à la vtable `0x820107D4`, soit `CModeTaskGameDemoOffline`. La cellule de service `0x82822F08` est consommée virtuellement au slot `+0x0C`; l'insertion `0x82259E18` est déjà qualifiée dans les reçus existants.

## Preuves littérales consultées

Le C++ généré n'est utilisé ici que comme cross-match littéral de contrôle de flux et d'ABI, conformément aux règles du projet :

- `recompilation/ace-combat-6-demo/build-codegen-on/codegen/generated/ppc_recomp.14.cpp:33533` — `0x8217E890` ;
- `recompilation/ace-combat-6-demo/build-codegen-on/codegen/generated/ppc_recomp.16.cpp:7035` — `0x8218E970` ;
- `recompilation/ace-combat-6-demo/build-codegen-on/codegen/generated/ppc_recomp.16.cpp:7206` — `0x8218EA88` ;
- `recompilation/ace-combat-6-demo/build-codegen-on/codegen/generated/ppc_recomp.16.cpp:12559` — `0x82190B18` ;
- `recompilation/ace-combat-6-demo/build-codegen-on/codegen/generated/ppc_recomp.17.cpp:825` — `0x82192780` ;
- `recompilation/ace-combat-6-demo/build-codegen-on/codegen/generated/ppc_recomp.17.cpp:965` — `0x82192850` ;
- `recompilation/ace-combat-6-demo/build-codegen-on/codegen/generated/ppc_recomp.17.cpp:1160` — `0x821929A8`.

Artefacts de qualification croisés :

- `analysis/demo/ac6-demo-rtti-atlas-v1.json` ;
- `artifacts/goal-playable/game-entry-followup/RESULT.md` ;
- `artifacts/goal-playable/mode-dispatch-consumer/RESULT.md` ;
- `artifacts/goal-playable/task-loading-publication/RESULT.md` ;
- `artifacts/goal-playable/mission-transition-static-20260823/RESULT.md` ;
- `reports/cycle-1777-demo-task-render-queue-ab.json`.

Avant de changer le ledger sémantique, le main thread doit refaire la qualification explicite dans `ace-combat-6-demo`, avec le module, le SHA-256 et chaque adresse. Le projet retail ne peut pas qualifier cette chaîne démo.

## Éléments existants à corriger ou superséder après qualification Ghidra

Les affirmations suivantes sont devenues suspectes ou fausses :

- `0x8218E970` « construit » le mode ;
- `0x8218EA88` « publie l'objet construit » dans `manager+0x10` ;
- `manager+0x10` contient un objet de mode ;
- `0x82190B18` est une fonction globale ;
- `0x827435F8` est seulement un `FrontendGlobalContext` générique ;
- le consumer de la valeur publiée serait encore inconnu.

Le fichier `analysis/demo/ac6-demo-structural-types-v1.json` et les reçus listés ci-dessus devront être corrigés ou explicitement supersédés, mais seulement après la requalification canonique. Aucun shim runtime, changement d'import ni contrat ABI nouveau n'est justifié par cette seule correction sémantique.

## Maturité des contrats retail

Les quatre contrats retail inspectés sont techniquement cohérents et utiles comme catalogues d'hypothèses :

| Contrat | Contenu et maturité utile | Usage recommandé pour la démo |
|---|---|---|
| `analysis/contracts/mission01-playable-gate-v1.json` | 34 comportements passés; catalogue le plus large malgré le suffixe `v1` | input, vol, terrain, carte, transformations et renderer |
| `analysis/contracts/mission01-native-gate-v2.json` | gates J0/J1 passées; huit domaines plus pause/save/restart | modèle des domaines d'observation, sessions et gates natives |
| `analysis/contracts/mission01-final-gate-v3.json` | huit comportements centraux passés | FSM, compteurs, objectifs, construction des unités et terminal |
| `analysis/contracts/mission01-visible-gate-v4.json` | dix comportements passés; couverture visuelle spécialisée | hypothèses NTXR, NDXR, textures, conteneurs et géométrie |

Le numéro de version n'est pas un classement de maturité : `playable-v1` est le catalogue sémantique le plus riche car il a été construit et étendu à partir des travaux précédents; `visible-v4` est volontairement plus spécialisé.

### Résultats des audits read-only

- audits de gate : tous valides pour les jalons demandés (`JF`, et `retail` pour native-v2) ;
- artefacts : 4 contrats valides, 146 artefacts cités uniques, 146 identiques au `HEAD` ;
- adresses : 321 citées, 321 supportées, 0 non supportée ;
- dérivations : 52 comportements, 0 gap et 0 dérivation multiple ambiguë ;
- références vérifiées par contrat : 128/128 pour playable-v1, 25/25 pour native-v2, 26/26 pour final-v3 et 38/38 pour visible-v4.

Ces contrats qualifient le retail PAL, SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`, et non la démo. Ils peuvent accélérer la décompilation en fournissant :

1. des noms de concepts et de domaines à rechercher ;
2. des formes attendues de producteurs/consommateurs ;
3. des schémas de tests et de gates déjà éprouvés ;
4. des hypothèses de formats et de pipelines à réfuter ou confirmer.

Ils ne transfèrent aucune preuve automatiquement. Chaque adresse, offset, ID de contenu, index de fichier, disposition de structure et hypothèse de parsing doit être réétabli sur le XEX et les neuf fichiers de la démo, dans son projet Ghidra canonique.

## Frontière encore ouverte

Le consumer de factory et l'insertion du mode sont désormais expliqués statiquement. La frontière utile devient le producteur guest de la requête de transition :

- l'état loading `2` écrit statiquement `manager+0x18 = 1` après son compte à rebours ;
- la route observée précédemment suit `0 -> 4 -> 1` et ne passe pas par cet état `2` ;
- il reste donc à prouver quel prédicat naturel permet d'atteindre cet état ou quel autre producteur légitime arme `+0x18`.

Cette frontière est postérieure au gate renderer SWG courant et ne doit pas le détourner.

## Prompt compact pour le main thread

```text
Piste read-only à intégrer après le gate SWG courant. Sur la démo PAL de917... et le projet Ghidra ace-combat-6-demo, requalifier puis corriger CTaskModeManager : 0x8218E970 ne construit rien, il retourne la factory [0x82391F0C]=0x82192780; 0x8217E890 la range via 0x8218EA88 dans manager+0x10. 0x82190B18, méthode d'instance appelée par l'update virtuel 0x821929A8, choisit +0x14 sinon +0x10, invoque la factory, range l'objet dans +0x08 puis l'insère via [0x82822F08]->slot+0x0C (0x82259E18). Reclasser 0x8218EA88 en setter d'instance, 0x82190B18 en méthode d'instance et 0x827435F8 en CTaskModeManager*; ne créer aucun shim sur cette seule preuve. Superséder les reçus/structural-types erronés après qualification Ghidra. Les 4 contrats retail sont audit-valid : utiliser playable-v1 pour input/vol, native-v2 pour domaines/session, final-v3 pour FSM/objectifs et visible-v4 pour NTXR/NDXR, uniquement comme hypothèses à revalider sur la démo. Le point encore ouvert est le prédicat guest qui arme manager+0x18.
```
