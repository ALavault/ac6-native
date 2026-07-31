# Cycle 369 — les planches de glyphes passent par le load shader 4, en pavé

## 1. Mesure

Paramètres de chargement journalisés pour chaque texture :

**Planches de glyphes (défaillantes)**

```
base=028B7000 320x180 load_shader=4 signed=false tiled=1 packed_mips=0 dim=1
base=03514000 256x256 load_shader=4 signed=false tiled=1 packed_mips=0 dim=1
```

**Échantillon d'autres textures**

```
base=03B51000 1280x720 load_shader=0 tiled=0
base=03CE1000 1280x720 load_shader=0 tiled=0
base=18E1B000  512x64  load_shader=4 tiled=1      <-- même combinaison
base=1AB60000 1280x720 load_shader=2 tiled=1
```

## 2. Lecture, prudente

Les deux planches utilisent `load_shader=4` avec `tiled=1`. **Mais au moins une
autre texture partage cette combinaison** (`18E1B000`, 512x64). Le couple
« load shader 4 + pavage » ne sépare donc **pas** à lui seul les défaillantes des
autres.

Réserve explicite sur la portée : l'échantillon ci-dessus couvre des textures
quelconques du runtime, **pas les cinq textures fonctionnelles de la passe
étudiée** (`028B2000`, `028D0000`, `028E9000`, `02953000`, `0294A000`). Leur
`load_shader` n'est pas encore connu, et c'est exactement la comparaison qui
manque.

Si elles utilisent aussi `load_shader=4`, le chargement est disculpé et le
défaut est ailleurs dans le téléversement. Si elles utilisent un autre shader de
chargement, `load_shader=4` devient le suspect direct.

## 3. Front suivant, à une ligne

Retirer l'échantillonnage du journal (`n <= 6 || n % 300`) pour ces cinq bases,
comme cela a été fait pour les deux planches au cycle 365 — le biais
d'échantillonnage a déjà masqué deux mesures dans cette enquête (cycles 350
et 365). Une exécution suffit ensuite.

## 4. État

Le défaut reste localisé au chemin de chargement/téléversement, avec des données
sources mesurées présentes (cycle 368). Dix-sept causes éliminées.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
