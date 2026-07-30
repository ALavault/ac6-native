# Cycle 325 — P0.1 fermée : c'est l'invité qui cesse de soumettre, l'anneau est vidé

## Cible

- Produit : AC6 Xbox 360 PAL, Xenon PPC big-endian, Xenos
- Module : `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Base image : `0x82000000`
- Exécutable natif : `e639a9fe5cd0504a416750c18005d3aa07a872f26763c61fa4f2dd0fa83a3b05`
- Route : `dynamic`

## 0. Objet

Premier cycle du `PLAYABLE_PLAN.md`. Sa phase P0 §2 posait trois causes
mutuellement exclusives au gel de la boucle de trame, dont **aucune n'avait
jamais été mesurée** :

- **(a)** l'invité cesse d'appeler `UpdateWritePointer` ;
- **(b)** l'invité continue mais l'hôte ne consomme pas ;
- **(c)** l'hôte consomme mais n'exécute plus les paquets utiles.

## 1. Instrument

Tout ce que l'invité demande au GPU arrive comme paquets PM4 dans l'anneau de
commandes. Six compteurs posés **dans l'arbre**, sur la frontière de soumission,
et émis par la ligne de télémétrie existante :

| compteur | site | ce qu'il prouve |
|---|---|---|
| `wptr_updates`, `wptr` | `CommandProcessor::UpdateWritePointer` | l'invité a mis du travail en file |
| `rptr`, `primary_executions` | après `ExecutePrimaryBuffer` | l'hôte a drainé l'anneau |
| `pm4_interrupt` | `ExecutePacketType3_INTERRUPT` | le paquet qui lève une EOP (source 1) |
| `pm4_swap` | `ExecutePacketType3_XE_SWAP` | le paquet qui demande une présentation |

Chaîne relue dans le SDK, sans hypothèse :

```text
invité écrit le registre GPU CP_RB_WPTR, indice 0x01C5, soit 0x7FC80714
  -> GraphicsSystem::WriteRegister -> CommandProcessor::UpdateWritePointer
     write_ptr_index_ = value ; write_ptr_index_event_->Set()
worker CP : tant que read_ptr == write_ptr, il tourne puis attend 5 ms
            sinon ExecutePrimaryBuffer(read, write)
  paquet PM4 INTERRUPT -> DispatchInterruptCallback(1, cpu)  = EOP
  paquet PM4 XE_SWAP   -> IssueSwap(...)                     = présentation
```

Artefact : `patches/rexglue-in-tree-frame-loop-telemetry-20260730.patch`
(mis à jour).

## 2. Mesure

`tools/ac6-frame-loop-probe.sh p01-ring 75`, 15 lignes de télémétrie sur 75 s.

```text
premier (vblank=300)  ring wptr_updates=20 wptr=00000043 rptr=00000043
                           primary_executions=11 pm4_interrupt=12 pm4_swap=4
dernier (vblank=4500) ring wptr_updates=20 wptr=00000043 rptr=00000043
                           primary_executions=11 pm4_interrupt=12 pm4_swap=4
```

Tout est **gelé dès le premier échantillon**, à t+5 s, et reste identique
70 secondes plus tard alors que la source 0 passe de 300 à 4 500.

## 3. Résultat : cause (a), et l'hôte est hors de cause

Trois lectures, chacune directe :

1. **`wptr == rptr == 0x43`.** L'anneau est **entièrement drainé** : l'hôte a
   consommé exactement tout ce que l'invité avait mis en file. Il n'y a pas de
   travail en attente que l'hôte négligerait.
2. **`wptr_updates` gelé à 20.** L'invité **ne réécrit plus** le pointeur
   d'écriture. Il ne soumet plus rien.
3. **`pm4_interrupt=12` est cohérent avec `eop=12`**, et `pm4_swap=4` avec
   `guest_swap_requests=4`. Le décodage des paquets n'a rien perdu.

Donc **(b) et (c) sont réfutées par mesure**, et **(a) est établie** : le défaut
est en amont, dans la logique invitée. Confiance : `confirmed` pour l'état de
l'anneau et pour l'exclusion de (b) et (c) ; le *pourquoi* invité reste ouvert.

Cela ferme aussi, pour de bon, une famille entière d'hypothèses côté hôte : ni la
livraison d'interruptions, ni le worker CP, ni le décodage PM4, ni le chemin de
présentation ne retiennent quoi que ce soit. La cohérence
`wptr == rptr` est la preuve la plus forte disponible : l'hôte n'a rien en main.

Note incidente : `write_ptr_index_event_` est un événement auto-reset, donc de la
famille corrigée au cycle 323 — mais le worker CP l'attend avec un délai de 5 ms
et revérifie le pointeur, donc un réveil perdu ne coûterait ici que 5 ms. Le
défaut du cycle 323 **n'est pas** l'explication de ce gel.

## 4. Ce que P0.2 doit faire, et l'instrument est identifié

La question devient : **l'invité exécute du code — 33 threads, dont plusieurs
consomment du CPU — sans appeler aucun service ni soumettre au GPU. Quel code ?**

La plateforme interdit les instruments habituels : `perf_event_paranoid = 4`
bloque tout profilage, `yama/ptrace_scope = 1` interdit `gdb -p`, `eu-stack -p`
et `perf -p` sur un processus déjà lancé. Deux routes, dans l'ordre de coût :

1. **Histogramme des cibles d'appel indirect, sans ptrace.** Les 14 111 sites
   d'appel indirect du corpus passent tous par un unique point d'étranglement :
   `PPC_CALL_INDIRECT_FUNC(x)` → `PPC_LOOKUP_FUNC(base, x)(ctx, base)`
   (`include/rex/ppc/context.h:131`). Compter `x` par adresse invité y nomme la
   boucle chaude sans débogueur. Attention : ce macro est sur un chemin
   ultra-chaud, donc l'instrument doit être compilé conditionnellement, jamais
   actif par défaut, et son coût mesuré avant d'en lire les résultats.
2. Si l'histogramme ne suffit pas — boucle purement intra-fonction, donc sans
   appel indirect — demander les deux sysctl, ou payer un `gdb --args` unique
   avec `thread apply all bt` après interruption, en budgétant le chargement des
   symboles du binaire LTO de 165 Mo.

## 5. Modifications

- `thirdparty/rexglue-sdk/src/graphics/graphics_system.cpp`,
  `include/rex/graphics/graphics_system.h`,
  `src/graphics/command_processor.cpp` : six compteurs d'anneau, émission
  étendue. Aucun changement de comportement : ce sont des incréments d'atomiques
  relaxés sur des chemins qui font déjà bien davantage.
- `patches/rexglue-in-tree-frame-loop-telemetry-20260730.patch` régénéré.
- `PLAYABLE_PLAN.md` : P0.1 fermée, cause (a) inscrite.
- Aucune sortie générée éditée. Configuration inchangée.

## 6. Validation exécutée

```text
build                                        rc=0, 0 erreur
binaire                                      e639a9fe…3b05
sonde 75 s                                   15 lignes, 0 REX_FATAL
anneau                                       wptr == rptr == 0x43, drainé
cohérence pm4_interrupt / eop                12 == 12
cohérence pm4_swap / guest_swap_requests     4 == 4
valeur invité 0x82870828                     0, stable (inchangé)
garde de fuite                               431 Mio récupérés
```

Ni preuve de jouabilité, ni preuve de parité retail.
`recompiler-generated` n'est pas `verified`.

## 7. Porte P0

**Non franchie.** Elle exige, sur 60 s : `wptr` qui avance continûment, `eop`
monotone au-delà de 100, et `host_swap_presents` au-delà de 600. Mesuré :
`wptr` gelé, `eop` 12, `host_swap_presents` 3.

P0.1 est fermée ; P0.2 est ouverte et son premier instrument est nommé.
