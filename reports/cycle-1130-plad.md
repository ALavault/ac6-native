# Cycle 1130 — `PLAD` : une position monde qui n'est pas dans le conteneur

Date : 2026-08-08. Cycle autonome. Un fichier de 32 octets qu'aucun cycle n'avait
ouvert.

## Qualification

- Image : Xbox 360 PAL `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Données : `DATA00.PAC` entrée 9, conteneur FHM à 26 enfants, extrait au cycle
  739 ; enfant 0 = le conteneur de scénario, enfant 1 = `MDLP`, **enfant 2 =
  `PLAD`, 32 octets**.
- **Statique seul.** Aucun oracle.

## Le changement de méthode

Six cycles ont cherché le placement dans le code, et six ont éliminé. Celui-ci a
regardé **ce qui est livré avec la mission** plutôt que ce qui l'exécute. Le
répertoire d'extraction du cycle 739 contient, à côté du conteneur de scénario de
3,4 Mo, un fichier de **32 octets** que personne n'avait ouvert.

```
00000000: 504c 4144 0000 0000 0000 0001 0000 0001  PLAD............
00000010: c4fd 2000 44bb 8000 44a8 2000 0000 0000  .. .D...D. .....
```

soit, en gros-boutiste : la signature `PLAD`, trois mots `0`, `1`, `1`, puis

```
(-2025.0, 1500.0, 1345.0, 0.0)
```

**C'est une position monde.** Et son ordonnée, `1500`, est exactement l'altitude
que portent les positions d'ordre d'étiquette 2 relevées au cycle 1122 — les
échantillons y sont tous à `1500`. Deux sources indépendantes du même paquet, la
même altitude.

## Ce que le binaire en dit : rien

Le premier réflexe est de chercher le consommateur par la signature. Il n'existe
pas :

- le mot `0x504C4144` **n'apparaît nulle part** dans la mémoire initialisée de
  l'image, à l'alignement 4 ;
- l'immédiat `lis rX,0x504c` n'apparaît dans **aucune** des 756 029 instructions
  décodées du corpus Xenon.

Le jeu ne compare donc pas cette signature : l'entrée est adressée **par son
index dans le FHM**, comme les lecteurs `*Bin` le sont par les chaînes de classe
du conteneur (cycle 1100). Le consommateur reste à trouver, et il faudra le
chercher côté chargeur de ressources, par l'index 2, pas par le nom.

## Le contrôle qui ne peut pas être fait ici

La vérification qui trancherait est évidente : **une autre mission a-t-elle un
`PLAD`, et à une autre position ?** Elle ne peut pas être faite dans cet espace
de travail :

| entrée PAC | enfants | `PLAD` | nature |
| ---: | ---: | :---: | --- |
| 9 | 26 | **oui** | la mission : scénario, `MDLP`, FHM |
| 119 | 23 | non | `NTXR`, textures |
| 165 | 11 | non | `NTXR`, textures |
| 199 | 1 | non | FHM |
| 210 | 6 | non | `BRDB`, `BMAP`, FHM |

Les quatre autres entrées extraites ne sont pas des missions, et les archives
retail ne sont pas dans cet espace — c'est la politique du dépôt, pas un oubli.
Le contrôle attend donc une extraction que ce cycle ne peut pas faire.

## Ce qui est établi, et ce qui ne l'est pas

**Établi.** Le paquet de la Mission 01 contient, à côté de son scénario, une
entrée de 32 octets dont la charge utile est une position monde unique, et cette
position partage son altitude avec les points de route du scénario.

**Non établi — et il faut le dire aussi net.** Que ce soit la position de départ
du joueur. Le nom le suggère, la forme le suggère, l'altitude le corrobore ; il
n'y a **ni consommateur ni second échantillon**. « `PLAD` est le départ du
joueur » reste une hypothèse bien formée, pas un fait, et rien n'en est câblé
dans le produit natif.

## Décisions de cycle

1. **Ne rien porter.** Une position unique sans consommateur ne place pas 230
   unités, et la brancher sur le joueur natif serait exactement la règle
   plausible que les cycles 1111 et 1113 ont appris à tuer.
2. **Ne pas renommer le fichier ni l'entrée** dans les manifestes : `PLAD` est ce
   que la signature dit, et « player start » est ce que je crois.

`ctest 24/24`, la porte JF reste verte.

## La prise suivante, nommée

Le consommateur de l'enfant **d'index 2** d'un FHM de mission. Il ne se cherche
pas par la signature — c'est acquis — mais par le chargeur de ressources qui
parcourt les enfants d'un FHM et les distribue par position.
