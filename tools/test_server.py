import http.server
import socketserver
import os

PORT = 8080
DIRECTORY = "build/iso_root/hd0/www" # Serve the browser's own HTML files

class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=DIRECTORY, **kwargs)

with socketserver.TCPServer(("", PORT), Handler) as httpd:
    print(f"[*] Zarqaa Test Web Server running on port {PORT}")
    print(f"[*] Serving files from {DIRECTORY}")
    print("[*] In Zarqaa Browser, open: http://10.0.2.2:8080/index.html")
    httpd.serve_forever()
