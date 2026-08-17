# Ajouter une frontière de codegen renumérote des preuves déjà publiées

Date : 2026-08-18
Contexte : qualification de l'entrée interne `0x820D3710`, atteinte quand on
appuie sur START pendant `CModeTaskTitleDemoOffline`

## Ce qui a été tenté

`config/confirmed-chunks.toml` est le mécanisme du dépôt pour déclarer une
entrée interne appelable ; il compte 154 entrées. Une 155ᵉ a été ajoutée pour
`0x820D3710` — 8 octets, `lfs f1,0xC(r3)` puis `blr`, propriétaire
`0x820D36F8`, `byte_sha256 edc806bb…`, revérifié par le build contre le
basefile extrait.

Le mécanisme fonctionne exactement comme annoncé : `test_build_demo.py` accepte
l'entrée (72 tests), la régénération complète réussit, et `sub_820D3710`
apparaît dans `ppc_func_mapping.cpp`. L'appui START ne trape plus.

## Ce que cela casse

Ajouter une fonction change la répartition du codegen entre fichiers, donc les
**numéros de ligne du code généré**. Mesuré par instruction, `sub_822F5E58`
et `sub_822F6008` se décalent de +91 lignes dans `ppc_recomp.39.cpp` :

```text
// stw r3,128(r31)      ligne 3948 -> 4039
// lwz r11,128(r31)     ligne 3953 -> 4044
// lhz r11,72(r31)      ligne 4187 -> 4278
```

Or ces numéros ne sont pas internes au build. Ils sont cités :

- par `tests/test_xam_return_chain.py`, qui les épingle comme fixtures et
  échoue (`mapping_error: line outside function source`) ;
- par les reçus publiés sous `analysis/demo/ac6-demo-xam-return-chain-ab/`,
  qui enregistrent `function=… generated_line=…` comme preuve d'un site
  d'accès mémoire.

Rafraîchir les fixtures du test ne suffit donc pas : cela revient à modifier
la description de preuves scellées dont les captures ne peuvent pas être
rejouées à l'identique. Une seule frontière ajoutée invalide la lecture de
toute évidence citant un numéro de ligne généré.

## Décision

La frontière n'est pas ajoutée. `confirmed-chunks.toml` reste à 154 entrées et
le guest est régénéré à l'identique.

Ce n'est pas un défaut du mécanisme : c'est un couplage entre deux choses que
le dépôt traite comme indépendantes — la frontière de codegen et les preuves
qui citent ses lignes. Quiconque ajoute une entrée doit d'abord décider quoi
faire des capsules concernées, et cette décision engage la campagne, pas un
cycle.

## Chemins restants pour `0x820D3710`

- **Le déclarer, puis re-sceller** les fixtures et les capsules qui citent des
  lignes générées, en énonçant que les anciennes lignes décrivent un codegen
  antérieur. Coûteux et à trancher explicitement.
- **Le traiter à l'exécution**, comme `dispatch_reached_chunk_entry` traite
  `0x820E7E08`. Mais cette entrée-là redirige vers une fonction générée
  existante ; ici il n'y en a pas, et exécuter soi-même `lfs`+`blr` dans le
  pont reviendrait à écrire du comportement guest à la main — ce que le
  produit refuse par principe.
- **Faire citer les preuves autrement** : si les capsules identifiaient un
  site par son PC PAL plutôt que par un numéro de ligne généré, le couplage
  disparaîtrait. C'est le seul des trois qui règle la cause.

## L'ampleur : 689 entrées internes, pas une

Mesuré en croisant l'ensemble des fonctions que l'oracle Xenia a traduites —
donc exécutées — avec les débuts de fonction de l'atlas statique :

```text
exécutées par l'oracle           12 922
qui sont des débuts de fonction  12 233
INTERNES (pas un début)             689
```

`0x820D3710` est l'une de ces 689. Le port ne peut donc pas les émettre, et
chacune apparaîtra comme un appel indirect non qualifié dès que le guest
l'atteindra.

Cela change la forme de la décision. Le coût de renumérotation ne dépend pas
du nombre d'entrées ajoutées : une seule le paie en entier. Déclarer les 689
en une passe coûte exactement le même re-scellement que d'en déclarer une, et
évite de le repayer 689 fois.

Ce qui ne change pas : **l'oracle ne peut pas fournir ces frontières.**
`AGENTS.md` interdit explicitement de laisser un oracle imposer ses débuts de
fonction par-dessus les frontières Ghidra. Les 689 adresses sont des
*candidates* — l'oracle dit où regarder — et chacune doit porter sa propre
qualification binaire, du type que les 154 entrées existantes portent déjà
(propriétaire, étendue, terminaison, hash des octets).

La séquence saine est donc : d'abord découpler les capsules du numéro de ligne
généré, ensuite qualifier les entrées par lots, jamais l'inverse.

## Le couplage a déjà mordu, et il ne se signale pas

En cherchant à découpler les capsules avant de toucher aux frontières, il
apparaît que quatre d'entre elles ne sont **déjà plus** résolubles, sans
qu'aucune frontière ait été ajoutée :

```text
ac6-demo-queue-slot-neutral600-v1.json          sub_820FF710:42775
ac6-demo-queue-slot-stores-ab-v1.json           sub_820FF710:42775
ac6-demo-render-queue-slot-write-probe-v1.json  sub_820FF710:42775
ac6-demo-render-queue-write-provenance-v1.json  sub_820FF710:42781
```

`sub_820FF710` commence aujourd'hui **ligne 43191** de `ppc_recomp.7.cpp`. La
ligne 42775 existe toujours dans ce fichier, mais elle appartient à une autre
fonction, et son contenu est `// vmrghw v13,v12,v10` — une instruction VMX,
pas le store de file de rendu que ces capsules décrivent.

C'est la forme la plus dangereuse du problème. Une citation périmée **ne cesse
pas de résoudre** : elle résout vers autre chose. Un lecteur qui suivrait ces
quatre capsules aujourd'hui trouverait une instruction sans rapport et n'aurait
aucun signal l'avertissant que le lien est rompu.

Le codegen n'avait pas besoin d'un changement de frontière pour bouger ; une
version d'outil ou un patch suffit. Un numéro de ligne généré est donc
périssable par nature, et non seulement fragile aux frontières.

`tools/anchor_capsule_generated_lines.py` sert des deux côtés : il ajoute le PC
PAL aux citations encore résolubles, et il **refuse** celles qui ne le sont
plus, ce qui en fait le contrôle qui manquait.
