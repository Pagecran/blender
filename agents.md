# Feature Documentation: FBX Materials Reuse

## Context
Blender 5.0 introduces a new C++ based FBX importer alongside the legacy Python importer.
This branch (`feature/5.0/fbx-materials`) implements a "Reuse Materials" feature to avoid duplicating materials when importing FBX files.

## Current Implementation Status
- **Legacy Importer (Python)**: The "Reuse Materials" feature is fully implemented via this branch.
- **New Importer (C++)**: The feature is **ALREADY IMPLEMENTED** natively (Option: *Materials > Material Name > Reference Existing*).

## Future Considerations
- This branch serves as a transition for users still relying on the legacy Python importer.
- Once the legacy importer is deprecated or removed, this custom feature will become obsolete.