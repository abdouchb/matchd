import sys, os, struct

if len(sys.argv) < 3:
    print("Usage: python convert_to_zv.py input.mp4 output.zv")
    sys.exit(1)

input_file = sys.argv[1]
output_file = sys.argv[2]

WIDTH = 160
HEIGHT = 120
FPS = 30
SAMPLE_RATE = 48000
CHANNELS = 2

# Extract raw video
print("Extracting video...")
os.system(f'ffmpeg -i "{input_file}" -f rawvideo -vcodec rawvideo -pix_fmt rgb565le -s {WIDTH}x{HEIGHT} -r {FPS} temp_vid.raw -y')

# Extract raw audio
print("Extracting audio...")
os.system(f'ffmpeg -i "{input_file}" -f s16le -acodec pcm_s16le -ar {SAMPLE_RATE} -ac {CHANNELS} temp_aud.raw -y')

with open("temp_vid.raw", "rb") as fv, open("temp_aud.raw", "rb") as fa:
    vid_data = fv.read()
    aud_data = fa.read()

frame_vid_size = WIDTH * HEIGHT * 2
total_frames = len(vid_data) // frame_vid_size

# Trim audio to match video duration
expected_aud_size = total_frames * (SAMPLE_RATE // FPS) * (CHANNELS * 2)
if len(aud_data) > expected_aud_size:
    aud_data = aud_data[:expected_aud_size]
elif len(aud_data) < expected_aud_size:
    aud_data += b'\x00' * (expected_aud_size - len(aud_data))

with open(output_file, "wb") as fout:
    # Header: 16 bytes
    fout.write(b'ZVID')
    fout.write(struct.pack('<H', WIDTH))
    fout.write(struct.pack('<H', HEIGHT))
    fout.write(struct.pack('<B', FPS))
    fout.write(struct.pack('<B', CHANNELS))
    fout.write(struct.pack('<H', SAMPLE_RATE))
    fout.write(struct.pack('<I', total_frames))
    
    fout.write(vid_data)
    fout.write(aud_data)

try:
    os.remove("temp_vid.raw")
    os.remove("temp_aud.raw")
except:
    pass

print(f"Success! Created {output_file} ({len(vid_data)} bytes video, {len(aud_data)} bytes audio)")
