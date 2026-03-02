# Feature: Select Through / From Camera (D6322 Port)

## Vue d'ensemble
Cette fonctionnalité permet de sélectionner des objets/vertices/faces/arêtes à travers la géométrie 3D, en utilisant le tampon de profondeur (depth buffer) pour déterminer si la sélection doit traverser les objets ou s'arrêter à la première surface visible.

## Références
- **Patch Original (Kio)**: D6322 - https://archive.blender.org/developer/D6322
- **Contributeurs**: Kio (original), LudvikKoutny (mises à jour), Icas (tentatives)
- **Statut**: Jamais mergé dans main, à adapter pour Blender 5.0

## Modes de Sélection

### 1. Select Through (Défaut)
- Le clic de souris sélectionne à travers la géométrie
- Utilise le depth buffer pour ignorer les obstacles
- Utile pour les scènes denses

### 2. From Camera (Nouveau)
- Sélectionne uniquement ce qui est visible depuis la caméra
- Respecte l'occlusion (ne sélectionne pas à travers les objets)
- Plus intuitif pour la modélisation précise

### 3. Camera Frustum Select
- Sélection par rectangle dans le viewport
- Prend en compte la profondeur

## Fichiers à Modifier

### C++ Core
- `source/blender/editors/space_view3d/view3d_select.cc` - Logique principale
- `source/blender/editors/include/ED_view3d.h` - Déclarations
- `source/blender/editors/interface/view3d_select_tool.cc` - Outils

### UI/Properties
- Ajout toggle dans l'overlay de la viewport
- Raccourcis clavier (Tab pour basculer ?)
- Property: `space_view3d.select_through`

### GPU/Depth
- `source/blender/draw/engines/workbench/workbench_engine.cc` - Accès depth buffer
- `source/blender/gpu/shaders/` - Shaders de lecture depth

## Implémentation Technique

### 1. Depth Buffer Access
```cpp
// Récupérer le depth buffer de la viewport
GPUTexture *depth_texture = DRW_context_depth_texture_get();
float depth = GPU_texture_sample_depth(depth_texture, x, y);
```

### 2. Test d'Occlusion
```cpp
bool is_occluded = (current_depth > depth_buffer_value + epsilon);
if (!select_through && is_occluded) {
    return false; // Ne pas sélectionner
}
```

### 3. Bascule UI
- Overlay: Icône "cube transparent" vs "cube plein"
- Raccourci: `Alt + S` (à définir)
- Menu: View3D > Select > Select Through

## Conflits Potentiels
1. **Sculpt Mode**: La sélection through peut être gênante
2. **X-Ray Mode**: Interaction avec le mode X-Ray existant
3. **Performance**: Lecture depth buffer peut impacter FPS
4. **Edit Mode**: Gestion des vertices cachés (hide)

## Plan d'Adaptation

### Phase 1: Analyse (FAIT)
- [x] Récupérer patch D6322
- [x] Identifier fichiers modifiés
- [x] Comprendre logique depth test

### Phase 2: Porting (À faire)
- [ ] Extraire code du patch
- [ ] Adapter API Blender 5.0
- [ ] Gérer conflits avec code actuel

### Phase 3: UI/UX
- [ ] Créer toggle overlay
- [ ] Ajouter propriété scene
- [ ] Raccourcis clavier

### Phase 4: Tests
- [ ] Scènes denses (1M+ faces)
- [ ] Edit Mode
- [ ] Sculpt Mode
- [ ] Grease Pencil
- [ ] Hair/Nodes

## Notes Techniques

### Problèmes du Patch Original
1. **Performance**: Lecture depth buffer lente sur vieux GPU
2. **Précision**: Problèmes avec orthographic view
3. **Conflits**: Sculpt mode et X-Ray

### Solutions Modernes (Blender 5.0)
1. **GPU Shader**: Utiliser compute shader pour depth test
2. **Hybrid**: Depth test uniquement si `select_through` activé
3. **Context**: Désactiver automatiquement en Sculpt Mode

## Code Structure (Proposition)

```cpp
// view3d_select.cc
namespace {

enum class SelectMode {
  THROUGH,    // Sélectionne à travers tout
  FROM_CAMERA // Respecte l'occlusion (défaut)
};

bool select_is_occluded(const ARegion *region, const float2 &pos_screen)
{
  // Lecture depth buffer
  // Retourne true si occlus
}

} // namespace

// Dans ED_view3d_select_pick
if (select_mode == SelectMode::FROM_CAMERA) {
  if (select_is_occluded(region, pos)) {
    return nullptr;
  }
}
```

## Prochaines Étapes
1. **Extraire le patch D6322** (fichier .patch ou .diff)
2. **Analyser les diffs** (fichiers + lignes)
3. **Créer prototype** sur branche
4. **Tester** et ajuster

## Ressources
- [T62114](https://projects.blender.org/blender/blender/issues/62114) - Wireframe selection issue
- [T54190](https://projects.blender.org/blender/blender/issues/54190) - Occlusion query
- Commit: `a1164eb3ddb` - View3D: support both orbit select & depth
