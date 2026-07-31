# Cycle 410 — sonde d'aiguillage écrite et câblée, mais **non active**

## 1. Ce qui est en place

- `ac6recomp_config.toml:7004` — `0x8234D50C = { name = "rex_sub_8234D50C" }`
- `src/ac6_backend_fixes/ac6_ui_input_dispatch_probe.cpp` — surcharge
  `PPC_FUNC_IMPL(rex_sub_8234D50C)` relevant le pointeur d'objet entrant et la
  branche `r11` réellement prise, comptés et résumés toutes les 600 trames,
  derrière `ac6_log_ui_dispatch` (faux par défaut)
- `CMakeLists.txt:44` — fichier ajouté à la cible
- compilation réussie ; le cvar est présent dans le binaire (4 occurrences)

## 2. Pourquoi elle ne se déclenchera pas

Le code recompilé **n'a pas été régénéré**. Dans
`recomp-eval/ac6/output/ppc_recomp.58.cpp`, `rex_sub_8234D50C` apparaît **zéro
fois** : l'invité appelle toujours `sub_8234D50C`, et la surcharge n'est jamais
atteinte.

L'édition de liens a pourtant réussi, ce qui est trompeur — `PPC_EXTERN_FUNC`
déclare un symbole faible, si bien qu'une surcharge non reliée se compile et se
lie sans erreur. **Rien dans le processus de compilation ne signale la panne.**

Le renommage d'une fonction dans la configuration exige `rexglue codegen`, qui
recompile le XEX entier ; cette étape n'a pas été menée à son terme ici.

## 3. Pourquoi aucun chiffre n'est publié

Exécuter maintenant produirait un journal sans aucune ligne `[ac6-ui-dispatch]`,
qu'il serait facile de lire comme « l'aiguilleur n'est jamais appelé » — une
conclusion fausse et du même genre que celles des cycles 394 à 400 : un
instrument muet pris pour une mesure.

La sonde ne vaut qu'après régénération. C'est la première étape de la reprise,
et elle se vérifie en une commande : `rex_sub_8234D50C` doit apparaître dans le
code généré avant toute exécution.

## 4. Piège d'outillage à retenir

Une surcharge `PPC_FUNC_IMPL` qui compile et se lie **ne prouve pas** qu'elle
est reliée à l'invité. La seule vérification valable est de chercher le nom
renommé dans le code généré. À faire systématiquement avant d'interpréter le
silence d'une sonde.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
