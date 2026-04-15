/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "DNA_camera_types.h"
#include "DNA_constraint_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"

#include "BKE_armature.hh"
#include "BKE_constraint.h"
#include "BKE_curve.hh"
#include "BKE_curves.h"
#include "BKE_displist.h"
#include "BKE_editmesh.hh"
#include "BKE_grease_pencil.h"
#include "BKE_grease_pencil.hh"
#include "BKE_lattice.hh"
#include "BKE_layer.hh"
#include "BKE_mball.hh"
#include "BKE_mesh.hh"
#include "BKE_object.hh"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_pointcloud.hh"
#include "BKE_scene.hh"
#include "BKE_volume.hh"

#include "MEM_guardedalloc.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_light_linking.hh"
#include "DEG_depsgraph_query.hh"

namespace blender {

static void object_eval_camera_aim(Depsgraph *depsgraph, Object *ob)
{
  if ((ob->type != OB_CAMERA) || (ob->data == nullptr)) {
    return;
  }

  const Camera *camera = id_cast<const Camera *>(ob->data);
  if (((camera->aim_flag & CAM_AIM_TARGET_ENABLED) == 0) || (camera->aim_target == nullptr)) {
    return;
  }

  Object *target_eval = DEG_get_evaluated(depsgraph, camera->aim_target);
  if ((target_eval == nullptr) || (target_eval == ob)) {
    return;
  }

  Object *parent_eval = (ob->parent != nullptr) ? DEG_get_evaluated(depsgraph, ob->parent) : nullptr;

  float camera_loc[3];
  float target_loc[3];
  float target_to_camera[3];
  float x_axis[3];
  float y_axis[3];
  float z_axis[3];
  float base_rot[3][3];
  float object_rot[3][3];
  float up_ref[3];
  float fallback_up_y[3];
  float fallback_up_x[3];
  copy_v3_v3(camera_loc, ob->object_to_world().location());
  copy_v3_v3(target_loc, target_eval->object_to_world().location());
  sub_v3_v3v3(target_to_camera, camera_loc, target_loc);
  if (len_squared_v3(target_to_camera) <= 1.0e-12f) {
    return;
  }

  float roll_rot[3][3];
  float scale[3];
  float object_mat[4][4];
  const float world_up[3] = {0.0f, 0.0f, 1.0f};
  const float world_y[3] = {0.0f, 1.0f, 0.0f};
  const float world_x[3] = {1.0f, 0.0f, 0.0f};

  mat4_to_size(scale, ob->object_to_world().ptr());
  if (is_negative_m4(ob->object_to_world().ptr())) {
    scale[0] = -scale[0];
  }

  if (parent_eval != nullptr) {
    copy_v3_v3(up_ref, parent_eval->object_to_world()[2]);
    copy_v3_v3(fallback_up_y, parent_eval->object_to_world()[1]);
    copy_v3_v3(fallback_up_x, parent_eval->object_to_world()[0]);
    if (normalize_v3(up_ref) == 0.0f) {
      copy_v3_v3(up_ref, world_up);
    }
    if (normalize_v3(fallback_up_y) == 0.0f) {
      copy_v3_v3(fallback_up_y, world_y);
    }
    if (normalize_v3(fallback_up_x) == 0.0f) {
      copy_v3_v3(fallback_up_x, world_x);
    }
  }
  else {
    copy_v3_v3(up_ref, world_up);
    copy_v3_v3(fallback_up_y, world_y);
    copy_v3_v3(fallback_up_x, world_x);
  }

  normalize_v3_v3(z_axis, target_to_camera);

  project_v3_v3v3(y_axis, up_ref, z_axis);
  sub_v3_v3v3(y_axis, up_ref, y_axis);
  if (normalize_v3(y_axis) == 0.0f) {
    project_v3_v3v3(y_axis, fallback_up_y, z_axis);
    sub_v3_v3v3(y_axis, fallback_up_y, y_axis);
    if (normalize_v3(y_axis) == 0.0f) {
      project_v3_v3v3(y_axis, fallback_up_x, z_axis);
      sub_v3_v3v3(y_axis, fallback_up_x, y_axis);
      if (normalize_v3(y_axis) == 0.0f) {
        return;
      }
    }
  }

  cross_v3_v3v3(x_axis, y_axis, z_axis);
  normalize_v3(x_axis);
  cross_v3_v3v3(y_axis, z_axis, x_axis);
  normalize_v3(y_axis);

  copy_v3_v3(base_rot[0], x_axis);
  copy_v3_v3(base_rot[1], y_axis);
  copy_v3_v3(base_rot[2], z_axis);

  if (camera->aim_roll != 0.0f) {
    axis_angle_to_mat3_single(roll_rot, 'Z', camera->aim_roll);
    mul_m3_m3m3(object_rot, base_rot, roll_rot);
  }
  else {
    copy_m3_m3(object_rot, base_rot);
  }

  unit_m4(object_mat);
  copy_m4_m3(object_mat, object_rot);
  rescale_m4(object_mat, scale);
  copy_v3_v3(object_mat[3], camera_loc);

  copy_m4_m4(ob->runtime->object_to_world.ptr(), object_mat);
}

void BKE_object_eval_reset(Object *ob_eval)
{
  BKE_object_free_derived_caches(ob_eval);
}

void BKE_object_eval_local_transform(Depsgraph *depsgraph, Object *ob)
{
  DEG_debug_print_eval(depsgraph, __func__, ob->id.name, ob);

  /* calculate local matrix */
  BKE_object_to_mat4(ob, ob->runtime->object_to_world.ptr());
}

void BKE_object_eval_parent(Depsgraph *depsgraph, Object *ob)
{
  /* NOTE: based on `solve_parenting()`, but with the cruft stripped out. */

  Object *par = ob->parent;

  float totmat[4][4];
  float tmat[4][4];
  float locmat[4][4];

  DEG_debug_print_eval(depsgraph, __func__, ob->id.name, ob);

  /* get local matrix (but don't calculate it, as that was done already!) */
  /* XXX: redundant? */
  copy_m4_m4(locmat, ob->object_to_world().ptr());

  /* get parent effect matrix */
  BKE_object_get_parent_matrix(ob, par, totmat);

  /* total */
  mul_m4_m4m4(tmat, totmat, ob->parentinv);
  mul_m4_m4m4(ob->runtime->object_to_world.ptr(), tmat, locmat);

  /* origin, for help line */
  if ((ob->partype & PARTYPE) == PARSKEL) {
    copy_v3_v3(ob->runtime->parent_display_origin, par->object_to_world().location());
  }
  else {
    copy_v3_v3(ob->runtime->parent_display_origin, totmat[3]);
  }
}

void BKE_object_eval_constraints(Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  bConstraintOb *cob;
  float ctime = BKE_scene_ctime_get(scene);

  DEG_debug_print_eval(depsgraph, __func__, ob->id.name, ob);

  /* evaluate constraints stack */
  /* TODO: split this into:
   * - pre (i.e. BKE_constraints_make_evalob, per-constraint (i.e.
   * - inner body of BKE_constraints_solve),
   * - post (i.e. BKE_constraints_clear_evalob)
   *
   * Not sure why, this is from Joshua - sergey
   */
  cob = BKE_constraints_make_evalob(depsgraph, scene, ob, nullptr, CONSTRAINT_OBTYPE_OBJECT);
  BKE_constraints_solve(depsgraph, &ob->constraints, cob, ctime);
  BKE_constraints_clear_evalob(cob);
}

void BKE_object_eval_transform_final(Depsgraph *depsgraph, Object *ob)
{
  DEG_debug_print_eval(depsgraph, __func__, ob->id.name, ob);
  /* Make sure inverse matrix is always up to date. This way users of it
   * do not need to worry about recalculating it. */
  invert_m4_m4_safe(ob->runtime->world_to_object.ptr(), ob->object_to_world().ptr());
  /* Set negative scale flag in object. */
  if (is_negative_m4(ob->object_to_world().ptr())) {
    ob->transflag |= OB_NEG_SCALE;
  }
  else {
    ob->transflag &= ~OB_NEG_SCALE;
  }

  ob->runtime->last_update_transform = DEG_get_update_count(depsgraph);
}

void BKE_object_handle_data_update(Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  DEG_debug_print_eval(depsgraph, __func__, ob->id.name, ob);

  /* includes all keys and modifiers */
  switch (ob->type) {
    case OB_MESH: {
      CustomData_MeshMasks cddata_masks = scene->customdata_mask;
      CustomData_MeshMasks_update(&cddata_masks, &CD_MASK_BAREMESH);
      /* Custom attributes should not be removed automatically. They might be used by the render
       * engine or scripts. They can still be removed explicitly using geometry nodes.
       * Vertex groups can be used in arbitrary situations with geometry nodes as well. */
      cddata_masks.vmask |= CD_MASK_PROP_ALL | CD_MASK_MDEFORMVERT;
      cddata_masks.emask |= CD_MASK_PROP_ALL;
      cddata_masks.fmask |= CD_MASK_PROP_ALL;
      cddata_masks.pmask |= CD_MASK_PROP_ALL;
      cddata_masks.lmask |= CD_MASK_PROP_ALL;
      if (DEG_get_mode(depsgraph) == DAG_EVAL_RENDER) {
        /* Always compute orcos for render. */
        cddata_masks.vmask |= CD_MASK_ORCO;
      }
      bke::mesh_data_update(*depsgraph, *scene, *ob, cddata_masks);
      break;
    }
    case OB_ARMATURE:
      BKE_pose_where_is(depsgraph, scene, ob);
      break;

    case OB_MBALL:
      BKE_mball_data_update(depsgraph, scene, ob);
      break;

    case OB_CURVES_LEGACY:
    case OB_SURF:
    case OB_FONT: {
      bool for_render = (DEG_get_mode(depsgraph) == DAG_EVAL_RENDER);
      BKE_displist_make_curveTypes(depsgraph, scene, ob, for_render);
      break;
    }

    case OB_LATTICE:
      BKE_lattice_modifiers_calc(depsgraph, scene, ob);
      break;
    case OB_CURVES:
      BKE_curves_data_update(depsgraph, scene, ob);
      break;
    case OB_POINTCLOUD:
      BKE_pointcloud_data_update(depsgraph, scene, ob);
      break;
    case OB_VOLUME:
      BKE_volume_data_update(depsgraph, scene, ob);
      break;
    case OB_GREASE_PENCIL:
      BKE_object_eval_grease_pencil(depsgraph, scene, ob);
      break;
  }

  /* particles */
  if (!(ob->mode & OB_MODE_EDIT) && ob->particlesystem.first) {
    const bool use_render_params = (DEG_get_mode(depsgraph) == DAG_EVAL_RENDER);
    ParticleSystem *tpsys, *psys;
    ob->transflag &= ~OB_DUPLIPARTS;
    psys = static_cast<ParticleSystem *>(ob->particlesystem.first);
    while (psys) {
      if (psys_check_enabled(ob, psys, use_render_params)) {
        /* check use of dupli objects here */
        if (psys->part && (psys->part->draw_as == PART_DRAW_REND || use_render_params) &&
            ((psys->part->ren_as == PART_DRAW_OB && psys->part->instance_object) ||
             (psys->part->ren_as == PART_DRAW_GR && psys->part->instance_collection)))
        {
          ob->transflag |= OB_DUPLIPARTS;
        }

        particle_system_update(depsgraph, scene, ob, psys, use_render_params);
        psys = psys->next;
      }
      else if (psys->flag & PSYS_DELETE) {
        tpsys = psys->next;
        BLI_remlink(&ob->particlesystem, psys);
        psys_free(ob, psys);
        psys = tpsys;
      }
      else {
        psys = psys->next;
      }
    }
  }

  /* Cache the contained geometry types of the #geometry_set_eval. */
  if (ob->runtime->geometry_set_eval) {
    ob->runtime->contained_geometry_types = 0;
    for (const bke::GeometryComponent::Type type :
         ob->runtime->geometry_set_eval->gather_component_types(true, true))
    {
      ob->runtime->contained_geometry_types |= uint16_t(1 << size_t(type));
    }
  }

  if (DEG_is_active(depsgraph)) {
    Object *object_orig = DEG_get_original(ob);
    object_orig->runtime->bounds_eval = BKE_object_evaluated_geometry_bounds(ob);
  }
}

void BKE_object_sync_to_original(Depsgraph *depsgraph, Object *object)
{
  if (!DEG_is_active(depsgraph)) {
    return;
  }
  Object *object_orig = DEG_get_original(object);
  /* Base flags. */
  object_orig->base_flag = object->base_flag;
  object_orig->base_local_view_bits = object->base_local_view_bits;

  /* Particle edit mode draws from the original object, so sync imat from evaluated to original
   * object so drawing uses the correct transform. */
  for (ParticleSystem *
           psys_eval = static_cast<ParticleSystem *>(object->particlesystem.first),
          *psys_orig = static_cast<ParticleSystem *>(object_orig->particlesystem.first);
       psys_eval && psys_orig;
       psys_eval = psys_eval->next, psys_orig = psys_orig->next)
  {
    copy_m4_m4(psys_orig->imat, psys_eval->imat);
  }

  /* Transformation flags. */
  copy_m4_m4(object_orig->runtime->object_to_world.ptr(), object->object_to_world().ptr());
  copy_m4_m4(object_orig->runtime->world_to_object.ptr(), object->world_to_object().ptr());
  copy_m4_m4(object_orig->constinv, object->constinv);
  object_orig->transflag = object->transflag;
  object_orig->flag = object->flag;

  /* Copy back error messages from modifiers. */
  for (ModifierData *md = static_cast<ModifierData *>(object->modifiers.first),
                    *md_orig = static_cast<ModifierData *>(object_orig->modifiers.first);
       md != nullptr && md_orig != nullptr;
       md = md->next, md_orig = md_orig->next)
  {
    BLI_assert(md->type == md_orig->type && STREQ(md->name, md_orig->name));
    MEM_SAFE_DELETE(md_orig->error);
    if (md->error != nullptr) {
      md_orig->error = BLI_strdup(md->error);
    }
  }
}

void BKE_object_eval_uber_transform(Depsgraph *depsgraph, Object *object)
{
  DEG_debug_print_eval(depsgraph, __func__, object->id.name, object);
  object_eval_camera_aim(depsgraph, object);
}

void BKE_object_batch_cache_dirty_tag(Object *ob)
{
  switch (ob->type) {
    case OB_MESH:
      BKE_mesh_batch_cache_dirty_tag(id_cast<Mesh *>(ob->data), BKE_MESH_BATCH_DIRTY_ALL);
      break;
    case OB_LATTICE:
      BKE_lattice_batch_cache_dirty_tag(id_cast<Lattice *>(ob->data), BKE_LATTICE_BATCH_DIRTY_ALL);
      break;
    case OB_CURVES_LEGACY:
    case OB_SURF:
    case OB_FONT:
      BKE_curve_batch_cache_dirty_tag(id_cast<Curve *>(ob->data), BKE_CURVE_BATCH_DIRTY_ALL);
      break;
    case OB_MBALL: {
      /* This function is currently called on original objects, so to properly
       * clear the actual displayed geometry, we have to tag the evaluated mesh. */
      Mesh *mesh = BKE_object_get_evaluated_mesh_no_subsurf(ob);
      if (mesh) {
        BKE_mesh_batch_cache_dirty_tag(mesh, BKE_MESH_BATCH_DIRTY_ALL);
      }
      break;
    }
    case OB_CURVES:
      BKE_curves_batch_cache_dirty_tag(id_cast<Curves *>(ob->data), BKE_CURVES_BATCH_DIRTY_ALL);
      break;
    case OB_POINTCLOUD:
      BKE_pointcloud_batch_cache_dirty_tag(id_cast<PointCloud *>(ob->data),
                                           BKE_POINTCLOUD_BATCH_DIRTY_ALL);
      break;
    case OB_VOLUME:
      BKE_volume_batch_cache_dirty_tag(id_cast<Volume *>(ob->data), BKE_VOLUME_BATCH_DIRTY_ALL);
      break;
    case OB_GREASE_PENCIL:
      BKE_grease_pencil_batch_cache_dirty_tag(id_cast<GreasePencil *>(ob->data),
                                              BKE_GREASEPENCIL_BATCH_DIRTY_ALL);
      break;
    default:
      break;
  }
}

void BKE_object_eval_uber_data(Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  DEG_debug_print_eval(depsgraph, __func__, ob->id.name, ob);
  BLI_assert(ob->type != OB_ARMATURE);
  BKE_object_handle_data_update(depsgraph, scene, ob);
  BKE_object_batch_cache_dirty_tag(ob);

  ob->runtime->last_update_geometry = DEG_get_update_count(depsgraph);
}

void BKE_object_eval_ptcache_reset(Depsgraph *depsgraph, Scene *scene, Object *object)
{
  DEG_debug_print_eval(depsgraph, __func__, object->id.name, object);
  BKE_ptcache_object_reset(scene, object, PTCACHE_RESET_DEPSGRAPH);
}

void BKE_object_eval_transform_all(Depsgraph *depsgraph, Scene *scene, Object *object)
{
  /* This mimics full transform update chain from new depsgraph. */
  BKE_object_eval_local_transform(depsgraph, object);
  if (object->parent != nullptr) {
    BKE_object_eval_parent(depsgraph, object);
  }
  if (!BLI_listbase_is_empty(&object->constraints)) {
    BKE_object_eval_constraints(depsgraph, scene, object);
  }
  BKE_object_eval_uber_transform(depsgraph, object);
  BKE_object_eval_transform_final(depsgraph, object);
}

void BKE_object_data_select_update(Depsgraph *depsgraph, ID *object_data)
{
  DEG_debug_print_eval(depsgraph, __func__, object_data->name, object_data);
  switch (GS(object_data->name)) {
    case ID_ME:
      BKE_mesh_batch_cache_dirty_tag(id_cast<Mesh *>(object_data), BKE_MESH_BATCH_DIRTY_SELECT);
      break;
    case ID_CU_LEGACY:
      BKE_curve_batch_cache_dirty_tag(id_cast<Curve *>(object_data), BKE_CURVE_BATCH_DIRTY_SELECT);
      break;
    case ID_LT:
      BKE_lattice_batch_cache_dirty_tag(reinterpret_cast<Lattice *>(object_data),
                                        BKE_LATTICE_BATCH_DIRTY_SELECT);
      break;
    default:
      break;
  }
}

void BKE_object_select_update(Depsgraph *depsgraph, Object *object)
{
  DEG_debug_print_eval(depsgraph, __func__, object->id.name, object);
  if (object->type == OB_MESH && !object->runtime->is_data_eval_owned) {
    Mesh *mesh_input = id_cast<Mesh *>(object->runtime->data_orig);
    std::lock_guard lock{mesh_input->runtime->eval_mutex};
    BKE_object_data_select_update(depsgraph, object->data);
  }
  else {
    BKE_object_data_select_update(depsgraph, object->data);
  }
}

void BKE_object_eval_eval_base_flags(Depsgraph *depsgraph,
                                     Scene *scene,
                                     const int view_layer_index,
                                     Object *object,
                                     int base_index,
                                     const bool is_from_set)
{
  /* TODO(sergey): Avoid list lookup. */
  BLI_assert(view_layer_index >= 0);
  ViewLayer *view_layer = static_cast<ViewLayer *>(
      BLI_findlink(&scene->view_layers, view_layer_index));
  BLI_assert(view_layer != nullptr);
  BLI_assert(view_layer->object_bases_array != nullptr);
  BLI_assert(base_index >= 0);
  BLI_assert(base_index < MEM_allocN_len(view_layer->object_bases_array) / sizeof(Base *));
  Base *base = view_layer->object_bases_array[base_index];
  BLI_assert(base->object == object);

  DEG_debug_print_eval(depsgraph, __func__, object->id.name, object);

  /* Set base flags based on collection and object restriction. */
  BKE_base_eval_flags(base);

  /* For render, compute base visibility again since BKE_base_eval_flags
   * assumed viewport visibility. Select-ability does not matter here. */
  if (DEG_get_mode(depsgraph) == DAG_EVAL_RENDER) {
    /* If object is hidden in this view layer, disable it for rendering. */
    if (base->flag & BASE_HIDDEN) {
      base->flag &= ~BASE_ENABLED_RENDER;
    }
    
    if (base->flag & BASE_ENABLED_RENDER) {
      base->flag |= BASE_ENABLED_AND_MAYBE_VISIBLE_IN_VIEWPORT;
    }
    else {
      base->flag &= ~BASE_ENABLED_AND_MAYBE_VISIBLE_IN_VIEWPORT;
    }
  }

  /* Copy flags and settings from base. */
  object->base_flag = base->flag;
  if (is_from_set) {
    object->base_flag |= BASE_FROM_SET;
    object->base_flag &= ~(BASE_SELECTED | BASE_SELECTABLE);
  }
  object->base_local_view_bits = base->local_view_bits;
  object->runtime->local_collections_bits = base->local_collections_bits;

  if (object->mode == OB_MODE_PARTICLE_EDIT) {
    for (ParticleSystem *psys = static_cast<ParticleSystem *>(object->particlesystem.first);
         psys != nullptr;
         psys = psys->next)
    {
      BKE_particle_batch_cache_dirty_tag(psys, BKE_PARTICLE_BATCH_DIRTY_ALL);
    }
  }

  /* Copy base flag back to the original view layer for editing. */
  if (DEG_is_active(depsgraph) && (view_layer == DEG_get_evaluated_view_layer(depsgraph))) {
    Base *base_orig = base->base_orig;
    BLI_assert(base_orig != nullptr);
    BLI_assert(base_orig->object != nullptr);
    base_orig->flag = base->flag;
  }
}

void BKE_object_eval_light_linking(Depsgraph *depsgraph, Object *object)
{
  DEG_debug_print_eval(depsgraph, __func__, object->id.name, object);
  deg::light_linking::eval_runtime_data(depsgraph, *object);
}

void BKE_object_eval_shading(Depsgraph *depsgraph, Object *object)
{
  DEG_debug_print_eval(depsgraph, __func__, object->id.name, object);

  object->runtime->last_update_shading = DEG_get_update_count(depsgraph);
}

}  // namespace blender
