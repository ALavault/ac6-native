# Cycle 333 — la porte P0 est franchie : le jeu rend et présente

## 0. Résultat

| observable | exigé | mesuré |
|---|---:|---:|
| `eop` monotone | > 100 | **9 094** |
| `host_swap_presents` | > 600 | **3 029** |
| `write_ptr` avance | oui | **`0x8E39`**, `rptr` `0x8E21` |
| `REX_FATAL` | 0 | **0** |

Reproduit sur une seconde exécution indépendante : `eop` 8 194,
`host_swap_presents` 2 730, `wptr` `0x8029` / `rptr` `0x8011`.

Trois captures espacées de 12 s **diffèrent toutes** (empreintes et tailles
distinctes : 367 Ko, 412 Ko, 606 Ko). Le contenu présenté **change dans le
temps** — critère P1.1.

La capture montre une vue aérienne de ville rendue en 1280x720, **29,76 im/s**,
renderer Vulkan, 127 519 dessins hôte, audio actif. **Le jeu rend son contenu.**

## 1. La cause du dernier blocage : un double échange d'octets

Le cycle 332 constatait que le correctif réseau ne s'activait pas depuis le SDK
alors que sa chaîne était bien dans le binaire, et laissait trois candidats
indépartagés. Une trace inconditionnelle en tête de `XSocket::Bind` les a
départagés en une exécution :

```
XSocket::Bind trace: ret=-1 errno=13 (EACCES) sin_port_read=999
                     ntohs(sin_port_read)=59139 raw_port_bytes=03E7 name_len=16
```

- `errno` **était** `EACCES` — candidat 1 réfuté ;
- le `#if` **n'éliminait pas** la branche — candidat 3 réfuté ;
- `sin_port` est un `rex::be<uint16_t>` : **le lire rend déjà l'ordre hôte**,
  soit 999. Mon `ntohs()` l'échangeait une seconde fois, donnant **59 139**,
  au-dessus de 1024, donc `IsPrivilegedPort` était faux et le repli ne
  s'exécutait jamais. **Candidat 2 confirmé.**

Les octets bruts `03 E7` sont bien 999 en ordre réseau : le format sur le fil
était correct depuis le début, seul mon double échange était faux. Corrigé en
lisant `name->sin_port` directement, et en écrivant `shifted.sin_port =
host_port` sans `htons` — un champ `be<>` échange déjà à l'écriture.

## 2. Ce qu'il a fallu, au total

Quatre défauts distincts, chacun masquant le suivant :

1. **Port privilégié** — l'invité lie l'UDP 999 ; Linux refuse sous 1024
   (`EACCES`), la 360 n'a pas cette règle. Réception infinie sur une socket non
   liée. Corrigé sans privilège : essai du port réel, repli décalé sur `EACCES`.
2. **`FIONBIO` Winsock** — `0x8004667E` transmis tel quel à `ioctl()`. Traduit
   en `fcntl(O_NONBLOCK)`, argument invité big-endian échangé.
3. **Tables de saut tronquées** — le scanner nomme comme registre d'index le
   registre qui porte l'**adresse** chargée, ce qui à la fois fait porter le
   `switch` sur l'adresse et fait tronquer la table. Corrigé en aiguillant sur
   `ctr` (correct par construction) et en fournissant les tables **mesurées** en
   mémoire invitée pour les deux dispatchers atteints.
4. **Frontières de fonctions** — deux entrées `[functions]` pour les pièges que
   l'invité atteint une fois qu'il va plus loin (méthode des cycles 306-307).

## 3. Ce que le mécanisme « échouer bruyamment » a rapporté

Le correctif du cycle 329 — remplacer le `ud2` muet par un `REX_FATAL` nommé — a
directement produit le dernier pas :

```
[FATAL] Unlisted jump-table target 0x8237bf38 at bctr 0x8237BF30
```

Une ligne, l'adresse du branchement et la cible manquante. Le deuxième
dispatcher dégénéré s'est déclaré lui-même au lieu de faire tourner un cœur en
silence. Sans lui, ce cycle aurait recommencé un profil et une passe
d'annotation.

## 4. Validation exécutée

```text
trace XSocket::Bind                          cause départagée en 1 exécution
correctif du double échange                  remap actif, "bound 40999 instead"
corpus régénéré                              rc=0, 52 unités
runtime reconstruit                          lié, 0 erreur
exécution 1, 80 s                            eop 9094, presents 3029, 0 FATAL
exécution 2, 60 s                            eop 8194, presents 2730, 0 FATAL
3 captures à 12 s d'intervalle               toutes différentes
contenu                                      ville aérienne, 1280x720, 29,76 im/s
audio                                        actif, 10 108 consommés
```

## 5. Portée honnête

La porte **P0** est franchie et reproductible. Ce n'est **pas** la livraison :

- P1.2 — l'entrée modifie l'état présenté : **non testé** ;
- P1.3 — traverser jusqu'au sélecteur de campagne : **non testé** ;
- P2 — entrée 9, chargement de la première mission, monde non vide : **non fait** ;
- P3 à P7 — renderer complet, vol, audio en mission, parité, durcissement.

« Jouable » et « parité retail » restent interdits d'emploi.
`recompiler-generated` n'est pas `verified`.

## 6. Règle ajoutée

**Un type d'accès qui échange déjà les octets ne doit pas être repassé par
`ntohs`/`htons`.** Le défaut était invisible à la lecture parce que
`ntohs(port)` *paraît* correct partout ailleurs ; seul l'affichage simultané de
la valeur lue, de sa version échangée et des octets bruts l'a montré. Tracer les
trois formes d'une même donnée coûte une ligne et tranche une classe entière
d'erreurs d'endianness.
