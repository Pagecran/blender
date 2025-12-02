### Support des Overrides de Matériaux (L2)**

**Implementation :**
On a etendu le library overide aux nodetre et une partie des properties de shading (ça reste une implmetation WIP)


### Support des Overrides de Matériaux Imbriqués Cross-Library (L2)**

**Contexte :**
L'objectif est de permettre l'override d'un Matériau (situé dans `Lib B`) assigné à un Objet (situé dans `Lib A`), lui-même override localement. Blender standard interdit cela car la hiérarchie d'override traverse une frontière de bibliothèque (`Local -> Lib A -> Lib B`).

**État Actuel des Modifications (Commit actuel) :**

1.  **`source/blender/makesrna/intern/rna_ID.cc` :**
    *   Ajout des includes nécessaires (`DNA_object_types.h`, `BKE_material.hh`, `BKE_object.hh`).
    *   **Remplacement de `rna_ID_override_create` :** Implémentation d'une version "intelligente" (inspirée d'un backup `Pagec`) qui détecte si l'ID est un Matériau lié.
        *   Si oui, elle recherche l'Objet Override parent. Si absent, elle le crée.
        *   Elle appelle ensuite `BKE_lib_override_library_create_from_id_with_root` en passant l'Objet Override comme `hierarchy_root`.
        *   Cela force la création de l'override Matériau DANS la hiérarchie de l'Objet, au lieu d'en faire un override orphelin.

2.  **`source/blender/blenkernel/BKE_lib_override.hh` :**
    *   Ajout de la déclaration de `BKE_lib_override_library_create_from_id_with_root`.

3.  **`source/blender/blenkernel/intern/lib_override.cc` :**
    *   **Implémentation de `BKE_lib_override_library_create_from_id_with_root` :** Une variante de `create_from_id` qui accepte un argument `id_hierarchy_root` et configure `override->hierarchy_root` explicitement.
    *   **Désactivation des Checks de Sécurité (Cross-Lib) :** J'ai commenté plusieurs assertions et conditions qui bloquent les opérations entre bibliothèques différentes :
        *   `lib_override_hierarchy_dependencies_skip_check` (L931) : `other_id->lib != owner_id->lib`.
        *   `BKE_lib_override_library_create_from_tag` (L583) : Filtre `reference_id->lib == reference_library`. (Et ajout de `ID_IS_LINKED` pour sécurité).
        *   `lib_override_overrides_group_tag_recursive` (L1430) : `to_id_reference->lib != reference_lib`.
        *   `lib_override_library_create_post_process` (L1501) : `id_root->newid->lib != owner_library`.
        *   `lib_override_library_resync` (L2583) : `id->lib != id_root_reference->lib`.
        *   `lib_override_library_main_resync_on_library_indirect_level` (L3491) : `id_to->lib != id->lib`.
        *   `lib_override_library_main_hierarchy_id_root_ensure` (L1976) : `hierarchy_root->lib != id->lib`.
		
4.  **`une implemetation plus poussée a permis de faire fonctionner l'override quand on passe par l'API python` :**

✦ Voici la procédure que nous avons utilisée avec succès pour vérifier le fonctionnement des overrides via l'API Python :

   1. Script de Test (`debug_override.py`) :
       * Ce script est conçu pour être exécuté par Blender en mode background (-b).
       * Il ouvre le fichier .blend contenant la scène de test (lvl1_assigned_lv2.blend).
       * Il effectue les opérations d'override via l'API Python (obj.override_create(), mat.override_create()).
       * Il force manuellement la correction de la hiérarchie (hierarchy_root) et désactive le flag système (is_system_override = False) pour tenter de garantir
         la persistance.
       * Il sauvegarde le résultat dans un nouveau fichier (lvl1_assigned_lv2_fixed.blend).

      Commande pour lancer ce test :

   1     X:\_Github\build_windows_Lite_x64_vc18_Release\bin\Release\blender.exe -b -P debug_override.py

   2. Script de Vérification (`verify_fixed.py`) :
       * Ce script ouvre le fichier sauvegardé (lvl1_assigned_lv2_fixed.blend).
       * Il inspecte l'état des objets (Torus, Sphero) et de leurs matériaux.
       * Il vérifie si les matériaux sont toujours des overrides (Is Override: True) et s'ils sont correctement liés à leur parent (Hierarchy Root: Objet).
       * Il affiche "STATUS: OK" ou "STATUS: BROKEN".

      Commande pour lancer la vérification :

   1     X:\_Github\build_windows_Lite_x64_vc18_Release\bin\Release\blender.exe -b -P verify_fixed.py

  Cette procédure nous a permis de confirmer que l'override sur les materiaux l2 fonctionne avec la commande de l'API modifiée.
  
  
**Le Problème Restant :**

Malgré ces modifications qui permettent la **création** et l'**utilisation** de l'override L2 en session sans crash, l'override ne se fait toujours pas crorrectement quand on passe par l'UI de Blender et son code C++
Et au rechargement du fichier `.blend` (ou après un `revert`), l'override du Matériau est soit supprimé, soit corrompu (`Is Override: False` ou `Hierarchy Root` pointant sur lui-même au lieu de l'Objet).