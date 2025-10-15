## Library Override Push-Back

This feature integrates into Blender's existing library override workflow through three key components:

**1. Python Operator (scripts/startup/bl_operators/object.py)**
- Adds the `OBJECT_OT_library_override_pushback` operator (lines 993-1254).
- Supports all ID types via `context.selected_ids` and `context.id` (Outliner).
- Provides a fallback for `context.selected_objects` (3D view).
- Collects override properties while skipping unsafe pointer-based overrides.
- Resolves type names across Blender's inconsistent ID naming.
- Converts `mathutils` values to tuples for serialization.
- Creates timestamped backups before modification.
- Restores the backup automatically on failure.
- Optionally removes overrides after a successful push.
- Calls the `bpy.data.libraries.modify_external()` C++ API to patch the linked blend file.

**2. Outliner Menu Integration (scripts/startup/bl_ui/space_outliner.py)**
- Adds a "Push Back" entry to `OUTLINER_MT_liboverride` (lines 396-398).
- Places the entry between "Clear" and "Troubleshoot" to match the workflow.
- Uses `outliner.liboverride_operation` with `OVERRIDE_LIBRARY_PUSH_BACK`.
- Respects existing menu conventions (`selection_set = 'SELECTED'`).

**3. C++ Integration (source/blender/editors/space_outliner/outliner_tools.cc)**
- Adds `OUTLINER_LIBOVERRIDE_OP_PUSH_BACK` to the enum (line 1811).
- Declares the enum property with documentation (lines 1811-1815).
- Implements the executor case (lines 2007-2014).
- Invokes the Python operator through `WM_operator_name_call` with undo depth management.
- Triggers the necessary notifiers so the UI updates immediately.

### Key Features
- Works with all ID types (Objects, Materials, Collections, Armatures, etc.).
- Handles multi-selection via the Outliner and the 3D viewport.
- Creates automatic backups with optional retention.
- Filters unsafe properties to avoid corrupt overrides.
- Provides robust error handling with detailed user reports.
- Reloads affected libraries after modification.
- Fully supports undo/redo.

### Primary Use Case
Artists can modify library-linked assets through overrides and push those changes back to the source library, letting multiple .blend files share the updated data with minimal manual work.

---

## Current Scope (after reverting 66f5609)

1. **Push-back for volatile material overrides**
   - Session-only overrides (e.g. materials edited but not saved) keep their override operations in memory.
   - Ensure node sockets that should persist (float/vector/color default values) are flagged with `PROPOVERRIDE_OVERRIDABLE_LIBRARY` in `source/blender/makesrna/intern/rna_node_socket.cc` and the matching interface helpers.
   - Refresh override operations inside `OBJECT_OT_library_override_pushback.execute()` by calling `override_library.operations_update(id_override)` before `bpy.data.libraries.modify_external()` is invoked.
   - Continue converting `mathutils` values to tuples before serialization so the push-back payload is JSON-safe.
   - Validation path: edit a Principled BSDF color on an override, push back headlessly, reopen the source library and assert the value changed.

2. **Level-2 material overrides stay locked**
   - Scenario: a level-2 library (matlib) material assigned to a level-1 library (testwitmatlib) mesh. The override appears in the Outliner but `override_library` stays `None`, so the UI remains locked.
   - Cause: `id_override_library_create_hierarchy_pre_process()` returns as soon as it finds a parent anchored in another library, skipping the creation of a local anchor.
   - Fix outline (all in `source/blender/editors/space_outliner/outliner_tools.cc`):
     * When climbing parents, enqueue linked parents from other libraries through `OutlinerLibOverrideData::id_root_add` instead of returning.
     * In `id_override_library_create_hierarchy()` / `id_override_library_create_hierarchy_process()`, create those parent overrides first with `BKE_lib_override_library_create()`, clear `LIBOVERRIDE_FLAG_SYSTEM_DEFINED`, then process the children (the level-2 material).
     * Confirm no stray linked IDs remain instanced once the override exists.
   - Once `override_library` is populated for the material, apply the push-back pipeline from task 1.

---

## Known Gaps (current base)
- Collections nested inside linked collections occasionally attach at the wrong location after override creation; keep in mind while testing but still out of scope for the immediate fix.
- Automate the headless regression scripts once the collection issue above is addressed (current validation is manual via the dedicated .blend repros).

---

## Updated Development Plan

### Phase 0 - Preparation
- Maintain two reference .blend files focused on the two problem areas:
  1. `tests/liboverride_single_level.blend` - one linked object with a linked material (active node tree) from a single-level library; no lights needed.
  2. `tests/liboverride_nested_material.blend` - source collection at level 1 that instantiates a level-2 library containing the material we want to override (material belongs to lib L2, mesh to lib L1).
- Provide a Python validation script that:
  - Forces overrides for each relevant ID and RNA path.
  - Logs `ID.override_library` and the collected property operations.
  - Calls `bpy.ops.object.library_override_pushback()` and checks that the linked library records the changes.

### Phase 1 - Push-back pipeline for volatile materials (Python/RNA)
1. `source/blender/makesrna/intern/rna_node_socket.cc` (and interface helpers)
   - ✅ `default_value` for float/int/vector/color sockets – plus their interface variants – now carry `PROPOVERRIDE_OVERRIDABLE_LIBRARY`, so material node defaults participate in push-back even when the override is still volatile.
   - ✅ Principled BSDF sockets register override operations correctly after the refresh.
2. `scripts/startup/bl_operators/object.py`
   - ✅ `_collect_override_ids` is shared between `poll()` and `execute()` and keeps material overrides in the batch even when their operations are still in-memory.
   - ✅ `override_library.operations_update(id_override)` is called on-demand before serialisation, preserving volatile edits for push-back.
   - ✅ mathutils-to-tuple conversion and grouped reporting remain in place (error messages now include the failing ID).
3. Optional utility: script to reload the library and compare the updated values for automated verification. *(still optional)*.

### Phase 2 - Level-2 material override unlock (C++)
1. `source/blender/editors/space_outliner/outliner_tools.cc`
   - ✅ Cross-library parents discovered during the hierarchy walk are now enqueued instead of aborting, and depth sorting ensures anchors are created before their children.
2. `id_override_library_create_hierarchy(...)` / `id_override_library_create_hierarchy_process(...)`
   - ✅ Root batches are sorted by depth, and parent overrides are created first (system flags cleared) before processing the child ID (e.g. the level-2 material).
   - ✅ Instance empties are still removed only when appropriate; no extra duplicates are left behind.
3. Validation
   - ⏳ `Material.override_hierarchy_create()` still returns `None` in the nested scenario; the Outliner path is functional but the direct API remains to be wired through the new anchor scheduling.
   - ⏳ `Material.override_create()` continues to return `None`; it will pick up the nested support once the direct path above is completed.

### Phase 3 - Tests
1. Rebuild via `./make.bat release`.
2. Manual scenarios:
   - Single-level: override a material (change Principled color), push back, reload the library, confirm persistence.
   - Nested scenario: override the level-1 mesh and the level-2 material; ensure the material unlocks, accepts edits, and the edits survive push-back.
   - Regression: verify single-level overrides still behave after the hierarchy change.
3. Automation: extend `tools/liboverride_tests/run_pushback_validation.py` (or add a new script) to cover both scenarios headlessly once the hierarchy fix lands.
   - 2025-10-15: Headless smoke-test still shows `override_create` / `override_hierarchy_create` returning `None` on the nested material; automated coverage will be updated once the direct API is aligned with the Outliner implementation.

### Phase 5 - Finalisation
- Remove temporary instrumentation/logging.
- Update this document with the final state and testing evidence.
- Prepare the final patch (code + tests + documentation).
