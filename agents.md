## Library Override Push-Back

  This feature integrates into Blender's existing library override workflow through three key components:

  **1. Python Operator (scripts/startup/bl_operators/object.py)**
     - Added OBJECT_OT_library_override_pushback operator (lines 993-1254)
     - Supports all ID types through context.selected_ids and context.id (Outliner)
     - Fallback support for context.selected_objects (3D view)
     - Collects override properties, skipping unsafe pointer-based overrides
     - Handles type name resolution with multiple variants for Blender's inconsistent ID type naming
     - Converts mathutils types to tuples for proper serialization
     - Creates timestamped backups before modification
     - Implements error handling with automatic backup restoration on failure
     - Optional override deletion after successful push
     - Calls bpy.data.libraries.modify_external() C++ API for the actual file modification

  **2. Outliner Menu Integration (scripts/startup/bl_ui/space_outliner.py)**
     - Added "Push Back" menu entry to OUTLINER_MT_liboverride menu (lines 396-398)
     - Positioned between "Clear" and "Troubleshoot" for intuitive workflow
     - Uses outliner.liboverride_operation with type OVERRIDE_LIBRARY_PUSH_BACK
     - Respects existing menu conventions with selection_set = 'SELECTED'

  **3. C++ Integration (source/blender/editors/space_outliner/outliner_tools.cc)**
     - Added OUTLINER_LIBOVERRIDE_OP_PUSH_BACK to enum (line 1811)
     - Added enum property definition with documentation (lines 1811-1815)
     - Implemented case handler in liboverride operation executor (lines 2007-2014)
     - Calls Python operator through WM_operator_name_call with proper undo depth management
     - Triggers appropriate notifiers for UI updates

  **Key Features:**
  - Works with all ID types (Objects, Materials, Collections, Armatures, etc.)
  - Multi-selection support via Outliner and 3D viewport
  - Automatic backup creation with optional retention
  - Safe property filtering (skips pointer and collection properties)
  - Robust error handling with detailed user feedback
  - Automatic library reload after modification
  - Full undo/redo support

  **Use Case:**
  Artists can now modify library-linked assets through overrides and push those changes back to the source library for reuse across
  multiple blend files, streamlining asset pipeline workflows for studios and collaborative projects.)

## Known Gaps (single-level libraries)
- **Materials / Node Trees**
  - Many RNA paths are still flagged non-override → attributes should be green( overided) but they look juste normal and have no persistence (not saved in the file) = broken overide
  - modified `NodeTree` revert to previous state when pushed backed ...
- **Lights**
  - Remain flagged `LIBOVERRIDE_FLAG_SYSTEM_DEFINED`, keeping properties locked.
- `NodeTree` created via "Use Nodes" stays local; nothing is pushed back.
  _Next steps_: expose key material/light properties for overrides, create overrides for the companion node trees, clear the system flag when we create the override.

## Other Known Gaps – Nested Library Overrides
- **Symptom**: `id_override_library_create_hierarchy_pre_process()` bails out when an anchor points into a nested library → we get "ghost" overrides (listed in the Outliner but no local datablock, UI stays locked, duplicates stay visible).
For any item (material, mesh, etc.), if you make a library override, the object is just duplicated. So the coyp is tagged as overide but obviously this is just broken overide.
- **Proposed plan**:
  1. When walking up the hierarchy, push every nested parent through `OutlinerLibOverrideData::id_root_add` instead of returning early.
  2. In `id_override_library_create_hierarchy_process()` create these parents first so the children (materials, meshes, etc.) get a valid local anchor.
  3. Rebuild (`./make.bat release`) and validate:
     - nested material → `override_library` populated, editor unlocked;
     - nested object → no duplicate remains;
     - non-nested material → regression check.

## Plan de developpement

  ———

  ### Phase 0 – Préparation

  - Jeux de tests
      - Préparer deux fichiers .blend de référence :
          1. tests/liboverride_single_level.blend — un objet, un matériau (avec node tree actif) et une light provenant d’une lib non
             imbriquée.
          2. tests/liboverride_nested.blend — même typologie mais la collection provient d’une lib imbriquée.
      - Écrire un script de validation (Python) qui :
          - force un override sur chaque ID/chemin,
          - logge ID.override_library et le contenu de override_library.properties,
          - déclenche bpy.ops.object.library_override_pushback() et vérifie la persistance.

  ———

  ### Phase 1 – Gestion hiérarchique des librairies imbriquées

  (fichiers C++ Outliner)

  1. Refactorisation de la remontée hiérarchique
      - Fichier : source/blender/editors/space_outliner/outliner_tools.cc
      - Fonction principale à modifier : id_override_library_create_hierarchy_pre_process(...) (ligne ~1077).
      - Actions :
          - Extraire la boucle while ((te = te->parent) != nullptr) dans un helper privé (ex. collect_hierarchy_chain(...)) pour clarifier
            le flux.
          - Lorsqu’un parent id_current_hierarchy_root appartient à une autre lib, ne pas return :
              - appeler data->id_root_add(id_current_hierarchy_root, ...) avec id_root_reference et id_instance_hint.
              - Continuer la remontée jusqu’à trouver un parent local ou un override réel.
          - Conserver un ordre de traitement (parent → enfant) : ajouter un champ Vector<ID *> hierarchy_chain à OutlinerLibOverrideData
            si nécessaire.
  2. Création effective des parents
      - Fichier : même (outliner_tools.cc)
      - Fonction : id_override_library_create_hierarchy(...) (ligne 1267) et son caller
        id_override_library_create_hierarchy_process(...).
      - Actions :
          - Avant de boucler sur data_idroots, trier la liste selon la profondeur (parents en premier).
          - ⚠️ La structure actuelle (Map<ID*, Vector<...>>) n'est pas ordonnée :
            * Option A : ajouter un champ depth dans OutlinerLiboverrideDataIDRoot et trier la Vector.
            * Option B : convertir la map en Vector ordonnée parent → enfant.
          - Si id_hierarchy_root_reference est encore lié, créer son override via BKE_lib_override_library_create(...) et l’utiliser comme
            nouvelle racine pour les enfants.
          - Dans la branche if (success && data_idroot.is_override_instancing_object), s’assurer que l’override remplace bien l’ancien
            parent (base unlink).
          - Après création, retirer LIBOVERRIDE_FLAG_SYSTEM_DEFINED pour les types manquants (y compris lights) et enregistrer la
            hiérarchie dans data.id_hierarchy_roots_uid.
  3. Gestion des erreurs
      - Toujours dans pre_process : remplacer les return par des continue lorsqu’une hiérarchie est simplement imbriquée.
      - Conserver les return uniquement si le parent est réellement « non overridable » (type non libéré, datas locales, etc.).

  ———

  ### Phase 2 – Exposer les propriétés manquantes (RNA)

  1. Node sockets
      - Fichier : source/blender/makesrna/intern/rna_node_socket.cc
      - Fonctions :
          - rna_def_node_socket_float(...) (ligne 981)
          - rna_def_node_socket_int(...) (ligne 1070)
          - rna_def_node_socket_vector(...) (ligne 1272)
          - rna_def_node_socket_color(...) (ligne 1357)
      - Actions : ajouter RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY); sur les propriétés default_value.
  - ⚠️ De nombreuses propriétés (lights, Material.node_tree) sont déjà marquées overrideables ; l'éditeur reste verrouillé à cause du flag LIBOVERRIDE_FLAG_SYSTEM_DEFINED.
      - Vérifier aussi les structs d’interface correspondants dans rna_node_socket.cc (fonctions
        rna_def_node_socket_interface_float, ..._vector, ..._color).
  2. Lights
      - Fichier : source/blender/makesrna/intern/rna_light.cc (si certaines propriétés sont encore marquées PROPOVERRIDE_NO_COMPARISON).
      - Ajuster les flags pour les principales propriétés (color, energy) si besoin.
      - Côté C++ : dans id_override_library_create_hierarchy_pre_process, lorsque ID_IS_OVERRIDE_LIBRARY_REAL(...), ajouter un bloc pour
        retirer LIBOVERRIDE_FLAG_SYSTEM_DEFINED pour les lights (similaire aux objets/collections).
  3. Node tree pointer
      - Optionnel : décider si on autorise l’override du pointer material.node_tree.
      - Si oui, ajouter le flag dans rna_material.cc (prop = RNA_def_property(srna, "node_tree", ...);).
      - l’idée est de rendre persistants les node trees externalisés, mais ce point peut venir dans un deuxième temps si on se limite aux default_value.

  ———

  ### Phase 3 – Intégration Python

  (Adapter notre opérateur si les nouveaux chemins apparaissent)

  - Fichier : scripts/startup/bl_operators/object.py
  - S’assurer que _collect_override_ids / execute ne filtrent plus les nouveaux chemins (les sockets étant maintenant sérialisés, rien à
    faire si on se contente d’exists).
  - Si on voit des default_value pointer vers des node trees ou lights (pointeurs), vérifier que la branche « skip pointer types » reste
    pertinente.
  - Adapter le reporting si besoin.

  ———

  ### Phase 4 – Tests

  1. Rebuild
      - ./make.bat release
  2. Scénarios
      - Cas non imbriqué :
          - Override d’un matériau (modifier la couleur du Principled) → chemin vert → push back → recharger la lib → valeur persistante.
          - Override d’une light → slider modifiable + push back.
      - Cas imbriqué :
          - Override d’un objet → l’original disparaît, override_library est renseigné.
          - Override d’un matériau imbriqué → plus de duplicata, properties éditables, push back OK.
          - Regression sur matériau non imbriqué.
  3. Automatisation
      - Script Python (basé sur Phase 0) qui exécute ces cas et vérifie override_library.properties.

  ———

  ### Phase 5 – Finalisation

  - Retirer ou conditionner les logs d’instrumentation s’il y en avait.
  - Mettre à jour agents.md avec un état final.
  - Préparer le patch ou la PR (discussions, tests, doc).
