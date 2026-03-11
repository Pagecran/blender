/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BLI_listbase.h"
#include "BLI_math_geom.h"
#include "BLI_rect.h"

#include "BKE_action.hh"
#include "BKE_armature.hh"
#include "BKE_attribute.hh"
#include "BKE_customdata.hh"
#include "BKE_curve.hh"
#include "BKE_displist.h"
#include "BKE_editmesh.hh"
#include "BKE_modifier.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_iterators.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"

#include "DEG_depsgraph_query.hh"

#include "ANIM_armature.hh"

#include "bmesh.hh"

#include "ED_armature.hh"
#include "ED_view3d.hh"

/* -------------------------------------------------------------------- */
/** \name Internal Clipping Utilities
 * \{ */

/**
 * Calculate clipping planes to use when #V3D_PROJ_TEST_CLIP_CONTENT is enabled.
 *
 * Planes are selected from the viewpoint using `clip_flag`
 * to detect which planes should be applied (maximum 6).
 *
 * \return The number of planes written into `planes`.
 */
static int content_planes_from_clip_flag(const ARegion *region,
                                         const Object *ob,
                                         const eV3DProjTest clip_flag,
                                         float planes[6][4])
{
  BLI_assert(clip_flag & V3D_PROJ_TEST_CLIP_CONTENT);

  float *clip_xmin = nullptr, *clip_xmax = nullptr;
  float *clip_ymin = nullptr, *clip_ymax = nullptr;
  float *clip_zmin = nullptr, *clip_zmax = nullptr;

  int planes_len = 0;

  /* The order of `planes` has been selected based on the likelihood of points being fully
   * outside the plane to increase the chance of an early exit in #clip_segment_v3_plane_n.
   * With "near" being most likely and "far" being unlikely.
   *
   * Otherwise the order of axes in `planes` isn't significant. */

  if (clip_flag & V3D_PROJ_TEST_CLIP_NEAR) {
    clip_zmin = planes[planes_len++];
  }
  if (clip_flag & V3D_PROJ_TEST_CLIP_WIN) {
    clip_xmin = planes[planes_len++];
    clip_xmax = planes[planes_len++];
    clip_ymin = planes[planes_len++];
    clip_ymax = planes[planes_len++];
  }
  if (clip_flag & V3D_PROJ_TEST_CLIP_FAR) {
    clip_zmax = planes[planes_len++];
  }

  BLI_assert(planes_len <= 6);
  if (planes_len != 0) {
    RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
    const blender::float4x4 projection = ED_view3d_ob_project_mat_get(rv3d, ob);
    planes_from_projmat(
        projection.ptr(), clip_xmin, clip_xmax, clip_ymin, clip_ymax, clip_zmin, clip_zmax);
  }
  return planes_len;
}

/**
 * Edge projection is more involved since part of the edge may be behind the view
 * or extend beyond the far limits. In the case of single points, these can be ignored.
 * However it just may still be visible on screen, so constrained the edge to planes
 * defined by the port to ensure both ends of the edge can be projected, see #32214.
 *
 * \note This is unrelated to #V3D_PROJ_TEST_CLIP_BB which must be checked separately.
 */
static bool view3d_project_segment_to_screen_with_content_clip_planes(
    const ARegion *region,
    const float v_a[3],
    const float v_b[3],
    const eV3DProjTest clip_flag,
    const rctf *win_rect,
    const float content_planes[][4],
    const int content_planes_len,
    /* Output. */
    float r_screen_co_a[2],
    float r_screen_co_b[2])
{
  /* Clipping already handled, no need to check in projection. */
  eV3DProjTest clip_flag_nowin = clip_flag & ~V3D_PROJ_TEST_CLIP_WIN;

  const eV3DProjStatus status_a = ED_view3d_project_float_object(
      region, v_a, r_screen_co_a, clip_flag_nowin);
  const eV3DProjStatus status_b = ED_view3d_project_float_object(
      region, v_b, r_screen_co_b, clip_flag_nowin);

  if ((status_a == V3D_PROJ_RET_OK) && (status_b == V3D_PROJ_RET_OK)) {
    if (clip_flag & V3D_PROJ_TEST_CLIP_WIN) {
      if (!BLI_rctf_isect_segment(win_rect, r_screen_co_a, r_screen_co_b)) {
        return false;
      }
    }
  }
  else {
    if (content_planes_len == 0) {
      return false;
    }

    /* Both too near, ignore. */
    if ((status_a & V3D_PROJ_TEST_CLIP_NEAR) && (status_b & V3D_PROJ_TEST_CLIP_NEAR)) {
      return false;
    }

    /* Both too far, ignore. */
    if ((status_a & V3D_PROJ_TEST_CLIP_FAR) && (status_b & V3D_PROJ_TEST_CLIP_FAR)) {
      return false;
    }

    /* Simple cases have been ruled out, clip by viewport planes, then re-project. */
    float v_a_clip[3], v_b_clip[3];
    if (!clip_segment_v3_plane_n(v_a, v_b, content_planes, content_planes_len, v_a_clip, v_b_clip))
    {
      return false;
    }

    if ((ED_view3d_project_float_object(region, v_a_clip, r_screen_co_a, clip_flag_nowin) !=
         V3D_PROJ_RET_OK) ||
        (ED_view3d_project_float_object(region, v_b_clip, r_screen_co_b, clip_flag_nowin) !=
         V3D_PROJ_RET_OK))
    {
      return false;
    }

    /* No need for #V3D_PROJ_TEST_CLIP_WIN check here,
     * clipping the segment by planes handle this. */
  }

  return true;
}

/**
 * Project an edge, points that fail to project are tagged with #IS_CLIPPED.
 */
static bool view3d_project_segment_to_screen_with_clip_tag(const ARegion *region,
                                                           const float v_a[3],
                                                           const float v_b[3],
                                                           const eV3DProjTest clip_flag,
                                                           /* Output. */
                                                           float r_screen_co_a[2],
                                                           float r_screen_co_b[2])
{
  int count = 0;

  if (ED_view3d_project_float_object(region, v_a, r_screen_co_a, clip_flag) == V3D_PROJ_RET_OK) {
    count++;
  }
  else {
    r_screen_co_a[0] = IS_CLIPPED; /* weak */
    /* screen_co_a[1]: intentionally don't set this so we get errors on misuse */
  }

  if (ED_view3d_project_float_object(region, v_b, r_screen_co_b, clip_flag) == V3D_PROJ_RET_OK) {
    count++;
  }
  else {
    r_screen_co_b[0] = IS_CLIPPED; /* weak */
    /* screen_co_b[1]: intentionally don't set this so we get errors on misuse */
  }

  /* Caller may want to know this value, for now it's not needed. */
  return count != 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Private User Data Structures
 * \{ */

struct foreachScreenObjectVert_userData {
  void (*func)(void *user_data, const float screen_co[2], int index);
  void *user_data;
  ViewContext vc;
  blender::VArraySpan<bool> hide_vert;
  eV3DProjTest clip_flag;
};

struct foreachScreenVert_userData {
  void (*func)(void *user_data, BMVert *eve, const float screen_co[2], int index);
  void *user_data;
  ViewContext vc;
  eV3DProjTest clip_flag;
};

/** User data structures for evaluated mesh callbacks. */
struct foreachScreenEdge_userData {
  void (*func)(void *user_data,
               BMEdge *eed,
               const float screen_co_a[2],
               const float screen_co_b[2],
               int index);
  void *user_data;
  ViewContext vc;
  eV3DProjTest clip_flag;

  rctf win_rect; /* copy of: vc.region->winx/winy, use for faster tests, minx/y will always be 0 */

  /**
   * Clip plans defined by the view bounds,
   * use when #V3D_PROJ_TEST_CLIP_CONTENT is enabled.
   */
  float content_planes[6][4];
  int content_planes_len;
};

struct foreachScreenFaceCenter_userData {
  void (*func)(void *user_data, BMFace *efa, const float screen_co_b[2], int index);
  void *user_data;
  ViewContext vc;
  eV3DProjTest clip_flag;
};

/**
 * \note foreach functions should be called while drawing or directly after
 * if not, #ED_view3d_init_mats_rv3d() can be used for selection tools
 * but would not give correct results with dupli's for eg. which don't
 * use the object matrix in the usual way.
 */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit-Mesh: For Each Screen Vertex
 * \{ */

static void meshobject_foreachScreenVert__mapFunc(void *user_data,
                                                  int index,
                                                  const float co[3],
                                                  const float /*no*/[3])
{
  foreachScreenObjectVert_userData *data = static_cast<foreachScreenObjectVert_userData *>(
      user_data);
  if (!data->hide_vert.is_empty() && data->hide_vert[index]) {
    return;
  }

  float screen_co[2];

  if (ED_view3d_project_float_object(data->vc.region, co, screen_co, data->clip_flag) !=
      V3D_PROJ_RET_OK)
  {
    return;
  }

  data->func(data->user_data, screen_co, index);
}

void meshobject_foreachScreenVert(const ViewContext *vc,
                                  void (*func)(void *user_data,
                                               const float screen_co[2],
                                               int index),
                                  void *user_data,
                                  eV3DProjTest clip_flag)
{
  using namespace blender;
  BLI_assert((clip_flag & V3D_PROJ_TEST_CLIP_CONTENT) == 0);
  foreachScreenObjectVert_userData data;

  const Object *ob_eval = DEG_get_evaluated(vc->depsgraph, vc->obact);
  const Mesh *mesh = BKE_object_get_evaluated_mesh(ob_eval);
  const bke::AttributeAccessor attributes = mesh->attributes();

  ED_view3d_check_mats_rv3d(vc->rv3d);

  data.vc = *vc;
  data.func = func;
  data.user_data = user_data;
  data.clip_flag = clip_flag;
  data.hide_vert = *attributes.lookup<bool>(".hide_vert", bke::AttrDomain::Point);

  if (clip_flag & V3D_PROJ_TEST_CLIP_BB) {
    ED_view3d_clipping_local(vc->rv3d, vc->obact->object_to_world().ptr());
  }

  BKE_mesh_foreach_mapped_vert(
      mesh, meshobject_foreachScreenVert__mapFunc, &data, MESH_FOREACH_NOP);
}

static void mesh_foreachScreenVert__mapFunc(void *user_data,
                                            int index,
                                            const float co[3],
                                            const float /*no*/[3])
{
  foreachScreenVert_userData *data = static_cast<foreachScreenVert_userData *>(user_data);
  BMVert *eve = BM_vert_at_index(data->vc.em->bm, index);
  if (UNLIKELY(BM_elem_flag_test(eve, BM_ELEM_HIDDEN))) {
    return;
  }

  float screen_co[2];
  if (ED_view3d_project_float_object(data->vc.region, co, screen_co, data->clip_flag) !=
      V3D_PROJ_RET_OK)
  {
    return;
  }

  data->func(data->user_data, eve, screen_co, index);
}

void mesh_foreachScreenVert(
    const ViewContext *vc,
    void (*func)(void *user_data, BMVert *eve, const float screen_co[2], int index),
    void *user_data,
    eV3DProjTest clip_flag)
{
  foreachScreenVert_userData data;

  Mesh *mesh = blender::bke::editbmesh_get_eval_cage_from_orig(
      vc->depsgraph, vc->scene, vc->obedit, &CD_MASK_BAREMESH);
  mesh = BKE_mesh_wrapper_ensure_subdivision(mesh);

  ED_view3d_check_mats_rv3d(vc->rv3d);

  data.vc = *vc;
  data.func = func;
  data.user_data = user_data;
  data.clip_flag = clip_flag;

  if (clip_flag & V3D_PROJ_TEST_CLIP_BB) {
    ED_view3d_clipping_local(vc->rv3d,
                             vc->obedit->object_to_world().ptr()); /* for local clipping lookups */
  }

  BM_mesh_elem_table_ensure(vc->em->bm, BM_VERT);
  BKE_mesh_foreach_mapped_vert(mesh, mesh_foreachScreenVert__mapFunc, &data, MESH_FOREACH_NOP);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit-Mesh: For Each Screen Mesh Edge
 * \{ */

static void mesh_foreachScreenEdge__mapFunc(void *user_data,
                                            int index,
                                            const float v_a[3],
                                            const float v_b[3])
{
  foreachScreenEdge_userData *data = static_cast<foreachScreenEdge_userData *>(user_data);
  BMEdge *eed = BM_edge_at_index(data->vc.em->bm, index);
  if (UNLIKELY(BM_elem_flag_test(eed, BM_ELEM_HIDDEN))) {
    return;
  }

  float screen_co_a[2], screen_co_b[2];
  if (!view3d_project_segment_to_screen_with_content_clip_planes(data->vc.region,
                                                                 v_a,
                                                                 v_b,
                                                                 data->clip_flag,
                                                                 &data->win_rect,
                                                                 data->content_planes,
                                                                 data->content_planes_len,
                                                                 screen_co_a,
                                                                 screen_co_b))
  {
    return;
  }

  data->func(data->user_data, eed, screen_co_a, screen_co_b, index);
}

void mesh_foreachScreenEdge(const ViewContext *vc,
                            void (*func)(void *user_data,
                                         BMEdge *eed,
                                         const float screen_co_a[2],
                                         const float screen_co_b[2],
                                         int index),
                            void *user_data,
                            eV3DProjTest clip_flag)
{
  foreachScreenEdge_userData data;

  Mesh *mesh = blender::bke::editbmesh_get_eval_cage_from_orig(
      vc->depsgraph, vc->scene, vc->obedit, &CD_MASK_BAREMESH);
  mesh = BKE_mesh_wrapper_ensure_subdivision(mesh);

  ED_view3d_check_mats_rv3d(vc->rv3d);

  data.vc = *vc;

  data.win_rect.xmin = 0;
  data.win_rect.ymin = 0;
  data.win_rect.xmax = vc->region->winx;
  data.win_rect.ymax = vc->region->winy;

  data.func = func;
  data.user_data = user_data;
  data.clip_flag = clip_flag;

  if (clip_flag & V3D_PROJ_TEST_CLIP_BB) {
    ED_view3d_clipping_local(vc->rv3d,
                             vc->obedit->object_to_world().ptr()); /* for local clipping lookups */
  }

  if (clip_flag & V3D_PROJ_TEST_CLIP_CONTENT) {
    data.content_planes_len = content_planes_from_clip_flag(
        vc->region, vc->obedit, clip_flag, data.content_planes);
  }
  else {
    data.content_planes_len = 0;
  }

  BM_mesh_elem_table_ensure(vc->em->bm, BM_EDGE);
  BKE_mesh_foreach_mapped_edge(mesh, vc->em->bm->totedge, mesh_foreachScreenEdge__mapFunc, &data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit-Mesh: For Each Screen Edge (Bounding Box Clipped)
 * \{ */

/**
 * Only call for bound-box clipping.
 * Otherwise call #mesh_foreachScreenEdge__mapFunc
 */
static void mesh_foreachScreenEdge_clip_bb_segment__mapFunc(void *user_data,
                                                            int index,
                                                            const float v_a[3],
                                                            const float v_b[3])
{
  foreachScreenEdge_userData *data = static_cast<foreachScreenEdge_userData *>(user_data);
  BMEdge *eed = BM_edge_at_index(data->vc.em->bm, index);
  if (UNLIKELY(BM_elem_flag_test(eed, BM_ELEM_HIDDEN))) {
    return;
  }

  BLI_assert(data->clip_flag & V3D_PROJ_TEST_CLIP_BB);

  float v_a_clip[3], v_b_clip[3];
  if (!clip_segment_v3_plane_n(v_a, v_b, data->vc.rv3d->clip_local, 4, v_a_clip, v_b_clip)) {
    return;
  }

  float screen_co_a[2], screen_co_b[2];
  if (!view3d_project_segment_to_screen_with_content_clip_planes(data->vc.region,
                                                                 v_a_clip,
                                                                 v_b_clip,
                                                                 data->clip_flag,
                                                                 &data->win_rect,
                                                                 data->content_planes,
                                                                 data->content_planes_len,
                                                                 screen_co_a,
                                                                 screen_co_b))
  {
    return;
  }

  data->func(data->user_data, eed, screen_co_a, screen_co_b, index);
}

void mesh_foreachScreenEdge_clip_bb_segment(const ViewContext *vc,
                                            void (*func)(void *user_data,
                                                         BMEdge *eed,
                                                         const float screen_co_a[2],
                                                         const float screen_co_b[2],
                                                         int index),
                                            void *user_data,
                                            eV3DProjTest clip_flag)
{
  foreachScreenEdge_userData data;

  Mesh *mesh = blender::bke::editbmesh_get_eval_cage_from_orig(
      vc->depsgraph, vc->scene, vc->obedit, &CD_MASK_BAREMESH);
  mesh = BKE_mesh_wrapper_ensure_subdivision(mesh);

  ED_view3d_check_mats_rv3d(vc->rv3d);

  data.vc = *vc;

  data.win_rect.xmin = 0;
  data.win_rect.ymin = 0;
  data.win_rect.xmax = vc->region->winx;
  data.win_rect.ymax = vc->region->winy;

  data.func = func;
  data.user_data = user_data;
  data.clip_flag = clip_flag;

  if (clip_flag & V3D_PROJ_TEST_CLIP_CONTENT) {
    data.content_planes_len = content_planes_from_clip_flag(
        vc->region, vc->obedit, clip_flag, data.content_planes);
  }
  else {
    data.content_planes_len = 0;
  }

  BM_mesh_elem_table_ensure(vc->em->bm, BM_EDGE);

  if ((clip_flag & V3D_PROJ_TEST_CLIP_BB) && (vc->rv3d->clipbb != nullptr)) {
    ED_view3d_clipping_local(
        vc->rv3d, vc->obedit->object_to_world().ptr()); /* for local clipping lookups. */
    BKE_mesh_foreach_mapped_edge(
        mesh, vc->em->bm->totedge, mesh_foreachScreenEdge_clip_bb_segment__mapFunc, &data);
  }
  else {
    BKE_mesh_foreach_mapped_edge(
        mesh, vc->em->bm->totedge, mesh_foreachScreenEdge__mapFunc, &data);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit-Mesh: For Each Screen Face Center
 * \{ */

static void mesh_foreachScreenFaceCenter__mapFunc(void *user_data,
                                                  int index,
                                                  const float cent[3],
                                                  const float /*no*/[3])
{
  foreachScreenFaceCenter_userData *data = static_cast<foreachScreenFaceCenter_userData *>(
      user_data);
  BMFace *efa = BM_face_at_index(data->vc.em->bm, index);
  if (UNLIKELY(BM_elem_flag_test(efa, BM_ELEM_HIDDEN))) {
    return;
  }

  float screen_co[2];
  if (ED_view3d_project_float_object(data->vc.region, cent, screen_co, data->clip_flag) !=
      V3D_PROJ_RET_OK)
  {
    return;
  }

  data->func(data->user_data, efa, screen_co, index);
}

void mesh_foreachScreenFaceCenter(
    const ViewContext *vc,
    void (*func)(void *user_data, BMFace *efa, const float screen_co_b[2], int index),
    void *user_data,
    const eV3DProjTest clip_flag)
{
  BLI_assert((clip_flag & V3D_PROJ_TEST_CLIP_CONTENT) == 0);
  foreachScreenFaceCenter_userData data;

  Mesh *mesh = blender::bke::editbmesh_get_eval_cage_from_orig(
      vc->depsgraph, vc->scene, vc->obedit, &CD_MASK_BAREMESH);
  mesh = BKE_mesh_wrapper_ensure_subdivision(mesh);
  ED_view3d_check_mats_rv3d(vc->rv3d);

  data.vc = *vc;
  data.func = func;
  data.user_data = user_data;
  data.clip_flag = clip_flag;

  BM_mesh_elem_table_ensure(vc->em->bm, BM_FACE);

  const int face_dot_tags_num = mesh->runtime->subsurf_face_dot_tags.size();
  if (face_dot_tags_num && (face_dot_tags_num != mesh->verts_num)) {
    BKE_mesh_foreach_mapped_subdiv_face_center(
        mesh, mesh_foreachScreenFaceCenter__mapFunc, &data, MESH_FOREACH_NOP);
  }
  else {
    BKE_mesh_foreach_mapped_face_center(
        mesh, mesh_foreachScreenFaceCenter__mapFunc, &data, MESH_FOREACH_NOP);
  }
}

void mesh_foreachScreenFaceVerts(
    const ViewContext *vc,
    void (*func)(void *user_data,
                 BMFace *efa,
                 const float screen_co[][2],
                 int total_count,
                 rctf *screen_rect,
                 bool *face_hit),
    void *user_data,
    const eV3DProjTest clip_flag)
{
  using namespace blender;

  Mesh *mesh = blender::bke::editbmesh_get_eval_cage_from_orig(
      vc->depsgraph, vc->scene, vc->obedit, &CD_MASK_BAREMESH);
  mesh = BKE_mesh_wrapper_ensure_subdivision(mesh);
  ED_view3d_check_mats_rv3d(vc->rv3d);

  if (clip_flag & V3D_PROJ_TEST_CLIP_BB) {
    ED_view3d_clipping_local(vc->rv3d, vc->obedit->object_to_world().ptr());
  }

  BM_mesh_elem_table_ensure(vc->em->bm, BM_FACE);

  const bool cage_display = BKE_modifiers_get_cage_index(vc->scene, vc->obedit, nullptr, true) !=
                            -1;
  Array<bool> faces_visited(cage_display ? vc->em->bm->totface : 0, false);

  const Span<float3> positions = mesh->vert_positions();
  const OffsetIndices faces = mesh->faces();
  const Span<int> corner_verts = mesh->corner_verts();
  const int *face_orig_indices = static_cast<const int *>(
      CustomData_get_layer(&mesh->face_data, CD_ORIGINDEX));

  Array<float2> screen_coords(positions.size(), float2(0.0f, 0.0f));
  Array<bool> vert_valid(positions.size(), false);
  for (const int i : positions.index_range()) {
    float screen_co[2];
    if (ED_view3d_project_float_object(vc->region, positions[i], screen_co, clip_flag) ==
        V3D_PROJ_RET_OK)
    {
      screen_coords[i] = float2(screen_co[0], screen_co[1]);
      vert_valid[i] = true;
    }
  }

  Vector<float2, 16> face_screen_verts;
  bool any_face_processed = false;
  for (const int face_i : faces.index_range()) {
    int orig_face_i = face_i;
    if (face_orig_indices != nullptr) {
      orig_face_i = face_orig_indices[face_i];
      if (orig_face_i == ORIGINDEX_NONE) {
        continue;
      }
    }

    if (orig_face_i >= vc->em->bm->totface) {
      continue;
    }
    if (cage_display && faces_visited[orig_face_i]) {
      continue;
    }

    BMFace *efa = BM_face_at_index(vc->em->bm, orig_face_i);
    if (UNLIKELY(BM_elem_flag_test(efa, BM_ELEM_HIDDEN))) {
      continue;
    }

    face_screen_verts.clear();
    face_screen_verts.reserve(faces[face_i].size());

    rctf screen_rect;
    BLI_rctf_init_minmax(&screen_rect);
    bool skip = false;
    for (const int corner : faces[face_i]) {
      const int vert_i = corner_verts[corner];
      if (!vert_valid[vert_i]) {
        skip = true;
        break;
      }
      face_screen_verts.append(screen_coords[vert_i]);
      BLI_rctf_do_minmax_v(&screen_rect, screen_coords[vert_i]);
    }

    if (skip || face_screen_verts.size() < 3) {
      continue;
    }

    any_face_processed = true;
    bool face_hit = false;
    func(user_data,
         efa,
         reinterpret_cast<const float(*)[2]>(face_screen_verts.data()),
         face_screen_verts.size(),
         &screen_rect,
         &face_hit);

    if (cage_display && face_hit) {
      faces_visited[orig_face_i] = true;
    }
  }

  if (!any_face_processed) {
    BMIter iter;
    BMFace *efa;

    BM_ITER_MESH (efa, &iter, vc->em->bm, BM_FACES_OF_MESH) {
      if (UNLIKELY(BM_elem_flag_test(efa, BM_ELEM_HIDDEN))) {
        continue;
      }

      face_screen_verts.clear();
      face_screen_verts.reserve(efa->len);

      rctf screen_rect;
      BLI_rctf_init_minmax(&screen_rect);
      bool skip = false;

      BMLoop *l_iter = BM_FACE_FIRST_LOOP(efa);
      BMLoop *l_first = l_iter;
      do {
        float screen_co[2];
        if (ED_view3d_project_float_object(vc->region, l_iter->v->co, screen_co, clip_flag) !=
            V3D_PROJ_RET_OK)
        {
          skip = true;
          break;
        }
        face_screen_verts.append(float2(screen_co[0], screen_co[1]));
        BLI_rctf_do_minmax_v(&screen_rect, screen_co);
      } while ((l_iter = l_iter->next) != l_first);

      if (skip || face_screen_verts.size() < 3) {
        continue;
      }

      bool face_hit = false;
      func(user_data,
           efa,
           reinterpret_cast<const float(*)[2]>(face_screen_verts.data()),
           face_screen_verts.size(),
           &screen_rect,
           &face_hit);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit-Nurbs: For Each Screen Vertex
 * \{ */

void nurbs_foreachScreenVert(const ViewContext *vc,
                             void (*func)(void *user_data,
                                          Nurb *nu,
                                          BPoint *bp,
                                          BezTriple *bezt,
                                          int beztindex,
                                          bool handles_visible,
                                          const float screen_co_b[2]),
                             void *user_data,
                             const eV3DProjTest clip_flag)
{
  Curve *cu = static_cast<Curve *>(vc->obedit->data);
  int i;
  ListBase *nurbs = BKE_curve_editNurbs_get(cu);
  /* If no point in the triple is selected, the handles are invisible. */
  const bool only_selected = (vc->v3d->overlay.handle_display == CURVE_HANDLE_SELECTED);

  ED_view3d_check_mats_rv3d(vc->rv3d);

  if (clip_flag & V3D_PROJ_TEST_CLIP_BB) {
    ED_view3d_clipping_local(vc->rv3d,
                             vc->obedit->object_to_world().ptr()); /* for local clipping lookups */
  }

  LISTBASE_FOREACH (Nurb *, nu, nurbs) {
    if (nu->type == CU_BEZIER) {
      for (i = 0; i < nu->pntsu; i++) {
        BezTriple *bezt = &nu->bezt[i];

        if (bezt->hide == 0) {
          const bool handles_visible = (vc->v3d->overlay.handle_display != CURVE_HANDLE_NONE) &&
                                       (!only_selected || BEZT_ISSEL_ANY(bezt));
          float screen_co[2];

          if (!handles_visible) {
            if (ED_view3d_project_float_object(
                    vc->region,
                    bezt->vec[1],
                    screen_co,
                    eV3DProjTest(V3D_PROJ_RET_CLIP_BB | V3D_PROJ_RET_CLIP_WIN)) == V3D_PROJ_RET_OK)
            {
              func(user_data, nu, nullptr, bezt, 1, false, screen_co);
            }
          }
          else {
            if (ED_view3d_project_float_object(
                    vc->region,
                    bezt->vec[0],
                    screen_co,
                    eV3DProjTest(V3D_PROJ_RET_CLIP_BB | V3D_PROJ_RET_CLIP_WIN)) == V3D_PROJ_RET_OK)
            {
              func(user_data, nu, nullptr, bezt, 0, true, screen_co);
            }
            if (ED_view3d_project_float_object(
                    vc->region,
                    bezt->vec[1],
                    screen_co,
                    eV3DProjTest(V3D_PROJ_RET_CLIP_BB | V3D_PROJ_RET_CLIP_WIN)) == V3D_PROJ_RET_OK)
            {
              func(user_data, nu, nullptr, bezt, 1, true, screen_co);
            }
            if (ED_view3d_project_float_object(
                    vc->region,
                    bezt->vec[2],
                    screen_co,
                    eV3DProjTest(V3D_PROJ_RET_CLIP_BB | V3D_PROJ_RET_CLIP_WIN)) == V3D_PROJ_RET_OK)
            {
              func(user_data, nu, nullptr, bezt, 2, true, screen_co);
            }
          }
        }
      }
    }
    else {
      for (i = 0; i < nu->pntsu * nu->pntsv; i++) {
        BPoint *bp = &nu->bp[i];

        if (bp->hide == 0) {
          float screen_co[2];
          if (ED_view3d_project_float_object(
                  vc->region,
                  bp->vec,
                  screen_co,
                  eV3DProjTest(V3D_PROJ_RET_CLIP_BB | V3D_PROJ_RET_CLIP_WIN)) == V3D_PROJ_RET_OK)
          {
            func(user_data, nu, bp, nullptr, -1, false, screen_co);
          }
        }
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit-Meta: For Each Screen Meta-Element
 * \{ */

void mball_foreachScreenElem(const ViewContext *vc,
                             void (*func)(void *user_data,
                                          MetaElem *ml,
                                          const float screen_co_b[2]),
                             void *user_data,
                             const eV3DProjTest clip_flag)
{
  MetaBall *mb = (MetaBall *)vc->obedit->data;

  ED_view3d_check_mats_rv3d(vc->rv3d);

  LISTBASE_FOREACH (MetaElem *, ml, mb->editelems) {
    float screen_co[2];
    if (ED_view3d_project_float_object(vc->region, &ml->x, screen_co, clip_flag) ==
        V3D_PROJ_RET_OK)
    {
      func(user_data, ml, screen_co);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit-Lattice: For Each Screen Vertex
 * \{ */

void lattice_foreachScreenVert(const ViewContext *vc,
                               void (*func)(void *user_data, BPoint *bp, const float screen_co[2]),
                               void *user_data,
                               const eV3DProjTest clip_flag)
{
  Object *obedit = vc->obedit;
  Lattice *lt = static_cast<Lattice *>(obedit->data);
  BPoint *bp = lt->editlatt->latt->def;
  DispList *dl = obedit->runtime->curve_cache ?
                     BKE_displist_find(&obedit->runtime->curve_cache->disp, DL_VERTS) :
                     nullptr;
  const float *co = dl ? dl->verts : nullptr;
  int i, N = lt->editlatt->latt->pntsu * lt->editlatt->latt->pntsv * lt->editlatt->latt->pntsw;

  ED_view3d_check_mats_rv3d(vc->rv3d);

  if (clip_flag & V3D_PROJ_TEST_CLIP_BB) {
    ED_view3d_clipping_local(vc->rv3d,
                             obedit->object_to_world().ptr()); /* for local clipping lookups */
  }

  for (i = 0; i < N; i++, bp++, co += 3) {
    if (bp->hide == 0) {
      float screen_co[2];
      if (ED_view3d_project_float_object(vc->region, dl ? co : bp->vec, screen_co, clip_flag) ==
          V3D_PROJ_RET_OK)
      {
        func(user_data, bp, screen_co);
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit-Armature: For Each Screen Bone
 * \{ */

void armature_foreachScreenBone(const ViewContext *vc,
                                void (*func)(void *user_data,
                                             EditBone *ebone,
                                             const float screen_co_a[2],
                                             const float screen_co_b[2]),
                                void *user_data,
                                const eV3DProjTest clip_flag)
{
  bArmature *arm = static_cast<bArmature *>(vc->obedit->data);

  ED_view3d_check_mats_rv3d(vc->rv3d);

  float content_planes[6][4];
  int content_planes_len;
  rctf win_rect;

  if (clip_flag & V3D_PROJ_TEST_CLIP_CONTENT) {
    content_planes_len = content_planes_from_clip_flag(
        vc->region, vc->obedit, clip_flag, content_planes);
    win_rect.xmin = 0;
    win_rect.ymin = 0;
    win_rect.xmax = vc->region->winx;
    win_rect.ymax = vc->region->winy;
  }
  else {
    content_planes_len = 0;
  }

  LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
    if (!blender::animrig::bone_is_visible(arm, ebone)) {
      continue;
    }

    float screen_co_a[2], screen_co_b[2];
    const float *v_a = ebone->head, *v_b = ebone->tail;

    if (clip_flag & V3D_PROJ_TEST_CLIP_CONTENT) {
      if (!view3d_project_segment_to_screen_with_content_clip_planes(vc->region,
                                                                     v_a,
                                                                     v_b,
                                                                     clip_flag,
                                                                     &win_rect,
                                                                     content_planes,
                                                                     content_planes_len,
                                                                     screen_co_a,
                                                                     screen_co_b))
      {
        continue;
      }
    }
    else {
      if (!view3d_project_segment_to_screen_with_clip_tag(
              vc->region, v_a, v_b, clip_flag, screen_co_a, screen_co_b))
      {
        continue;
      }
    }

    func(user_data, ebone, screen_co_a, screen_co_b);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pose: For Each Screen Bone
 * \{ */

void pose_foreachScreenBone(const ViewContext *vc,
                            void (*func)(void *user_data,
                                         bPoseChannel *pchan,
                                         const float screen_co_a[2],
                                         const float screen_co_b[2]),
                            void *user_data,
                            const eV3DProjTest clip_flag)
{
  /* Almost _exact_ copy of #armature_foreachScreenBone */

  const Object *ob_eval = DEG_get_evaluated(vc->depsgraph, vc->obact);
  const bArmature *arm_eval = static_cast<const bArmature *>(ob_eval->data);
  bPose *pose = vc->obact->pose;

  ED_view3d_check_mats_rv3d(vc->rv3d);

  float content_planes[6][4];
  int content_planes_len;
  rctf win_rect;

  if (clip_flag & V3D_PROJ_TEST_CLIP_CONTENT) {
    content_planes_len = content_planes_from_clip_flag(
        vc->region, ob_eval, clip_flag, content_planes);
    win_rect.xmin = 0;
    win_rect.ymin = 0;
    win_rect.xmax = vc->region->winx;
    win_rect.ymax = vc->region->winy;
  }
  else {
    content_planes_len = 0;
  }

  LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) {
    if (!blender::animrig::bone_is_visible(arm_eval, pchan)) {
      continue;
    }

    bPoseChannel *pchan_eval = BKE_pose_channel_find_name(ob_eval->pose, pchan->name);
    float screen_co_a[2], screen_co_b[2];
    const float *v_a = pchan_eval->pose_head, *v_b = pchan_eval->pose_tail;

    if (clip_flag & V3D_PROJ_TEST_CLIP_CONTENT) {
      if (!view3d_project_segment_to_screen_with_content_clip_planes(vc->region,
                                                                     v_a,
                                                                     v_b,
                                                                     clip_flag,
                                                                     &win_rect,
                                                                     content_planes,
                                                                     content_planes_len,
                                                                     screen_co_a,
                                                                     screen_co_b))
      {
        continue;
      }
    }
    else {
      if (!view3d_project_segment_to_screen_with_clip_tag(
              vc->region, v_a, v_b, clip_flag, screen_co_a, screen_co_b))
      {
        continue;
      }
    }

    func(user_data, pchan, screen_co_a, screen_co_b);
  }
}

/** \} */
