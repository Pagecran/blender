# Evidence Summary

Task: `TASK-2026-05-28-002-fix-separate-by-material-object-linked-materials`

## What Changed

- Updated material-based mesh separation so duplicated material slots preserve the source slot link type before per-material slot condensation.
- Condensed separated objects now keep the effective material on either the object slot or mesh-data slot according to the original slot link.
- Added `tests/python/mesh_separate.py` and registered it in `tests/python/CMakeLists.txt` to cover `bpy.ops.mesh.separate(type='MATERIAL')` for object-linked and data-linked material slots.
- Incremented `BLENDER_VERSION_BUILD` from `3` to `4`; the only located build-number process documentation is the in-code Pagecran build marker comment in `source/blender/blenkernel/BKE_blender_version.h` plus CMake parsing references.

## Verification

- `logs/python-py-compile.log`: `python -m py_compile tests/python/mesh_separate.py` completed successfully.
- `logs/git-diff-check.log`: `git diff --check` completed successfully; it reported existing line-ending normalization warnings only.
- `logs/blender-runtime-availability.log`: no `blender.exe` or configured build directory was found, so the runtime Blender operator regression test could not be executed in this checkout.
- `nomadworks_validate` was run and failed on pre-existing repository CodeMap coverage/link issues unrelated to this implementation.

## Screenshots

No screenshots were produced because this is a non-UI logic fix and no local Blender runtime was available.
