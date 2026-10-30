Mannequin Viewer - pistol / M4 / Kar98k sniper / aim trainer / VS AI
================================================
Run (a local web server is required; double-clicking index.html will NOT load the FBX files):
  Windows:     double-click start-windows.bat   (needs Python or Node.js)
  Mac/Linux:   ./start-mac-linux.sh
  Then open    http://localhost:8000/            viewer
               http://localhost:8000/mobile.html  fullscreen aim trainer (phones: use your PC's LAN IP)
Internet is needed on first load (three.js is loaded from unpkg.com).

Controls: WASD move · Shift run · Q/E turn · Space fire · 1 pistol / 2 M4 / 3 Kar98k · R reload
          Kar98k: bolt is worked after every shot; RMB in the aim game = scope
Aim game modes: Aim trainer (60 s, steel targets) or VS AI (enemy squads take cover, peek and shoot,
          flank and fall back; walls and crates block bullets both ways; survive the waves, HP regenerates)
Phones: push the left stick further to run (or toggle RUN); resolution adapts to keep the frame rate up
          Edit IK (drag handles, T move / R rotate) · Aim game: mouse aim, LMB fire, RMB zoom, Esc pause
Character: model/Player_kachabia.fbx (UE5 Manny skeleton) by default; ?model=mannequin loads the old UE4 Mannequin.FBX.
          Its textures are not embedded (the FBX points at a missing Player_kachabia.fbm folder), so neutral tones are used;
          re-export from Blender with Path Mode = Copy + Embed Textures to get the real textures.
Locomotion QA: http://localhost:8000/?test  (foot-contact slide, pelvis acceleration, knee velocity, reach metrics in the page title)
Files: index.html (all code) · model/Mannequin.FBX · anim/*.FBX (pose sources) · assets/reference.jpg
       assets/glb/m4_fps.glb (M4 + FPS hand/reload animation, Sketchfab pack) · assets/sfx/*.mp3 (sounds cut from the reference video)
تشغيل: شغّل start-windows.bat ثم افتح http://localhost:8000
