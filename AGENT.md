Fork Blender Pagecran
=====================

Ce dépôt est le fork Pagecran de Blender, basé sur Blender amont `v5.1.1` et maintenu sur la branche
`Pagec5.1.1`. La version Pagecran courante est `5.1.1.8` quatrième composant = build.

![Blender screenshot](https://code.blender.org/wp-content/uploads/2018/12/springrg.jpg "Blender screenshot")

Changements 
-----------

Le fork ajoute ces changements :

- Déploiement interne Pagecran de Blender 5.x, avec raccourci bureau `blender 5.1` pour la compatibilité
  addons et Royal Render.
- Visibilité objet connectée à la rendabilité, avec gestion de visibilité par render layer et restauration du
  comportement où un objet caché dans le viewport est aussi caché au rendu.
- Workflow USD Pagecran : export USD adapté, conversion MaterialX/OpenPBR, conservation des matériaux de même
  nom à l'import USD, dossier `mtl` et conventions de nommage SG/shader.
- Overrides sur bibliothèques liées : override de matériaux de library, override de bibliothèques liées imbriquées
  et push back vers la bibliothèque d'origine depuis l'Outliner.
- Shader Editor : l'onglet N-panel `Color` expose le widget natif `Palette`; Ctrl+clic sur une couleur ouvre le
  sélecteur natif Blender pour ce swatch.
- Cycles : denoise AOV/light passes, y compris les passes Light Group `Combined_<Group>`, en conservant les
  métadonnées Light Group et le comportement des light passes standards.
- Select Through complet en Object Mode et Edit Mode : sélection de la face avant seule ou de tout ce qui est dans
  la sélection, même derrière la géométrie, sans activer X-Ray. Outils Circle et Lasso re-impelmentés.
- Overrides d'animation dans les shots : l'état mute des tracks/canaux NLA est spécifique à chaque View Layer.
- Panneaux flottants modeless : panneaux qui restent ouverts hors clic extérieur et ne sont pas fixés à la zone
  gauche du viewport, utilisés dans le menu Pagecran Blender pour Material Utilities et Cleaning Tools.
- Navigation et support de rig pour camera aim target.
- `Separate by Material` conserve les slots de matériaux liés à l'objet lors de la séparation.
- Support du numéro de build Pagecran via `BLENDER_VERSION_BUILD` pour le versioning Windows.

Compilation et packaging Windows
--------------------------------

Depuis la racine du dépôt, le build de release standard se lance avec :

```bat
.\make.bat release
```

Après un build release configuré, le MSI vérifié est généré depuis la configuration CPack du répertoire de build :

```bat
cpack -C Release -G WIX --config "D:\build_windows_Release_x64_vc17_Release\CPackConfig.cmake"
```

Chemin attendu après génération MSI pour le build courant :

```text
D:\build_windows_Release_x64_vc17_Release\blender-5.1.1.<build>-windows-x64.msi
```

La configuration WIX/CPack se trouve dans `build_files/cmake/packaging.cmake`. Le nom du package inclut le
quatrième composant de version Pagecran.

Procédure du numéro de build Pagecran
-------------------------------------

Lors de l'incrément du numéro de build Pagecran :

1. Mettre à jour `BLENDER_VERSION_BUILD` dans `source/blender/blenkernel/BKE_blender_version.h` avec l'entier suivant.
2. Utiliser cet entier comme quatrième composant de version. Par exemple, Blender `5.1.1` avec
   `BLENDER_VERSION_BUILD 8` est documenté comme `5.1.1.8`.
3. Commiter uniquement ce changement de numéro de build séparément, avec le sujet `Update BKE_blender_version.h`
   et le corps de commit `build <N>`, en remplaçant `<N>` par le nouveau numéro.

Push vers GitHub sans abonnement LFS
-------------------------------------

Ce fork publie ses branches sur `github.com/Pagecran/blender`, qui est un fork
public. Les forks publics GitHub n'autorisent pas l'upload de blobs LFS au-delà
du quota gratuit, et le fork Pagecran n'a pas d'abonnement LFS. Les blobs
d'assets Blender (matcaps, hdri, presets, etc.) restent hébergés côté
`projects.blender.org` (upstream Blender) ; les fichiers LFS présents dans les
commits sont publiés sur GitHub sous forme de pointeurs, et toute personne
clonant le fork récupère les blobs depuis l'URL référencée par chaque pointeur.


### Configuration permanente appliquée

Dans `.git/config` local de ce clone :

```ini
[remote "origin"]
        url = https://github.com/Pagecran/blender.git

[lfs]
        skipPush = true
```

`lfs.skipPush = true` dit à `git-lfs` d'envoyer les commits et leurs pointeurs
vers `origin` sans tenter d'uploader les blobs eux-mêmes. Les blobs référencés
continuent d'être servis depuis `projects.blender.org` pour les fetches via
`git lfs fetch` ou `git lfs pull`.

### Pousser une nouvelle branche de release depuis upstream

Exemple appliqué pour `Pagec5.2.0` basé sur `upstream/blender-v5.2-release` :

```bash
# Récupération de la ref upstream 5.2 (si pas déjà présente)
git fetch upstream

# Création de la branche de release Pagecran au même SHA que la release upstream
git branch Pagec5.2.0 upstream/blender-v5.2-release

# Push vers le fork public, sans déclencher l'upload LFS
git push origin Pagec5.2.0
```

Build d'une nouvelle branche de release Pagecran
------------------------------------------------

Aucune procédure manuelle de récupération des libs n'est nécessaire : `.\make.bat
update` fait tout. Le script détecte l'architecture (x64 → `lib/windows_x64`,
ARM64 → `lib/windows_arm64`), configure le submodule en mode `checkout`, lance
`git submodule update --init --progress <lib>` pour ramener le bon SHA depuis
`projects.blender.org/blender/lib-*.git`, puis exécute `make_update.py` pour
les opérations annexes. Les submodules Linux/macOS ne sont pas touchés.

Procédure type après création de la branche depuis upstream :

```bash
git checkout Pagec5.2.0
.\make.bat update
```

Puis build classique :

```bat
.\make.bat release
```
