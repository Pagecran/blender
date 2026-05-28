<!--
Keep this document short & concise,
linking to external resources instead of including content in-line.
See 'release/text/readme.html' for the end user read-me.
-->

Pagecran Blender Fork
=====================

Blender is the free and open source 3D creation suite.
It supports the entirety of the 3D pipeline—modeling, rigging, animation, simulation, rendering, compositing,
motion tracking and video editing.

This repository is the Pagecran fork of Blender, based on upstream Blender `v5.1.1` and maintained on the
`Pagec5.1.1` branch. The current Pagecran build version is `5.1.1.4`: Blender `5.1.1` plus Pagecran build
number `4` as the fourth version component.

![Blender screenshot](https://code.blender.org/wp-content/uploads/2018/12/springrg.jpg "Blender screenshot")

Pagecran Changes
----------------

The fork currently adds these changes on top of Blender `v5.1.1`:

- Material override operators for library materials from the material menu.
- Library Override Push Back support in the Outliner.
- Pagecran build-number support through `BLENDER_VERSION_BUILD` for Windows executable versioning.
- Cycles per-pass denoising support for light passes.
- Select Through support ported to edit and object modes.
- Outliner defaults adjusted so filter toggles are enabled by default.
- Hidden objects are treated as non-renderable.
- A dedicated tool window editor for modeless panels.
- Camera aim target navigation and rig support.
- NLA track mute state stored per view layer.
- USD material export naming conventions that use an `mtl` folder and SG/shader naming.
- Separate by Material preserves object-linked material slots when separating meshes.

Pagecran Build Number Procedure
-------------------------------

When incrementing the Pagecran build number:

1. Update `BLENDER_VERSION_BUILD` in `source/blender/blenkernel/BKE_blender_version.h` to the next integer.
2. Treat that integer as the fourth version component. For example, Blender `5.1.1` with
   `BLENDER_VERSION_BUILD 4` is documented as `5.1.1.4`.
3. Commit only that build-number change separately with subject `Update BKE_blender_version.h` and commit body
   `build <N>`, replacing `<N>` with the new build number.

Project Pages
-------------

- [Main Website](http://www.blender.org)
- [Reference Manual](https://docs.blender.org/manual/en/latest/index.html)
- [User Community](https://www.blender.org/community/)

Development
-----------

- [Build Instructions](https://developer.blender.org/docs/handbook/building_blender/)
- [Code Review & Bug Tracker](https://projects.blender.org)
- [Developer Forum](https://devtalk.blender.org)
- [Developer Documentation](https://developer.blender.org/docs/)


License
-------

Blender as a whole is licensed under the GNU General Public License, Version 3.
Individual files may have a different but compatible license.

See [blender.org/about/license](https://www.blender.org/about/license) for details.
