# Cycle 354 — le format de sommet est pris en charge ; reste la valeur

## 1. Vérification

Le cycle 353 a établi que la couche d'interface invisible est
« texture × couleur de sommet », la couleur venant d'un
`vfetch_full ... DataFormat=FMT_8_8_8_8, Offset=12, Stride=13`.

Le décodage de ce format dans le traducteur SPIR-V existe :
`spirv_translator_fetch.cpp:181`, `case xenos::VertexFormat::k_8_8_8_8:`,
parmi les treize formats de sommet traités.

**« Format de sommet non pris en charge » est donc écarté.** Comme pour
`k_DXT4_5` au cycle 350 : prise en charge ne vaut pas correction, mais rien
n'appuie l'absence.

## 2. Où en est la cause

Ce qui est **établi** (cycle 353) : la sortie de la passe défaillante est le
**produit** d'une texture et d'un attribut de sommet empaqueté ; la passe
témoin, qui s'affiche, ne multiplie par aucune couleur de sommet.

Ce qui **reste à mesurer** : la **valeur** de cet attribut. Un zéro annule le
produit et rend la couche invisible sans rien casser d'autre — ce qui est
cohérent avec les six causes déjà éliminées.

Ce qui est **écarté** à ce stade : cible de rendu et mode EDRAM (348),
géométrie hors viewport (349), test alpha (349), texture absente ou de taille
nulle (350), texture jamais chargée et mémoire non peuplée (352, témoin
vérifié), format de sommet non pris en charge (ici).

## 3. Le test décisif, non exécuté

Forcer `oC0 = r0` pour le seul shader `8F1C48BA92C8E43E`, c'est-à-dire ignorer
`r1` :

- si l'interface **apparaît**, la couleur de sommet arrive à zéro et la cause
  est le chemin `vfetch FMT_8_8_8_8` (buffer, foulée de 13 mots, décalage 12) ;
- si elle **reste absente**, la dépendance identifiée au cycle 353 n'est pas la
  cause et il faut revenir aux facteurs.

Il n'a pas été exécuté : il demande une intervention sur la traduction du
shader, que le budget de contexte de cette session ne couvre plus.

## 4. Honnêteté sur l'état global

Ce cycle n'améliore aucun compteur et ne débloque pas P1.3. Il retire une
huitième cause candidate et laisse **une seule question mesurable** :
la valeur de l'attribut de sommet.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
