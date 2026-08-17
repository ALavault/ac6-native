# Cycle 1624 — shared memory Vulkan neutral

## Résultat

Le chemin produit alimente maintenant les quatre descripteurs shared-memory de
l'ABI ReXGlue atteinte. Les buffers sont créés et initialisés intégralement à
zéro avant commit ; seul le segment 2 reçoit les 108 octets guest qualifiés de
`[0x127CA03C,0x127CA0A8)`, couvrant les plages fetch des deux rectangles. Le
sélecteur de segment et le masque `0x07FFFFFF` proviennent de ReXGlue/Xenos
générique ; les adresses et octets proviennent exclusivement du replay démo PAL.

La publication reste transactionnelle : pool, set et quatre buffers ne
remplacent l'état antérieur qu'après allocation, copie bornée et mise à jour des
descripteurs réussies. Toute erreur détruit l'état provisoire. Aucun buffer de
constantes, draw, resolve produit ou readback n'est encore revendiqué.

Deux imports et exécutions neutral/Vulkan frais jusqu'au tick 253 donnent quatre
descripteurs, quatre modules et deux pipelines, avec sorties byte-identiques :
RTPLY `c5357c6d…c5794`, rapport `04116bf6…de35`. START n'a pas été exécuté.

## Qualification

- `xenia-generic` : découpage en quatre segments et masque d'adresse ;
- `demo-observed` : plages fetch dans le snapshot PM4 ;
- `demo-qualified` : copie guest bornée et quatre descripteurs Vulkan ;
- `unknown` : cinq buffers de constantes, contenu EDRAM après draw et pixels.

Aucune preuve retail n'est fusionnée. Xenia/ReXGlue, Ghidra, C++ généré et
microcodes ne sont pas modifiés ; aucun actif propriétaire n'est suivi.

Reçu : `analysis/demo/ac6-demo-runtime-rexglue-neutral-v1.json`.

Le helper Vulkan est extrait dans `vulkan_shared_memory.cpp`; `main.cpp` revient
à la limite scellée de 1200 lignes sans exemption. Builds codegen OFF/ON et
CTest complets 18/18 et 17/17 sous Xvfb/audio dummy : PASS.

## Prochain checkpoint

Construire les cinq buffers de constantes depuis le snapshot exact des
registres. Binder et soumettre uniquement le rectangle normal ; trapper avant
commande GPU si un champ ou une plage manque.
