# Cycle 1580 — le runtime natif possède le curseur retail

`RetailSession::QualifiedRuntime` est désormais un mode produit accepté par
les deux entrées de session. À chaque tick `Gameplay`, le runtime vérifie les
trois gardes explicites (contexte vivant, curseur courant, producteur/joueur
publié), avance exactement un pas de `MissionScriptRunner`, puis laisse
`MissionExecution` appliquer la physique et l'HSM. `ExternalProbe` reste
diagnostic et `DiagnosticFixedTick` reste payload-only.

Le chemin `play` et le replay store-backed sélectionnent `QualifiedRuntime`;
leur rapport indique `script_drive=qualified_runtime` et
`script_advance_each_tick=true`. Pause, reprise et les états terminaux bloquent
le scheduler via l'état de scénario existant.

Validation ciblée :

```text
cmake --build reconstruction/ace-combat-6/build-core -j16     PASS
ctest -R 'retail-session|retail-playable'                     3/3 PASS
python3 -m unittest discover -s tools/tests -q                181/181 PASS
ruff check tools/verify_mission01_native_extraction.py        PASS
```

La géométrie interactive reste à raccorder au renderer Vulkan persistant;
`NativeRenderTarget` est encore la voie diagnostic du `play` dans ce
checkpoint. JV n'est donc pas promu par cette modification seule.
