# Cycle 903 — validation texture UV→PPM→rasterizer

La chaîne de vérification est reproductible sans archive runtime :

1. `scripts/probe_ntxr_bc.py` décode une tranche NTXR BC3 tiled en PPM ;
2. `textures.tsv` référence le PPM par chemin, taille et FNV-64 ;
3. le loader le vérifie et le renderer le sample via les UV NDXR.

Le smoke caméra bridge produit une frame et une profondeur avec cette texture.
Le hash couleur diffère du manifeste sans image (`c48ede12…` → `047f3bcd…`),
ce qui ferme le trajet texel observable. Cette preuve reste une lane de
diagnostic : le MATE/GIDX exact, les mips et les permutations shader retail ne
sont pas encore joints.

CTest normal et ASan/UBSan : 3/3 chacun.
