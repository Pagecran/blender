# Blender Agent Guidelines

## Build Commands (Windows)
- `./make.bat` - Build Blender (default release)
- `./make.bat debug` - Debug build
- `./make.bat lite` - Minimal build for faster compilation
- `./make.bat ninja` - Use Ninja for faster builds
- `./make.bat test` - Run all tests via ctest
- Single test: `ctest -C Release -R <test_name>` from build directory

## Code Style
- **C/C++**: snake_case, 2-space indent, 99 char line limit, clang-format enforced
- **Python**: PEP8, 120 char line limit, autopep8 formatting
- **Public functions**: `MODULE_object_operation()` (e.g., `BKE_curve_get`, `BLI_strnlen`)
- **Static functions**: No caps prefix, e.g., `static void my_helper_function(void);`
- **Return args**: `r_` prefix, placed at end of argument list
- **Size suffixes**: `_num` (count), `_size` (bytes), `_len` (string length)
- **C++ members**: Private members use trailing underscore (`my_data_`)
- **Namespaces**: Lowercase (e.g., `blender::bke`, `blender::io::alembic`)

## Formatting
- `./make.bat format` or `make format` - Format all code
- `./make.bat format PATHS="source/blender/blenlib"` - Format specific paths

## Checks
- `make check_cppcheck` - Static analysis
- `make check_pep8` - Python style check
- `make check_mypy` - Python type checking


### Support des Overrides sur les Propriétés de Nodes Shader

**Implementation :**
Extension du système de library override pour permettre la modification des propriétés des nodes shader dans les matériaux overridés.

**Fichiers Modifiés :**

1.  **`source/blender/makesrna/intern/rna_nodetree.cc` :**
    *   Ajout de `RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY)` sur de nombreuses propriétés de nodes shader :
        *   Nodes Math, Mix, Clamp, Map Range, Vector Math
        *   Nodes de texture (Image, Environment) : projection, interpolation, extension
        *   Nodes de shading (Principled BSDF, Glossy, Glass, Sheen, Refraction) : distribution, subsurface_method
        *   Nodes de mapping, bump, normal map, displacement
        *   ColorRamp, Mix RGB, etc.

2.  **`source/blender/makesrna/intern/rna_node_socket.cc` :**
    *   Ajout de `PROPOVERRIDE_OVERRIDABLE_LIBRARY` sur les types de sockets :
        *   Float socket (`default_value`)
        *   Vector socket (`default_value`)
        *   Color socket (`default_value`)
        *   Menu socket (`default_value`)
    *   Permet de modifier les valeurs par défaut des inputs non-connectés dans un matériau overridé.

**Résultat :**
Les propriétés des nodes shader (mode de blend, type de distribution, valeurs des sockets, etc.) peuvent maintenant être modifiées dans un matériau library override sans casser la référence au matériau original.


### Support des Overrides de Matériaux Imbriqués Cross-Library (L2)**

**Contexte :**
L'objectif est de permettre l'override d'un Matériau (situé dans `Lib B`) assigné à un Objet (situé dans `Lib A`), lui-même override localement. Blender standard interdit cela car la hiérarchie d'override traverse une frontière de bibliothèque (`Local -> Lib A -> Lib B`).

**Solution Technique Actuelle (Hybride C++/Python) :**
La solution fonctionnelle repose sur une modification du backend C++ exposée via un nouvel opérateur Python pour contourner les limitations de l'UI standard.

1.  **Backend C++ (Patched) :**
    *   **`source/blender/makesrna/intern/rna_ID.cc` :** `rna_ID_override_create` modifié pour détecter les matériaux liés et retrouver l'Objet Override parent automatiquement (pour définir `hierarchy_root`).
    *   **`source/blender/blenkernel/intern/lib_override.cc` :** Ajout de `BKE_lib_override_library_create_from_id_with_root` et désactivation des assertions de sécurité bloquant le cross-library.

2.  **Frontend Python (New Operator) :**
    *   **`scripts/startup/bl_operators/smart_material_override.py` :** Nouvel opérateur `outliner.smart_material_override` ("Make Library Override (Smart)").
    *   Il appelle `id.override_create(remap_local_usages=True)` qui déclenche la logique C++ patchée.
    *   Il est intégré dans le menu contextuel de l'Outliner (via modification de `bl_ui/space_outliner.py`).

**État de la Feature :**
*   ✅ **Fonctionne** : La création d'override Matériau L2 fonctionne via l'opérateur "Make Library Override (Smart)" ou l'API Python.
*   ⚠️ **Attention** : L'opérateur standard de l'UI C++ peut ne pas fonctionner correctement pour ce cas spécifique. Il est impératif d'utiliser l'opérateur "Smart" ou le script Python.

**Procédure de Test :**
*   **Script de Création** : `X:\_Github\blender\debug_override.py` (Ouvre le blend test, crée l'override via API, sauve).
*   **Script de Vérification** : `X:\_Github\blender\verify_fixed.py` (Vérifie la persistance de l'override et du lien hierarchy_root).
*   **Commande** : `blender.exe -b -P debug_override.py`
