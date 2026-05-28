---
id: DISCUSSION-001
title: Investigation commande UI matériaux objet vs data issue 42
status: closed
agent: product_manager
session_id: ses_192211b1bffelNzNE8ShGL3iPb
appended_message_ids:
  - msg_e6de1a93e001pFuRQRULo7dXoQ
  - msg_e6de2c2860015zjy0WUrksyFcZ
  - msg_e6de308ed001dyXwfi6jg3DWK9
  - msg_e6de3d964001kC5sR0KqCMtD0q
  - msg_e6de75866001oBJRZPhdSkSb1n
  - msg_e6e160d0f001p6uDIFmMNY5faO
---

# Discussion: Investigation commande UI matériaux objet vs data issue 42

## Messages

**User**
https://github.com/Pagecran/blender/issues/42#issue-4524674835
est-ce que tu peux regarder dans le code source de blender ce qu'il y a derriere cette commande de l'UI et pourquoi ça ne fonctionne pas si les materiaux sont appliqué au niveau objet plutot que data ?

**User**
ah attend avant ilfaut initialiser nomadworks

**product_manager**
Oui, tu as raison. J’ai initialisé NomadWorks en **full team mode**.

État actuel :
- `codemap.yml` créé
- registres/docs NomadWorks créés sous `docs/`
- discussion déjà capturée : `DISCUSSION-001`
- tâche d’investigation créée : `TASK-2026-05-28-001`

Je continue maintenant proprement via le workflow NomadWorks pour investiguer la commande Blender **Separate by Material** et le cas des matériaux liés à l’objet vs aux data.

**User**
ok vas y

**product_manager**
Oui. Analyse source faite.

La commande UI **Separate by Material** correspond à :

```python
bpy.ops.mesh.separate(type='MATERIAL')
```

Côté UI :
- `scripts/startup/bl_ui/space_view3d.py`
  - `layout.operator_menu_enum("mesh.separate", "type")`

Côté opérateur :
- `source/blender/editors/mesh/mesh_ops.cc`
  - enregistre `MESH_OT_separate`
- `source/blender/editors/mesh/editmesh_tools.cc`
  - `MESH_OT_separate()`
  - `edbm_separate_exec()`
  - `mesh_separate_material()`
  - `mesh_separate_tagged()`
  - `mesh_separate_material_assign_mat_nr()`

Le problème vient surtout de ce chemin :

```cpp
mesh_separate_material()
  -> mesh_separate_tagged()
  -> mesh_separate_material_assign_mat_nr()
```

La séparation groupe bien les faces par index matériau :

```cpp
BMFace.mat_nr
```

Mais lors de la création des nouveaux objets, le code recopie les matériaux via :

```cpp
BKE_object_material_array_assign(
    bmain,
    base_new->object,
    BKE_object_material_array_p(obedit),
    *BKE_object_material_len_p(obedit),
    false);
```

Or, pour un objet mesh, `BKE_object_material_array_p(obedit)` renvoie principalement le tableau de matériaux du **Mesh data** (`mesh->mat`), pas les matériaux liés à l’objet (`ob->mat` / `ob->matbits`).

Donc si le matériau visible est dans un slot **Object-linked**, la séparation utilise bien l’index de face, mais le nouvel objet ne récupère pas correctement le matériau object-level. Résultat : les objets séparés peuvent se retrouver sans matériau assigné dans leurs slots.

La zone probable à corriger est dans :

```text
source/blender/editors/mesh/editmesh_tools.cc
```

Autour de :

```cpp
mesh_separate_material_assign_mat_nr()
mesh_separate_material()
mesh_separate_tagged()
```

Recommandation : capturer explicitement depuis l’objet source, pour chaque `mat_nr` :
- le matériau effectif,
- le mode de liaison `matbits[mat_nr]`,
- puis réappliquer ce couple au nouvel objet séparé.

Caveat : l’analyse est statique, sans build/test Blender exécuté. Le futur correctif devrait inclure un test avec matériaux liés à l’objet puis `bpy.ops.mesh.separate(type='MATERIAL')`.

**User**
est-ce que tu peux faire l'implmentation et pouser le comit dans Pagec 5.1.1 et incrementer le numero de build stp (c documenté normalement)
