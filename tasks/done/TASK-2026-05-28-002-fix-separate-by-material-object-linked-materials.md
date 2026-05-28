---
task_id: TASK-2026-05-28-002
title: Fix Separate by Material object-linked material preservation and increment build
complexity: standard
track: implementation
slice: logic
status: done
discussion: DISCUSSION-001
issue_url: https://github.com/Pagecran/blender/issues/42#issue-4524674835
target_branch: Pagec5.1.1
---

# Task: Fix Separate by Material object-linked material preservation and increment build

## Request

Implement the bug fix requested by the Product Owner for GitHub issue #42, commit it on branch `Pagec5.1.1`, push it to the Pagecran repository, and increment the documented build number according to the repository's documented process.

Original user request: "est-ce que tu peux faire l'implmentation et pouser le comit dans Pagec 5.1.1 et incrementer le numero de build stp (c documenté normalement)"

## Background

Investigation task `TASK-2026-05-28-001` found that Blender's **Separate by Material** command maps to `bpy.ops.mesh.separate(type='MATERIAL')`, implemented primarily in `source/blender/editors/mesh/editmesh_tools.cc`.

The suspected failure path is:

- `mesh_separate_material()` groups faces by `BMFace.mat_nr`.
- `mesh_separate_tagged()` creates separated objects and copies material arrays.
- `mesh_separate_material_assign_mat_nr()` condenses material slots for each separated object.
- Object-linked material slots (`ob->mat` / `ob->matbits`) are not preserved correctly when new objects are created/condensed from mesh-data material arrays.

## Acceptance Criteria

- AC-1: `bpy.ops.mesh.separate(type='MATERIAL')` preserves separated object material slots when source materials are linked to the Object rather than Mesh Data.
- AC-2: Existing data-linked material behavior remains unchanged.
- AC-3: A regression test or executable validation script covers object-linked materials and data-linked materials for Separate by Material.
- AC-4: The repository's documented build number/version marker is incremented according to the documented Pagec/Blender process.
- AC-5: Evidence packet is created under `evidences/TASK-2026-05-28-002-fix-separate-by-material-object-linked-materials/` with `SUMMARY.md`, verification logs, and screenshots only if a UI/visual proof is practical.
- AC-6: Final implementation is committed on `Pagec5.1.1` and pushed to the corresponding remote branch, after PMA approval and technical verification.

## Constraints

- Do not implement unrelated behavior.
- Keep the fix narrowly scoped to material preservation during material-based separation.
- Avoid committing unrelated untracked files unless they are required workflow/evidence artifacts.
- 100% pass rate is required for all verification commands executed for this task.

## Discussion Record

- 2026-05-28: User requested implementation, commit/push to `Pagec5.1.1`, and build number increment.

## Expected Handoffs

1. Technical Architect / Tech Lead: confirm implementation approach and version/build-number location.
2. Developer: implement fix, tests/validation, evidence packet, and task file post-implementation updates.
3. QA / Tech Lead: verify evidence and code behavior.
4. Tech Lead: commit and push when PMA authorizes final closure.

# Post Implementation Task Updates

## Developer: Post Implementation Expectations

- Material-based separation preserves object-linked material slots by copying source slot link/material state before condensing each separated object to a single material slot.
- Data-linked material separation remains covered and expected to keep data-linked slot behavior unchanged.
- Regression coverage is added in `tests/python/mesh_separate.py` and registered as the `mesh_separate` Blender Python test.
- `BLENDER_VERSION_BUILD` is incremented from `3` to `4`; only in-code Pagecran build-marker documentation was found.
- Evidence is available under `evidences/TASK-2026-05-28-002-fix-separate-by-material-object-linked-materials/`.
- Runtime Blender test execution still needs QA/Tech Lead verification in a configured build because no `blender.exe` was present in this checkout.

## PMA Closure Notes

- QA and Tech Lead approved static/Python-level verification.
- PMA accepted deferred runtime Blender verification because this checkout has no built Blender runtime and all feasible targeted checks passed.
- Final commit/push delegated to Tech Lead on `Pagec5.1.1`.
