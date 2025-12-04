## Library Override Push-Back

This feature integrates into Blender's existing library override workflow through four key components:

**1. Python API: modify_external (source/blender/python/intern/bpy_library_write.cc)**
- Adds `bpy.data.libraries.modify_external()` method (lines 47-306).
- Opens external .blend files, modifies data-blocks via RNA paths, and saves them.
- Supports ALL RNA types (bool, int, float, string, enum, pointer, collection).
- Handles complex paths (dots, indices, collection access) via `RNA_path_resolve_full_maybe_null`.
- Uses existing `pyrna_py_to_prop`/`pyrna_py_to_array_index` helpers for conversion.
- Optionally creates timestamped backups before modification.

**2. Python Operator (scripts/startup/bl_operators/object.py)**
- Adds the `OBJECT_OT_library_override_pushback` operator (lines 1007-1313).
- Supports all ID types via `context.selected_ids` and `context.id` (Outliner).
- Provides a fallback for `context.selected_objects` (3D view).
- Collects override properties while skipping unsafe pointer-based overrides.
- Resolves type names across Blender's inconsistent ID naming (caches type variants).
- Converts `mathutils` values to tuples for serialization.
- Creates timestamped backups before modification.
- Restores the backup automatically on failure.
- Optionally removes overrides after a successful push.
- Provides detailed statistics (pushed/skipped/cleared counts, ID types summary).
- Calls the `bpy.data.libraries.modify_external()` C++ API to patch the linked blend file.

**3. Outliner Menu Integration (scripts/startup/bl_ui/space_outliner.py)**
- Adds a "Push Back" entry to `OUTLINER_MT_liboverride` (lines 396-398).
- Places the entry between "Clear" and "Troubleshoot" to match the workflow.
- Uses `outliner.liboverride_operation` with `OVERRIDE_LIBRARY_PUSH_BACK`.
- Respects existing menu conventions (`selection_set = 'SELECTED'`).

**4. C++ Integration (source/blender/editors/space_outliner/outliner_tools.cc)**
- Adds `OUTLINER_LIBOVERRIDE_OP_PUSH_BACK` to the enum (line 1792).
- Declares the enum property with documentation (lines 1813-1817).
- Implements the executor case (lines 2009-2015).
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

### Commits (since v4.5.0)
1. `dae0da59548` - Python API: Add modify_external method for external blend file editing
2. `abc3e1183b0` - Add Library Override Push Back functionality to Outliner
3. `3525aefabd5` - Optimize push-back operator: cache type variants, add detailed statistics
4. `e5f13813457` - feat: Add CMake configuration for a lite Blender build (build helper)

### Build Configuration
- `build_files/cmake/config/blender_lite.cmake` - Lite build preset for faster iteration.
- `build_files/windows/build_msbuild.cmd` - Windows MSBuild helper script.

### Testing Notes
This branch does NOT include:
- Material override persistence across saves
- Nested/embedded material overrides

Those features are on `feature/5.0/material-overrides`. A future integration branch will merge both features for complete end-to-end testing.

### Test Procedure
1. Build Blender (lite config recommended for faster builds).
2. Create a library .blend file with some objects/materials.
3. Link objects into a new .blend file.
4. Create library overrides on the linked objects.
5. Modify properties (location, rotation, material colors, etc.).
6. In Outliner, right-click the override > Library Override > Push Back.
7. Verify the source library file was updated (open it separately to confirm).
