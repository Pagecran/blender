# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import unittest

import bpy


class TestMeshSeparate(unittest.TestCase):

    def setUp(self):
        if bpy.ops.object.mode_set.poll():
            bpy.ops.object.mode_set(mode='OBJECT', toggle=False)
        bpy.ops.object.select_all(action='SELECT')
        bpy.ops.object.delete()

    def _create_two_material_mesh(self, name, link):
        mat_1 = bpy.data.materials.new(name + '_mat_1')
        mat_2 = bpy.data.materials.new(name + '_mat_2')

        mesh = bpy.data.meshes.new(name + '_mesh')
        mesh.from_pydata(
            [(-1, 0, 0), (0, 0, 0), (-1, 1, 0), (0, 1, 0), (1, 0, 0), (1, 1, 0)],
            [],
            [(0, 1, 3, 2), (1, 4, 5, 3)],
        )
        mesh.materials.append(mat_1)
        mesh.materials.append(mat_2)
        mesh.polygons[0].material_index = 0
        mesh.polygons[1].material_index = 1

        obj = bpy.data.objects.new(name, mesh)
        bpy.context.collection.objects.link(obj)
        bpy.context.view_layer.objects.active = obj
        obj.select_set(True)

        if link == 'OBJECT':
            for slot, material in zip(obj.material_slots, (mat_1, mat_2)):
                slot.link = 'OBJECT'
                slot.material = material

        return obj, (mat_1, mat_2)

    def _separate_by_material(self, obj):
        bpy.context.view_layer.objects.active = obj
        obj.select_set(True)
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.mesh.separate(type='MATERIAL')
        bpy.ops.object.mode_set(mode='OBJECT')
        return sorted(bpy.context.selected_objects, key=lambda item: item.name)

    def _assert_separated_materials(self, objects, materials, link):
        self.assertEqual(len(objects), 2)
        seen_materials = []
        for obj in objects:
            self.assertEqual(len(obj.material_slots), 1)
            self.assertEqual(obj.material_slots[0].link, link)
            self.assertEqual(len(obj.data.polygons), 1)
            self.assertEqual(obj.data.polygons[0].material_index, 0)
            seen_materials.append(obj.material_slots[0].material)

        self.assertEqual(set(seen_materials), set(materials))

    def test_separate_by_material_preserves_object_linked_materials(self):
        obj, materials = self._create_two_material_mesh('object_linked', 'OBJECT')

        objects = self._separate_by_material(obj)

        self._assert_separated_materials(objects, materials, 'OBJECT')

    def test_separate_by_material_preserves_data_linked_materials(self):
        obj, materials = self._create_two_material_mesh('data_linked', 'DATA')

        self.assertTrue(all(slot.link == 'DATA' for slot in obj.material_slots))

        objects = self._separate_by_material(obj)

        self._assert_separated_materials(objects, materials, 'DATA')


if __name__ == '__main__':
    unittest.main()
