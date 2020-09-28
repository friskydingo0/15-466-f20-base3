"C:\Program Files\Blender Foundation\Blender 2.90\blender.exe" -y --background --python export-scene.py -- "%1.blend":%2 "..\dist\%1.scene"

"C:\Program Files\Blender Foundation\Blender 2.90\blender.exe" -y --background --python export-meshes.py -- "%1.blend":%2 "..\dist\%1.pnct"