import bpy

print("--- LOADING SMART MATERIAL OVERRIDE MODULE ---")

class MY_OT_smart_material_override(bpy.types.Operator):
    """Smart override for materials (handles nested libraries)"""
    bl_idname = "outliner.smart_material_override"
    bl_label = "Make Library Override (Smart)"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        # Check if we are in outliner and have selected ID(s)
        if context.area.type != 'OUTLINER':
            return False
        
        # Ensure that selected_ids exists and contains at least one linked material
        if not context.selected_ids:
            return False
        
        # Check if any selected ID is a linked material
        for id_data in context.selected_ids:
            if isinstance(id_data, bpy.types.Material) and id_data.library:
                return True
        
        return False


    def execute(self, context):
        selected_ids = context.selected_ids
        
        overridden_count = 0
        
        for id_data in selected_ids:
            if isinstance(id_data, bpy.types.Material):
                if id_data.library: # Only linked materials
                    print(f"Smart Override: Processing linked material {id_data.name}")
                    try:
                        # This calls the patched C++ API which handles parent detection and root setting
                        new_override = id_data.override_create(remap_local_usages=True)
                        
                        if new_override:
                            print(f"  -> Success: {new_override.name}")
                            overridden_count += 1
                        else:
                            self.report({'WARNING'}, f"Failed to override {id_data.name}")
                            
                    except Exception as e:
                        self.report({'ERROR'}, f"Error overriding {id_data.name}: {str(e)}")
                else:
                    self.report({'INFO'}, f"Skipping local material {id_data.name}")
            
            # For objects, standard create is fine, but we focus on materials for this operator
            # elif isinstance(id_data, bpy.types.Object) and id_data.library:
            #      print(f"Smart Override: Processing linked object {id_data.name}")
            #      id_data.override_create(remap_local_usages=True)
            #      overridden_count += 1


        if overridden_count > 0:
            self.report({'INFO'}, f"Created {overridden_count} overrides")
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "No suitable linked materials selected or processed")
            return {'CANCELLED'}

classes = (
    MY_OT_smart_material_override,
)
