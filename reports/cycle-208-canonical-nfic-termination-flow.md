# AC6 cycle 208 — canonical NFIC termination flow

## Cible et méthode

- target : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- projet : `workspaces/ace-combat-6/ghidra-projects/ace-combat-6`
- mode : Ghidra headless, `-readOnly -noanalysis`.

Cette passe revalide le chemin NFIC sur l'image canonique. Les rapports anciens
qui utilisaient `ace-combat-6-corrected` ne sont pas réutilisés comme preuve de
frontière.

## Vérification du marqueur `CutTerminate`

Le code canonique contient deux tests exécutables du tag `0x8004` dans la zone
NFIC :

- `0x8236a510` teste l'enregistrement courant lors du balayage du bloc `0x3040`
  et le conserve dans le slot `+0x7c` lorsqu'il s'agit du terminateur;
- `0x8236a550` est l'assistant appelé par les chemins de lecture à
  `0x8236a5dc`, `0x8236a600` et `0x8236a6c4`. Il relit `object+0x80`, compare
  son identifiant 16-bit à `0x8004`, puis avance ou retourne selon la borne du
  tableau.

Le branchement brut `0x8236a708 -> 0x8236a548` est bien présent dans l'image,
mais ne constitue pas une référence Ghidra nommée. La passe canonique
`ReferencesTo` trouve les trois appels directs vers `0x8236a548` aux sites
`0x8236a5dc`, `0x8236a600` et `0x8236a6c4`.

Ces fonctions bornent la détection et l'itération du terminateur; elles ne
publient ni sélecteur de mission, ni ressource DPL, ni objet joueur/vol.

## Producteurs de structures NFIC

Le dispatcher de construction autour de `0x8236ad70..0x8236adc4` initialise
plusieurs sous-structures (`0x3000`, `0x3021`, `0x3022`, `0x3031`, `0x3040`),
puis appelle respectivement les parseurs `0x8236a720`, `0x8236a890`,
`0x8236aa00`, `0x8236a478` et `0x8236ab70`. Cette composition établit une
initialisation de ressources/chunks, pas une transition post-CUT.

Le parseur `0x8236a720` effectue une recherche de `0x3021`, construit ses
records et appelle un slot virtuel `+0x04` d'un objet retourné par
`0x82365d58`. Le slot et l'objet sont dynamiques; aucun lien vers le current-
level, le propriétaire `DAT_8293BA10`, `CutTerminate` ou un groupe de mission
n'est démontré.

La recherche canonique de triples `lwz [vptr] / lwz +0x20 / mtspr CTR` ne
produit aucun site dans la zone NFIC `0x82360000..0x82375000`. Les dispatchs
virtuels de cette zone utilisent d'autres slots et ne peuvent pas être
assimilés au receiver partagé du mode.

## Qualification et limite

- `confirmed` : tag `0x8004` comme terminateur NFIC, tests canoniques
  `0x8236a510/0x8236a550`, et appels directs vers l'assistant;
- `cross-match` : composition des chunks `0x3000/0x3021/0x3022/0x3031/0x3040`
  dans la zone canonique;
- `unknown` / `needs-dynamic-evidence` : type des receivers virtuels,
  consumer métier du terminateur, mapping vers un groupe Scene, mission et
  démarrage du vol.

Le verrou post-CUT reste donc ouvert. La frontière native ne doit pas dépasser
`scene_complete`, et aucune action humaine n'est requise pour cette tranche.

## Validation

- `DumpProgramIdentity.java` : projet canonique, `.text`
  `0x82090000..0x823d772c`;
- `FindMemoryScalarInRange.java` sur le `.text` pour `0x8001`, `0x8003` et
  `0x8004`;
- `DumpRange.java` `0x8236a300..0x8236a700`, `0x8236a6f8..0x8236a770`,
  `0x8236a720..0x8236ab80` et `0x8236ad70..0x8236ae20`;
- `ReferencesTo.java`, `FindDirectCallsTo.java` et
  `FindPpcRawBranchesTo.java` pour `0x8236a548`, `0x8236a720` et
  `0x8236ad94`;
- `FindVirtualDispatchTriples.java` sur le `.text` canonique;
- toutes les commandes en lecture seule, sans import du projet corrigé.
