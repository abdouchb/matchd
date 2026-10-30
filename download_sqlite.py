import urllib.request
import zipfile
import ssl
import os
import shutil

url = "https://www.sqlite.org/2023/sqlite-amalgamation-3430000.zip"
zip_path = "sqlite.zip"
extract_dir = "libs/sqlite_temp"
target_dir = "libs/sqlite"

print("Downloading SQLite...")
ctx = ssl.create_default_context()
ctx.check_hostname = False
ctx.verify_mode = ssl.CERT_NONE

with urllib.request.urlopen(url, context=ctx) as response, open(zip_path, 'wb') as out_file:
    shutil.copyfileobj(response, out_file)

print("Extracting...")
with zipfile.ZipFile(zip_path, 'r') as zip_ref:
    zip_ref.extractall(extract_dir)

print("Moving files...")
if not os.path.exists(target_dir):
    os.makedirs(target_dir)

src_folder = os.path.join(extract_dir, "sqlite-amalgamation-3430000")
for file_name in os.listdir(src_folder):
    shutil.move(os.path.join(src_folder, file_name), os.path.join(target_dir, file_name))

print("Cleaning up...")
os.remove(zip_path)
shutil.rmtree(extract_dir)
print("Done!")
