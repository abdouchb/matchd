Mannequin Viewer - pistol / rifle / aim trainer
================================================
Run (a local web server is required; double-clicking index.html will NOT load the FBX files):
  Windows:     double-click start-windows.bat   (needs Python or Node.js)
  Mac/Linux:   ./start-mac-linux.sh
  Then open    http://localhost:8000/            viewer
               http://localhost:8000/mobile.html  fullscreen aim trainer (phones: use your PC's LAN IP)
Internet is needed on first load (three.js is loaded from unpkg.com).

Controls: WASD move · Shift run · Q/E turn · Space fire · 1/2 pistol/rifle · R reload
          Edit IK (drag handles, T move / R rotate) · Aim game: mouse aim, LMB fire, RMB zoom, Esc pause
Files: index.html (all code) · model/Mannequin.FBX · anim/*.FBX (pose sources) · assets/reference.jpg
تشغيل: شغّل start-windows.bat ثم افتح http://localhost:8000
