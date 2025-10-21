Object & Material Override Design
=================================

Context
-------
- Even in simple hierarchies, overrides lack persistent property operations (the Library Override editor stays empty and push-back cannot find overriden properties).
- Outliner logic that discovers owners and schedules override creation lives entirely in `outliner_tools.cc`.
- Direct APIs (`ID.override_*`, scripts) do not reproduce that logic, leading to missing anchors, duplicated objects, or overrides flagged as system-defined.

Goals
-----
- Capture the override-creation algorithm in a reusable design so both Outliner and scripted flows can use the same behaviour.
- Ensure overrides can be created for any combination of local, single-library, or cross-library relationships without duplication or loss of hierarchy.
- Guarantee that resulting overrides are user-editable and retain proper owner links, including materials, meshes, node trees, and collections.

Prerequisites & Boundaries
--------------------------
- Baseline: single-level overrides must produce discoverable property operations (Library Override editor entries + push-back inputs). Missing operations are tracked separately but must be resolved before this design can succeed.
- Out of scope for this document: push-back serialisation code paths (Python operator), known collection instancing bugs, and permanent storage of volatile edits (handled via `operations_update` refresh).

Current Pain Points
-------------------
- Owner discovery is incomplete: nested materials and meshes lose their parent anchor.
- Override creation relies on `BKE_lib_override_library_create` with arbitrary `nullptr` scene/view-layer, causing crashes.
- Fallbacks (`BKE_lib_override_library_create_from_id`) do not attach the new override to a hierarchy root, so overrides remain “system” and uneditable.
- Post-processing fails when owners remain linked (e.g. material owner is still the imported object), so properties never appear in the Library Override editor.

Design Overview
---------------
1. **Owner Chain Discovery**
   - For any selected ID, enumerate upstream owners and collections.
   - Prefer owners from the same library; fall back to locals when needed.
   - Cache existing overrides to avoid duplicating anchors.
2. **Hierarchy Scheduling**
   - Build `OutlinerLibOverrideDataIDRoot` entries sorted by depth.
   - Each entry carries: reference ID, existing override (if any), owner references, collection owners, and library pointer.
3. **Override Creation Pipeline**
   - If an existing override is available, reuse it and ensure the system flag is cleared.
   - Otherwise call `BKE_lib_override_library_create` with valid `scene` / `view_layer`.
   - On failure due to cross-library parents, fall back to `BKE_lib_override_library_create_from_id`, then attach the resulting override to a parent anchor.
4. **Hierarchy Root Resolution**
   - When creating or reusing a hierarchy root, set `override_library->hierarchy_root` appropriately.
   - For nested cases, reuse or create parent overrides before processing children.
5. **Post Processing**
   - Remap owner pointers (object → mesh, mesh → material, material → node tree, collection → objects).
   - For owners that remain linked (e.g. local scene object), allow direct relink without requiring another override.
   - Update dependency graph tags (`ID_RECALC_*`) based on ownership type.

Key Data Structures
-------------------
- `OutlinerLibOverrideNestedParentInfo`
  - Add `Library *library` and `ID *existing_override` to track owners and cached overrides.
- `OutlinerLiboverrideDataIDRoot`
  - Add `Library *reference_library` and `ID *existing_override`.
- `OutlinerLibOverrideData::id_root_add`
  - Accept existing override argument; log library/override for debug.

Algorithm Steps
---------------
1. **Collect Selection**
   - Traverse `TreeElement` parents; mark ID_TAG_DOIT on overridable IDs.
   - Record nested parents with their library, depth, and existing overrides.
2. **Augment Hierarchy**
   - For materials, acquire owner object/mesh/collection via `lib_override_find_material_owner`.
   - For meshes, gather owning object and collection via `lib_override_find_mesh_owner`.
   - Append owners to hierarchy entries if not already registered.
3. **Register Entries**
   - Call `id_root_add` with new metadata (library, existing override).
   - When a node tree is linked and non-embedded, queue it as child entry.
4. **Process Hierarchy Roots**
   - Sort roots by minimal depth.
   - For each root:
     - If reference is linked, search for existing override; create one if missing (clearing system flag).
     - Pass resulting override as new root for child processing.
5. **Create Overrides**
   - For each entry:
     - Reset ID tags/new pointers.
     - If existing override provided, reuse and clear system flag.
     - Else call `BKE_lib_override_library_create`.
       - On failure (e.g. mixed libraries) fall back to `BKE_lib_override_library_create_from_id`.
     - Store override in `created_overrides` map.
     - For materials, call `lib_override_ensure_material_nodetree_override` to ensure node tree is local and flagged correctly.
6. **Post Process**
   - For each created override, determine owner override (or local owner).
   - Remap owner references via `BKE_libblock_relink_ex`.
   - Update dependency graph tags.
   - If owner remains linked but local override exists elsewhere, search fallback map.
7. **Cleanup**
   - Clear `created_overrides`.
   - Remove instancing empties only when legitimate overrides exist.
   - Report success/failure per hierarchy root.

Implementation Plan
-------------------
Phase 1 – Data Struct Prep
- Extend `OutlinerLibOverrideNestedParentInfo` and `OutlinerLiboverrideDataIDRoot` with library/override slots.
- Propagate new arguments through `id_root_add` and its call sites.

Phase 2 – Owner Discovery Refinement
- Update material and mesh owner helpers to prefer same-library owners while still reporting local fallbacks.
- Record collection owners even if they're local so overrides have anchors.

Phase 3 – Hierarchy Registration
- When scanning nested parents, capture existing overrides via `lib_override_find_existing_override`.
- When appending material node trees, include library and existing override.

Phase 4 – Override Creation Loop
- Insert reuse path: if `data_idroot.existing_override` is set, reuse and clear system flag.
- Wrap fallback to `BKE_lib_override_library_create_from_id` and ensure hierarchy root is set.
- Maintain valid `scene` / `view_layer` parameters (never pass `nullptr`).

Phase 5 – Post Process Enhancements
- Allow local owners to serve as anchor when no override exists.
- Add detailed logging for fallback owner resolution.

Validation Plan
---------------
- **Manual Tests**
  - Level-1: override material, confirm icon, editability, push-back.
  - Nested: override L1 mesh + L2 material; ensure no duplication and edits are unlocked.
  - Cross-library: mix local object with linked material; ensure anchors created.
- **Headless Scripts**
  - Update existing Python helpers (`__tmp_nested_*`) to assert owner override assignments.
  - Add regression script for `ID.override_hierarchy_create()` verifying the API matches Outliner results.
- **Instrumentation**
  - Enable `liboverride` logs during development to trace reuse and fallback paths.
- **Future Automation**
  - Integrate scripts into `tools/liboverride_tests/run_pushback_validation.py` once collection-placement issue is resolved.

Deliverables
------------
- `outliner_tools.cc` updates covering data structures, owner discovery, creation loop, and post-process.
- Optional helper wrappers documented (but not yet promoted) for potential C API unification once behaviour stabilises.
- Updated design document (this file) guiding ongoing development and code review.
