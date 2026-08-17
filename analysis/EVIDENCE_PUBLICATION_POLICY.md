# Politique de publication des preuves AC6

## Canon

Les preuves canoniques sont des fichiers texte versionnés séparément :

```text
analysis/**/*.csv
analysis/**/*.json
reports/**/*.md
tools/**/*.py
```

Les archives `.zip`, `.tar` et équivalentes ne sont pas des sources de preuve
canoniques et ne doivent pas être commitées sur les branches de recherche.

## Incident du 17 août 2026

Trois archives publiées sur `infos` étaient des préfixes ZIP tronqués. Les ZIP
locaux disponibles étaient valides ; la corruption s'est produite dans la
chaîne de publication binaire. Git a correctement stocké les octets incomplets
qu'on lui avait fournis, ce qui est techniquement irréprochable et humainement
inutile.

La réparation est :

1. suppression des trois blobs ZIP du tree courant ;
2. import des membres intacts déjà récupérés en fichiers clairs ;
3. restauration exacte de `ranking_ui_callback_contract.csv` ;
4. régénération explicitement marquée de `mission_result_layout.csv` ;
5. publication d'un résumé reproductible des conditions de démarrage d'Act ;
6. publication future des preuves uniquement en clair.

## Gates de publication

Avant commit :

- chaque fichier a un chemin stable ;
- les extracteurs passent `py_compile` ;
- les fonctions citées sont vérifiées contre `.pdata` lorsque possible ;
- les fichiers régénérés déclarent leur provenance ;
- la présence de classes compiled-only n'est jamais transformée en reachability.

Aucune conclusion ne doit dépendre exclusivement d'un agrégat binaire.
