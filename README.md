<!--
README du fork Pagecran.
-->

Fork Blender Pagecran
=====================

Blender est une suite de création 3D libre et open source.
Elle couvre toute la chaîne 3D : modélisation, rigging, animation, simulation, rendu, compositing,
motion tracking et montage vidéo.

Ce dépôt est le fork Pagecran de Blender, basé sur Blender amont `v5.1.1` et maintenu sur la branche
`Pagec5.1.1`. La version Pagecran courante est `5.1.1.8` : Blender `5.1.1` avec le numéro de build
Pagecran `8` comme quatrième composant de version.

![Blender screenshot](https://code.blender.org/wp-content/uploads/2018/12/springrg.jpg "Blender screenshot")

Changements Pagecran
--------------------

Le fork ajoute notamment ces changements au-dessus de Blender `v5.1.1` :

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
  la sélection, même derrière la géométrie, sans activer X-Ray. Les outils Circle et Lasso sont compatibles.
- Overrides d'animation dans les shots : l'état mute des tracks/canaux NLA est spécifique à chaque View Layer.
- Panneaux flottants modeless : panneaux qui restent ouverts hors clic extérieur et ne sont pas fixés à la zone
  gauche du viewport, utilisés dans le menu Pagecran Blender pour Material Utilities et Cleaning Tools.
- Navigation et support de rig pour camera aim target.
- `Separate by Material` conserve les slots de matériaux liés à l'objet lors de la séparation.
- Support du numéro de build Pagecran via `BLENDER_VERSION_BUILD` pour le versioning Windows.

Les captures d'écran de certaines fonctionnalités sont disponibles dans les annonces Teams PagecranTeam/General
de Pascal ; elles ne sont pas intégrées ici car les URLs Teams sont protégées par authentification.

Compilation et packaging Windows
--------------------------------

Depuis la racine du dépôt, le build de release standard se lance avec :

```bat
.\make.bat release
```

Pour nettoyer puis reconstruire une release, utiliser la cible `clean` avec une cible de build explicite :

```bat
.\make.bat clean release
```

`clean` est documenté par l'aide de `make.bat` comme nécessitant une cible ; avec le générateur MSBuild, ce chemin
appelle la cible MSBuild `clean` avant de relancer le build.

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

Conséquence pratique : un `git push origin <branche>` standard échoue avec :

```
batch response: @pascalandr can not upload new objects to public fork Pagecran/blender
error: failed to push some refs to 'https://github.com/Pagecran/blender.git'
```

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

### Pousser un commit qui modifie un fichier LFS (cas ponctuel)

Si jamais un commit Pagecran modifie un fichier tracké par LFS (matcap ajouté,
hdri remplacé, preset ajusté) et que le push du blob est nécessaire sur le fork
public :

```bash
# Push en one-shot, en neutralisant la config permanente
git -c lfs.skipPush=false push origin <branche>
# Ou, sur PowerShell en équivalent Windows :
#   git config --local --unset lfs.skipPush
#   git push origin <branche>
#   git config --local lfs.skipPush true
```

### Cas où ça ne marche toujours pas

Si malgré `lfs.skipPush = true` un push échoue encore avec le même message :
vérifier qu'il n'existe pas d'autre remote LFS prioritaire via :

```bash
git lfs env | grep Endpoint
```

Chez ce fork, trois endpoints sont attendus :

```
Endpoint        = https://github.com/Pagecran/blender.git/info/lfs (auth=none)
Endpoint (upstream)        = https://github.com/blender/blender.git/info/lfs (auth=none)
Endpoint (lfs-fallback)    = https://projects.blender.org/blender/blender.git/info/lfs (auth=none)
```

Le remote `upstream` est en lecture seule (mirror Blender officiel) et le remote
`lfs-fallback` a `pushurl = no_push` (politique Blender : on ne pousse pas
depuis un clone non autorisé). Le seul remote sur lequel un push doit réussir
est `origin`, qui après la configuration ci-dessus ne tente plus d'upload LFS.

Pages du projet
---------------

- [Site principal](http://www.blender.org)
- [Manuel de référence](https://docs.blender.org/manual/en/latest/index.html)
- [Communauté utilisateur](https://www.blender.org/community/)

Développement
-------------

- [Instructions de compilation](https://developer.blender.org/docs/handbook/building_blender/)
- [Code review et suivi des bugs](https://projects.blender.org)
- [Forum développeur](https://devtalk.blender.org)
- [Documentation développeur](https://developer.blender.org/docs/)


Licence
-------

Blender dans son ensemble est distribué sous licence GNU General Public License, version 3.
Certains fichiers peuvent utiliser une licence différente mais compatible.

Voir [blender.org/about/license](https://www.blender.org/about/license) pour les détails.
