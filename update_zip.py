import os
import zipfile

src_dir = os.path.dirname(os.path.abspath(__file__))
zip_path = r"C:\Users\ipiou\Desktop\ZarqaaOS_Core_Source.zip"

exclude_dirs = {'.git', 'build', '.gemini'}

print("Updating zip file:", zip_path)
count = 0
with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as zipf:
    for root, dirs, files in os.walk(src_dir):
        dirs[:] = [d for d in dirs if d not in exclude_dirs]
        for file in sorted(files):
            if file.endswith('.iso') or file.endswith('.zip') or file.endswith('.o'):
                continue
            full_path = os.path.join(root, file)
            rel_path = os.path.relpath(full_path, src_dir)
            zipf.write(full_path, rel_path)
            count += 1

size_mb = os.path.getsize(zip_path) / (1024 * 1024)
print(f"Zip updated successfully: {count} files, {size_mb:.2f} MB")
