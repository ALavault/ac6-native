# Mission 01 — frontière UpHud et soumission texte

## UpHud

Dans le corpus exécuté, le bloc situé à `0x8226DF00` est inline dans
`sub_8226D1C8`; ce n'est pas le wrapper secondaire `rex_sub_8226DE30`. Le hook
read-only attaché au début de `sub_8226D1C8` a produit 1 066 records dans le
run `cycle-1026-campaign-selector`.

Aux frames C5/C6 :

```text
marker       0x8226DF00
call         0x8226DF1C
update mask  0x0001F7FF
bit 0x80     1
manager      0xB0E80000
+0x29C       0xB0E8CE20
vtable       0x82055544
slot +0x38   0x8223B398
```

UpHud est donc exécuté au niveau de sa frontière et la cible virtuelle est
non-nulle. La conclusion F (« UpHud non exécuté ») est rejetée. Le probe ne
compte pas à lui seul les éléments construits, les submits texte ou les pixels;
ces colonnes restent explicitement `not_proven` dans
`analysis/hud/up_hud_and_text_submission.jsonl`.

## Éléments déjà séparés

Le rapport préexistant `reports/hud_partial_render_report.md` et les JSONL
associés apportent la séparation suivante, sans confondre vertices et pixels :

* panneaux/boutons : construits, soumis, draw hôte et pixels visibles;
* lots glyphes courts et longs : buffers/metrics/quads/submit observés, mais
  pixels absents; le bind atlas exact reste inconnu;
* `M70000_222` : lookup observé à 0; le défaut peut être une clé absente ou une
  suppression appelante, et n'est pas promu atlas sans join supplémentaire;
* `LOAD_W_003` : lookup 69 et mêmes stages texte;
* `0x82382480` : override PAL `atoi` déjà lié et testé dans le cycle dédié,
  mais aucun appel HUD courant n'est encore joint aux quatre contrôles
  `0`, `123`, `-45`, `70000`.

La chaîne HUD n'est donc pas « jamais soumise » globalement. La classification
G est rejetée pour les glyphes déjà observés soumis; la frontière atlas/draw
exact et la cause des valeurs manquantes restent ouvertes par élément.

## Statuts

| gate | statut |
| --- | --- |
| UpHud atteint | `proven` en lane bridge |
| bit update `0x80` autorise la branche | `proven` aux frames gameplay |
| target virtuel +0x38 non nul | `strongly_supported` |
| tous les textes attendus construits | `open` |
| tous les textes attendus soumis | `open` |
| atlas/glyphes corrects | `open` |
| chiffres absents attribués à atoi | `rejected` sans appel HUD joint |

Le prochain probe doit accrocher les frontières texte existantes pendant une
frame C5/C6 et associer chaque clé à son draw et à une région pixel. Il ne doit
pas ajouter de texte de secours ni modifier le renderer.
