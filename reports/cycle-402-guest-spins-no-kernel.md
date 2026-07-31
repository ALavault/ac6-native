# Cycle 402 — sur le dialogue, l'invité présente et n'appelle plus le noyau

## 1. Mesure

Journal en `--log_level=debug`, fenêtre prise à partir de l'instant où l'écran
est atteint, pendant ~14 s avec trois pressions au milieu. 760 lignes :

| étiquette | lignes | contenu |
|---|---|---|
| `[gpu]` | 619 | `PRESENT: swap_tex…`, espacées de 16-17 ms |
| `[krnl]` | **6** | uniquement `[xam-input]` |
| `[apu]` | 15 | audio |
| `[warning]` | 10 | — |

## 2. Lecture

L'invité **présente régulièrement à 60 Hz** et **reçoit les entrées**, mais
n'émet **aucun appel noyau** hors de la lecture des manettes. Pas d'ouverture de
fichier, pas d'énumération de contenu, pas d'attente d'objet, rien.

Cela **affaiblit l'hypothèse du cycle 401** — « attente d'une opération
asynchrone qui ne se termine jamais ». Une attente de ce genre laisse
normalement une trace d'appel avant de bloquer, et ici la fenêtre observée n'en
contient aucune. Deux formes restent compatibles avec la mesure :

1. la machine à états de l'interface tourne en boucle sans avancer, entièrement
   en code invité, sans jamais solliciter le noyau ;
2. un fil de travail est déjà bloqué depuis avant la fenêtre observée, pendant
   que le fil de rendu continue.

La mesure ne les départage pas. La distinction se tranche en observant l'état
des fils, pas le journal.

## 3. Ce qui est solidement acquis à ce stade

| fait | cycle | méthode |
|---|---|---|
| le panneau déborde à droite (264 → ≥1279 sur 1280) | 397 | mesure de pixels, sans surimpression |
| l'écran ne réagit à aucune touche | 400 | balayage avec témoin de touches non affectées |
| l'entrée parvient à l'invité, correctement encodée | 401 | sonde au niveau `X_INPUT_STATE` |
| l'invité présente à 60 Hz sans appel noyau | 402 | journal debug |
| le stockage de contenu n'y change rien | 397 | comparaison avec/sans arborescence |
| la transformation NDC est correcte | 399 | lecture de `util/draw.cpp` |

## 4. Prochain pas

Observer l'état des fils invités pendant que le dialogue est affiché — lequel
tourne, lequel dort, et sur quoi. C'est la mesure qui départage les deux formes
restantes, et aucune quantité de journal ou de pixels ne le fera.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
