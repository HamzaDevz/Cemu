# Compatibilité Odin 3 : Cemu (Wii U)

Notre propre liste de compatibilité, tenue d'après nos tests sur l'AYN Odin 3 (Snapdragon 8 Elite, Adreno 830) avec notre build de la branche `odin3`. Chaque test met à jour la ligne du jeu : statut, mesure, date et commit testé.

Autres émulateurs : [Eden (Switch)](https://github.com/HamzaDevz/eden/blob/odin3/ODIN3_COMPAT.md) · [Azahar (3DS)](https://github.com/HamzaDevz/azahar/blob/odin3/ODIN3_COMPAT.md) · [Dolphin (GC/Wii)](https://github.com/HamzaDevz/dolphin/blob/odin3/ODIN3_COMPAT.md) · [Cemu (Wii U)](https://github.com/HamzaDevz/Cemu/blob/odin3/ODIN3_COMPAT.md) · [Vita3K (Vita)](https://github.com/HamzaDevz/Vita3K/blob/odin3/ODIN3_COMPAT.md) · [aPS3e (PS3)](https://github.com/HamzaDevz/aps3e/blob/odin3/ODIN3_COMPAT.md) · [Bachata S4 (PS4)](https://github.com/HamzaDevz/bachata-s4/blob/odin3/ODIN3_COMPAT.md) · [xemu (Xbox)](https://github.com/HamzaDevz/xemu/blob/odin3/ODIN3_COMPAT.md)

## Échelle

| Statut | Sens |
|---|---|
| 🟢 Parfait | vu en partie, plein régime, aucun défaut vu |
| 🔵 Jouable | vu en partie, défauts mineurs |
| 🟡 Problèmes | vu en partie, défauts gênants (fps, graphismes, son) |
| 🟠 Menus/intro | démarre, pas encore vu en partie |
| 🔴 Ne marche pas | crash, écran noir ou blocage |
| ⚪ Non testé | présent sur la console, jamais lancé |

## Comment on teste

Paquet `info.cemu.cemu.odin3`, installé à côté de l'officiel `info.cemu.cemu` ; ES-DE le lance par défaut (« Cemu Odin 3 »). Banc adb dans `tools/odin3/` : fps relevés chaque seconde par SurfaceFlinger (`fps.sh`, `fps.py`), manettes DS4 virtuelles (`vpads.sh`), déplacements scriptés (`walk.sh`). Caches de shaders et de pipelines communautaires (emiyl) installés avant de mesurer. Config : `AsyncCompile`, `GX2DrawdoneSync` et `vkAccurateBarriers` actifs.

## Jeux

| Jeu | Statut | Vu jusqu'à | Mesure | Notes | Testé le | Build |
|---|---|---|---|---|---|---|
| Super Mario 3D World (EUR) `0005000010145D00` | 🔵 Jouable | niveau 1-1 à 1, 2 et 3 joueurs | 60,0 fps, 0 % d'images hors cadence, pire image 16,7 ms (1 joueur) ; 60,1 fps à 3 joueurs | 4 joueurs impossibles quand le joueur 1 est en Pro Controller (3 au plus) ; sauvegardes à vérifier (3 fichiers « Nouveau » à chaque lancement) ; sortie TV pas testée ; correctifs 3f673d9c (manettes) et 4aa6d486 (60 Hz, plus de judder sur l'écran 120 Hz) ; démarrage ~30 s avec les caches | 2026-10-17 | 4aa6d486 |
| Zelda: Twilight Princess HD (EUR) `000500001019E600` | 🟠 Menus/intro | intro (85 s) | pas mesuré ; aucun shader recompilé grâce aux caches | | 2026-10-17 | 50dfa688 |
| Zelda: Breath of the Wild (USA v208 + DLC) `00050000101C9400` | 🟠 Menus/intro | écran titre | 30 fps au titre (plafond natif) ; premier lancement 3 min 17 s, puis 16 s | crashait au chargement du cache avant af6de619 ; ROM retirée de l'Odin le 17/10 (`Emulations/_retires/wiiu`), sauvegardes gardées | 2026-10-17 | af6de619 |
