import bpy
import sys
import os

file_path = r"X:\\_Github\blender\.blend\lvl1_assigned_lv2_fixed.blend"

print(f"--- VERIFY: Opening {file_path} ---")
if not os.path.exists(file_path):
    print("ERROR: File does not exist!")
    sys.exit(1)

try:
    bpy.ops.wm.open_mainfile(filepath=file_path)
except Exception as e:
    print(f"ERROR: Could not open file: {e}")
    sys.exit(1)

def check_object(obj_name):
    print(f"\n--- Checking Object '{obj_name}' ---")
    obj = bpy.data.objects.get(obj_name)
    if not obj:
        print(f"ERROR: Object '{obj_name}' not found")
        return

    print(f"Object Library: {obj.library} (Override: {bool(obj.override_library)})")
    
    for slot in obj.material_slots:
        mat = slot.material
        if mat:
            print(f" - Material: {mat.name}")
            print(f"   - Library: {mat.library}")
            print(f"   - Is Override: {bool(mat.override_library)}")
            if mat.override_library:
                root = mat.override_library.hierarchy_root
                print(f"   - Hierarchy Root: {root.name if root else 'None'}")
                print(f"   - Material's reference: {mat.override_library.reference.name if mat.override_library.reference else 'None'}")
                if root == obj:
                    print("   - STATUS: OK (Linked to parent)")
                else:
                    print("   - STATUS: BROKEN (Wrong root / Orphanned)")
            else:
                print("   - Hierarchy Root: N/A (Not an override)")
        else:
            print(" - No Material")

check_object("Torus")
check_object("Sphero")

print("\n--- VERIFY FINISHED ---")
