import bpy
import sys
import os

file_path = r"X:\_Github\blender\.blend\lvl1_assigned_lv2.blend"
output_path = r"X:\_Github\blender\.blend\lvl1_assigned_lv2_fixed.blend"

print(f"--- DEBUG: Opening {file_path} ---")
try:
    bpy.ops.wm.open_mainfile(filepath=file_path)
except Exception as e:
    print(f"ERROR: Could not open file: {e}")
    sys.exit(1)

def override_and_check(obj_name, mat_name):
    print(f"\n--- Processing Object '{obj_name}' (Material '{mat_name}') ---")
    obj = bpy.data.objects.get(obj_name)
    if not obj:
        print(f"ERROR: Object '{obj_name}' not found")
        return False

    if obj.override_library:
        print(f"Object '{obj_name}' is ALREADY an override.")
    elif obj.library:
        print(f"Object '{obj_name}' is LINKED. Creating Override...")
        try:
            obj = obj.override_create(remap_local_usages=True)
            print(f"SUCCESS: Object Override created: {obj.name}")
            # Ensure object is NOT system override too (good practice)
            obj.override_library.is_system_override = False
        except Exception as e:
            print(f"EXCEPTION creating object override: {e}")
            return False
    else:
        print(f"Object '{obj_name}' is LOCAL.")

    for slot in obj.material_slots:
        mat = slot.material
        if mat and mat_name in mat.name:
            found_mat = True
            print(f"Found material slot: {mat.name} (Lib: {mat.library}, Override: {mat.override_library})")
            
            if mat.override_library:
                print("   -> Material is ALREADY overridden.")
            elif mat.library:
                print("   -> Material is LINKED. Attempting Override...")
                try:
                    new_mat = mat.override_create(remap_local_usages=True)
                    if new_mat:
                        print(f"   -> SUCCESS: Material Override created: {new_mat.name}")
                        
                        # 1. Assign
                        slot.material = new_mat
                        print("   -> Assigned.")
                        
                        # 2. FIX ROOT
                        try:
                            new_mat.override_library.hierarchy_root = obj
                            print("   -> Root SET successfully.")
                        except Exception as e:
                            print(f"   -> FAILED to set root: {e}")
                        
                        # 3. PROTECT from Resync Deletion
                        new_mat.override_library.is_system_override = False
                        print("   -> Protected (System Override = False).")
                        
                    else:
                        print("   -> FAILURE: override_create returned None")
                         
                except Exception as e:
                    print(f"   -> EXCEPTION creating material override: {e}")
    
    return True

override_and_check("Torus", "cyan")
override_and_check("Sphero", "red")

print(f"\n--- Saving to {output_path} ---")
for obj_name in ["Torus", "Sphero"]:
    obj = bpy.data.objects.get(obj_name)
    if obj and obj.override_library:
        for slot in obj.material_slots:
            mat = slot.material
            if mat and mat.override_library:
                print(f"DEBUG: Before Save - Material '{mat.name}' (Ref: {mat.override_library.reference.name if mat.override_library.reference else 'None'}) hierarchy_root: {mat.override_library.hierarchy_root.name if mat.override_library.hierarchy_root else 'None'}")
try:
    bpy.ops.wm.save_as_mainfile(filepath=output_path)
    print("Saved.")
except Exception as e:
    print(f"Error saving: {e}")

print("\n--- DEBUG FINISHED ---")
